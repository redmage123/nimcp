//=============================================================================
// nimcp_transactive.c - Transactive Memory System Implementation
//=============================================================================
/**
 * @file nimcp_transactive.c
 * @brief Implementation of transactive memory for distributed expertise
 *
 * This file implements the "who knows what" memory system for tracking
 * and leveraging distributed knowledge across multiple agents.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_transactive.h"
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
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE(transactive, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-local Error Handling
//=============================================================================

#ifdef _MSC_VER
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    } else {
        g_last_error[0] = '\0';
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Hash table entry for agent lookup
 */
typedef struct agent_entry_struct {
    uint64_t agent_id;
    transactive_agent_t agent;
    struct agent_entry_struct* next;
} agent_entry_t;

/**
 * @brief Hash table entry for domain lookup
 */
typedef struct domain_entry_struct {
    prime_signature_t signature;
    char* name;
    struct domain_entry_struct* next;
} domain_entry_t;

/**
 * @brief Internal transactive memory structure
 */
struct transactive_memory_struct {
    // Configuration
    transactive_config_t config;

    // Agent registry (hash table)
    agent_entry_t** agent_table;
    size_t agent_table_size;
    size_t num_agents;

    // Domain taxonomy (hash table)
    domain_entry_t** domain_table;
    size_t domain_table_size;
    size_t num_domains;

    // Delegation tracking
    delegation_state_t* delegations;
    size_t delegation_capacity;
    size_t num_delegations;
    uint64_t next_delegation_id;

    // PR integration (optional)
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    // Statistics
    transactive_stats_t stats;
};

//=============================================================================
// Hash Functions
//=============================================================================

/**
 * @brief Hash function for agent IDs
 */
static size_t hash_agent_id(uint64_t agent_id, size_t table_size) {
    // FNV-1a inspired mixing
    uint64_t hash = 14695981039346656037ULL;
    hash ^= agent_id;
    hash *= 1099511628211ULL;
    hash ^= (agent_id >> 32);
    hash *= 1099511628211ULL;
    return (size_t)(hash % table_size);
}

/**
 * @brief Hash function for prime signatures
 */
static size_t hash_signature(const prime_signature_t* sig, size_t table_size) {
    if (!sig) return 0;
    return (size_t)(sig->hash % table_size);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find agent entry by ID
 */
static agent_entry_t* find_agent_entry(
    transactive_memory_t tm,
    uint64_t agent_id
) {
    if (!tm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tm is NULL");

        return NULL;

    }

    size_t index = hash_agent_id(agent_id, tm->agent_table_size);
    agent_entry_t* entry = tm->agent_table[index];

    while (entry) {
        if (entry->agent_id == agent_id) {
            return entry;
        }
        entry = entry->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent_entry: validation failed");
    return NULL;
}

/**
 * @brief Find domain entry by signature
 */
static domain_entry_t* find_domain_entry(
    transactive_memory_t tm,
    const prime_signature_t* sig
) {
    if (!tm || !sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_domain_entry: required parameter is NULL (tm, sig)");
        return NULL;
    }

    size_t index = hash_signature(sig, tm->domain_table_size);
    domain_entry_t* entry = tm->domain_table[index];

    while (entry) {
        if (prime_sig_equal(&entry->signature, sig)) {
            return entry;
        }
        entry = entry->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_domain_entry: validation failed");
    return NULL;
}

/**
 * @brief Find expertise entry for an agent's domain
 */
static expertise_entry_t* find_expertise_entry(
    transactive_agent_t* agent,
    const prime_signature_t* domain_sig
) {
    if (!agent || !domain_sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_expertise_entry: required parameter is NULL (agent, domain_sig)");
        return NULL;
    }

    for (size_t i = 0; i < agent->num_expertise; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && agent->num_expertise > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)agent->num_expertise);
        }

        if (prime_sig_jaccard(&agent->expertise[i].domain_signature, domain_sig) > 0.95f) {
            return &agent->expertise[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_expertise_entry: validation failed");
    return NULL;
}

/**
 * @brief Calculate agent score for a domain query
 */
static float calculate_agent_score(
    transactive_memory_t tm,
    transactive_agent_t* agent,
    const prime_signature_t* query_sig
) {
    if (!tm || !agent || !query_sig) return 0.0f;

    float best_expertise = 0.0f;
    float best_confidence = 0.0f;

    // Find best matching expertise
    for (size_t i = 0; i < agent->num_expertise; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && agent->num_expertise > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)agent->num_expertise);
        }

        float similarity = prime_sig_jaccard(
            &agent->expertise[i].domain_signature,
            query_sig
        );
        if (similarity > 0.5f) {
            float effective_expertise = agent->expertise[i].expertise_level * similarity;
            if (effective_expertise > best_expertise) {
                best_expertise = effective_expertise;
                best_confidence = agent->expertise[i].confidence;
            }
        }
    }

    if (best_expertise < tm->config.min_expertise_threshold) {
        return 0.0f;
    }

    // Combine factors according to configuration
    float score = 0.0f;
    score += tm->config.expertise_weight * best_expertise;
    score += tm->config.reliability_weight * agent->overall_reliability;
    score += tm->config.accessibility_weight *
             (agent->is_active ? 1.0f : 0.0f) *
             (agent->expertise ? agent->expertise[0].accessibility : 0.5f);
    score += tm->config.familiarity_weight * agent->familiarity;

    // Adjust by confidence
    score *= (0.5f + 0.5f * best_confidence);

    return score;
}

/**
 * @brief Find a free delegation slot
 */
static delegation_state_t* find_free_delegation(transactive_memory_t tm) {
    if (!tm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tm is NULL");

        return NULL;

    }

    // Find inactive slot
    for (size_t i = 0; i < tm->delegation_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->delegation_capacity > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->delegation_capacity);
        }

        if (!tm->delegations[i].is_active) {
            return &tm->delegations[i];
        }
    }

    // Need to expand
    if (tm->delegation_capacity >= TRANSACTIVE_MAX_DELEGATIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_free_delegation: capacity exceeded");
        return NULL;
    }

    size_t new_capacity = tm->delegation_capacity * 2;
    if (new_capacity > TRANSACTIVE_MAX_DELEGATIONS) {
        new_capacity = TRANSACTIVE_MAX_DELEGATIONS;
    }

    delegation_state_t* new_delegations = (delegation_state_t*)nimcp_realloc(
        tm->delegations,
        new_capacity * sizeof(delegation_state_t)
    );
    if (!new_delegations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_delegations");

        return NULL;
    }

    // Initialize new slots
    for (size_t i = tm->delegation_capacity; i < new_capacity; i++) {
        memset(&new_delegations[i], 0, sizeof(delegation_state_t));
    }

    tm->delegations = new_delegations;
    delegation_state_t* result = &tm->delegations[tm->delegation_capacity];
    tm->delegation_capacity = new_capacity;

    return result;
}

/**
 * @brief Find delegation by ID
 */
static delegation_state_t* find_delegation(
    transactive_memory_t tm,
    uint64_t delegation_id
) {
    if (!tm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tm is NULL");

        return NULL;

    }

    for (size_t i = 0; i < tm->delegation_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->delegation_capacity > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->delegation_capacity);
        }

        if (tm->delegations[i].delegation_id == delegation_id &&
            tm->delegations[i].is_active) {
            return &tm->delegations[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_delegation: operation failed");
    return NULL;
}

/**
 * @brief Free agent resources
 */
static void free_agent(transactive_agent_t* agent) {
    if (!agent) return;

    if (agent->agent_name) {
        nimcp_free(agent->agent_name);
        agent->agent_name = NULL;
    }
    if (agent->expertise) {
        nimcp_free(agent->expertise);
        agent->expertise = NULL;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT transactive_config_t transactive_config_default(void) {
    transactive_config_t config = {
        .initial_agent_capacity = 64,
        .initial_domain_capacity = 32,
        .min_expertise_threshold = TRANSACTIVE_MIN_EXPERTISE_THRESHOLD,
        .min_confidence_threshold = TRANSACTIVE_MIN_CONFIDENCE_THRESHOLD,
        .verification_decay_rate = 0.1f,
        .expertise_weight = 0.4f,
        .reliability_weight = 0.3f,
        .accessibility_weight = 0.2f,
        .familiarity_weight = 0.1f,
        .default_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS,
        .auto_verify = true,
        .track_history = true
    };
    return config;
}

NIMCP_EXPORT bool transactive_config_validate(const transactive_config_t* config) {
    if (!config) {
        set_error("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "transactive_config_validate: config is NULL");
        return false;
    }

    // Check capacities
    if (config->initial_agent_capacity == 0 ||
        config->initial_domain_capacity == 0) {
        set_error("Invalid capacity (must be > 0)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "transactive_config_validate: config is NULL");
        return false;
    }

    // Check thresholds
    if (config->min_expertise_threshold < 0.0f ||
        config->min_expertise_threshold > 1.0f ||
        config->min_confidence_threshold < 0.0f ||
        config->min_confidence_threshold > 1.0f) {
        set_error("Thresholds must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "transactive_config_validate: operation failed");
        return false;
    }

    // Check weights
    if (config->expertise_weight < 0.0f ||
        config->reliability_weight < 0.0f ||
        config->accessibility_weight < 0.0f ||
        config->familiarity_weight < 0.0f) {
        set_error("Weights must be >= 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "transactive_config_validate: operation failed");
        return false;
    }

    float weight_sum = config->expertise_weight +
                       config->reliability_weight +
                       config->accessibility_weight +
                       config->familiarity_weight;

    if (weight_sum < TRANSACTIVE_EPSILON) {
        set_error("At least one weight must be > 0");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Management
//=============================================================================

NIMCP_EXPORT transactive_memory_t transactive_create(const transactive_config_t* config) {
    return transactive_create_with_pr(config, NULL, NULL);
}

NIMCP_EXPORT transactive_memory_t transactive_create_with_pr(
    const transactive_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
) {
    transactive_config_t cfg = config ? *config : transactive_config_default();

    if (!transactive_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "transactive_create_with_pr: transactive_config_validate is NULL");
        return NULL;
    }

    transactive_memory_t tm = (transactive_memory_t)nimcp_calloc(1, sizeof(*tm));
    if (!tm) {
        set_error("Failed to allocate transactive memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "transactive_create_with_pr: tm is NULL");
        return NULL;
    }

    tm->config = cfg;

    // Allocate agent hash table
    tm->agent_table_size = cfg.initial_agent_capacity;
    tm->agent_table = (agent_entry_t**)nimcp_calloc(
        tm->agent_table_size,
        sizeof(agent_entry_t*)
    );
    if (!tm->agent_table) {
        set_error("Failed to allocate agent table");
        nimcp_free(tm);
        tm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "transactive_create_with_pr: tm->agent_table is NULL");
        return NULL;
    }

    // Allocate domain hash table
    tm->domain_table_size = cfg.initial_domain_capacity;
    tm->domain_table = (domain_entry_t**)nimcp_calloc(
        tm->domain_table_size,
        sizeof(domain_entry_t*)
    );
    if (!tm->domain_table) {
        set_error("Failed to allocate domain table");
        nimcp_free(tm->agent_table);
        nimcp_free(tm);
        tm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "transactive_create_with_pr: tm->domain_table is NULL");
        return NULL;
    }

    // Allocate delegation array
    tm->delegation_capacity = 16;
    tm->delegations = (delegation_state_t*)nimcp_calloc(
        tm->delegation_capacity,
        sizeof(delegation_state_t)
    );
    if (!tm->delegations) {
        set_error("Failed to allocate delegation array");
        nimcp_free(tm->domain_table);
        nimcp_free(tm->agent_table);
        nimcp_free(tm);
        tm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "transactive_create_with_pr: tm->delegations is NULL");
        return NULL;
    }

    tm->next_delegation_id = 1;
    tm->entanglement = entanglement;
    tm->node_manager = node_manager;

    // Initialize statistics
    memset(&tm->stats, 0, sizeof(tm->stats));

    set_error(NULL);
    return tm;
}

NIMCP_EXPORT void transactive_destroy(transactive_memory_t tm) {
    if (!tm) return;

    // Free agents
    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            agent_entry_t* next = entry->next;
            free_agent(&entry->agent);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(tm->agent_table);

    // Free domains
    for (size_t i = 0; i < tm->domain_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->domain_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->domain_table_size);
        }

        domain_entry_t* entry = tm->domain_table[i];
        while (entry) {
            domain_entry_t* next = entry->next;
            if (entry->name) {
                nimcp_free(entry->name);
            }
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }
    nimcp_free(tm->domain_table);

    // Free delegations
    nimcp_free(tm->delegations);

    nimcp_free(tm);
    tm = NULL;
}

//=============================================================================
// Agent Registration
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_register_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    const char* agent_name
) {
    return transactive_register_agent_with_expertise(
        tm, agent_id, agent_name, NULL, 0
    );
}

NIMCP_EXPORT transactive_error_t transactive_register_agent_with_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const char* agent_name,
    const expertise_entry_t* expertise,
    size_t num_expertise
) {
    if (!tm) {
        set_error("NULL transactive memory");
        return TRANSACTIVE_ERROR_NULL;
    }

    if (agent_id == TRANSACTIVE_INVALID_AGENT_ID) {
        set_error("Invalid agent ID");
        return TRANSACTIVE_ERROR_INVALID;
    }

    // Check if already exists
    if (find_agent_entry(tm, agent_id)) {
        set_error("Agent already registered");
        return TRANSACTIVE_ERROR_EXISTS;
    }

    // Check capacity
    if (tm->num_agents >= TRANSACTIVE_MAX_AGENTS) {
        set_error("Maximum agents reached");
        return TRANSACTIVE_ERROR_FULL;
    }

    // Create entry
    agent_entry_t* entry = (agent_entry_t*)nimcp_calloc(1, sizeof(agent_entry_t));
    if (!entry) {
        set_error("Failed to allocate agent entry");
        return TRANSACTIVE_ERROR_NO_MEMORY;
    }

    entry->agent_id = agent_id;
    entry->agent.agent_id = agent_id;

    // Copy name
    if (agent_name) {
        entry->agent.agent_name = strdup(agent_name);
        if (!entry->agent.agent_name) {
            nimcp_free(entry);
            entry = NULL;
            set_error("Failed to allocate agent name");
            return TRANSACTIVE_ERROR_NO_MEMORY;
        }
    }

    // Allocate expertise array
    size_t expertise_capacity = num_expertise > 0 ?
        num_expertise : TRANSACTIVE_MAX_EXPERTISE_PER_AGENT / 4;
    entry->agent.expertise = (expertise_entry_t*)nimcp_calloc(
        expertise_capacity,
        sizeof(expertise_entry_t)
    );
    if (!entry->agent.expertise) {
        if (entry->agent.agent_name) nimcp_free(entry->agent.agent_name);
        nimcp_free(entry);
        entry = NULL;
        set_error("Failed to allocate expertise array");
        return TRANSACTIVE_ERROR_NO_MEMORY;
    }
    entry->agent.expertise_capacity = expertise_capacity;

    // Copy initial expertise
    if (expertise && num_expertise > 0) {
        memcpy(entry->agent.expertise, expertise,
               num_expertise * sizeof(expertise_entry_t));
        entry->agent.num_expertise = num_expertise;
    }

    // Initialize metadata
    entry->agent.overall_reliability = 0.5f;  // Neutral until proven
    entry->agent.retrieval_success_rate = 0.5f;
    entry->agent.avg_response_time = 0.0f;
    entry->agent.familiarity = 0.0f;
    entry->agent.trust = 0.5f;
    entry->agent.is_active = true;
    entry->agent.is_verified = false;
    entry->agent.last_interaction_ms = transactive_current_time_ms();

    // Insert into hash table
    size_t index = hash_agent_id(agent_id, tm->agent_table_size);
    entry->next = tm->agent_table[index];
    tm->agent_table[index] = entry;
    tm->num_agents++;
    tm->stats.num_agents = tm->num_agents;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_unregister_agent(
    transactive_memory_t tm,
    uint64_t agent_id
) {
    if (!tm) {
        set_error("NULL transactive memory");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t index = hash_agent_id(agent_id, tm->agent_table_size);
    agent_entry_t** pp = &tm->agent_table[index];

    while (*pp) {
        if ((*pp)->agent_id == agent_id) {
            agent_entry_t* entry = *pp;
            *pp = entry->next;

            free_agent(&entry->agent);
            nimcp_free(entry);
            entry = NULL;

            tm->num_agents--;
            tm->stats.num_agents = tm->num_agents;

            set_error(NULL);
            return TRANSACTIVE_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    set_error("Agent not found");
    return TRANSACTIVE_ERROR_NOT_FOUND;
}

NIMCP_EXPORT transactive_error_t transactive_get_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    transactive_agent_t* agent
) {
    if (!tm || !agent) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    // Copy agent data (shallow copy for arrays)
    *agent = entry->agent;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_update_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    float reliability,
    float accessibility,
    bool is_active
) {
    if (!tm) {
        set_error("NULL transactive memory");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    if (reliability >= 0.0f && reliability <= 1.0f) {
        entry->agent.overall_reliability = reliability;
    }

    // Update accessibility for all expertise entries
    if (accessibility >= 0.0f && accessibility <= 1.0f) {
        for (size_t i = 0; i < entry->agent.num_expertise; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && entry->agent.num_expertise > 256) {
                transactive_heartbeat("transactive_loop",
                                 (float)(i + 1) / (float)entry->agent.num_expertise);
            }

            entry->agent.expertise[i].accessibility = accessibility;
        }
    }

    entry->agent.is_active = is_active;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT bool transactive_agent_exists(
    transactive_memory_t tm,
    uint64_t agent_id
) {
    return find_agent_entry(tm, agent_id) != NULL;
}

NIMCP_EXPORT transactive_error_t transactive_get_all_agents(
    transactive_memory_t tm,
    uint64_t* agents,
    size_t max_agents,
    size_t* count
) {
    if (!tm || !agents || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < tm->agent_table_size && n < max_agents; i++) {
        agent_entry_t* entry = tm->agent_table[i];
        while (entry && n < max_agents) {
            agents[n++] = entry->agent_id;
            entry = entry->next;
        }
    }

    *count = n;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Expertise Management
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_update_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    float expertise_level,
    float confidence
) {
    if (!tm || !domain_signature) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    if (expertise_level < 0.0f || expertise_level > 1.0f ||
        confidence < 0.0f || confidence > 1.0f) {
        set_error("Values must be in [0, 1]");
        return TRANSACTIVE_ERROR_INVALID;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    // Find existing expertise entry
    expertise_entry_t* exp = find_expertise_entry(&entry->agent, domain_signature);

    if (exp) {
        // Update existing
        exp->expertise_level = expertise_level;
        exp->confidence = confidence;
        exp->last_verification = (float)transactive_current_time_ms() / 1000.0f;
    } else {
        // Add new
        if (entry->agent.num_expertise >= entry->agent.expertise_capacity) {
            if (entry->agent.expertise_capacity >= TRANSACTIVE_MAX_EXPERTISE_PER_AGENT) {
                set_error("Maximum expertise entries reached");
                return TRANSACTIVE_ERROR_FULL;
            }

            size_t new_capacity = entry->agent.expertise_capacity * 2;
            if (new_capacity > TRANSACTIVE_MAX_EXPERTISE_PER_AGENT) {
                new_capacity = TRANSACTIVE_MAX_EXPERTISE_PER_AGENT;
            }

            expertise_entry_t* new_expertise = (expertise_entry_t*)nimcp_realloc(
                entry->agent.expertise,
                new_capacity * sizeof(expertise_entry_t)
            );
            if (!new_expertise) {
                set_error("Failed to expand expertise array");
                return TRANSACTIVE_ERROR_NO_MEMORY;
            }

            entry->agent.expertise = new_expertise;
            entry->agent.expertise_capacity = new_capacity;
        }

        // Initialize new entry
        expertise_entry_t* new_exp = &entry->agent.expertise[entry->agent.num_expertise];
        memset(new_exp, 0, sizeof(expertise_entry_t));

        new_exp->agent_id = agent_id;
        new_exp->domain_signature = *domain_signature;
        new_exp->expertise_level = expertise_level;
        new_exp->confidence = confidence;
        new_exp->accessibility = 1.0f;
        new_exp->last_verification = (float)transactive_current_time_ms() / 1000.0f;

        entry->agent.num_expertise++;
        tm->stats.num_expertise_entries++;
    }

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_remove_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature
) {
    if (!tm || !domain_signature) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    for (size_t i = 0; i < entry->agent.num_expertise; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entry->agent.num_expertise > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)entry->agent.num_expertise);
        }

        if (prime_sig_jaccard(&entry->agent.expertise[i].domain_signature,
                              domain_signature) > 0.95f) {
            // Shift remaining entries
            if (i < entry->agent.num_expertise - 1) {
                memmove(&entry->agent.expertise[i],
                        &entry->agent.expertise[i + 1],
                        (entry->agent.num_expertise - i - 1) * sizeof(expertise_entry_t));
            }
            entry->agent.num_expertise--;
            tm->stats.num_expertise_entries--;

            set_error(NULL);
            return TRANSACTIVE_SUCCESS;
        }
    }

    set_error("Expertise not found");
    return TRANSACTIVE_ERROR_NOT_FOUND;
}

NIMCP_EXPORT transactive_error_t transactive_get_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    expertise_entry_t* entry_out
) {
    if (!tm || !domain_signature || !entry_out) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    expertise_entry_t* exp = find_expertise_entry(&entry->agent, domain_signature);
    if (!exp) {
        set_error("Expertise not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    *entry_out = *exp;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_get_agent_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    expertise_entry_t* entries,
    size_t max_entries,
    size_t* count
) {
    if (!tm || !entries || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    size_t n = entry->agent.num_expertise;
    if (n > max_entries) n = max_entries;

    memcpy(entries, entry->agent.expertise, n * sizeof(expertise_entry_t));
    *count = n;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_verify_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    bool verification_success,
    float new_confidence
) {
    if (!tm || !domain_signature) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    expertise_entry_t* exp = find_expertise_entry(&entry->agent, domain_signature);
    if (!exp) {
        set_error("Expertise not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    exp->last_verification = (float)transactive_current_time_ms() / 1000.0f;

    if (verification_success) {
        exp->successful_queries++;
        if (new_confidence >= 0.0f) {
            exp->confidence = new_confidence;
        } else {
            // Auto-calculate: increase confidence
            exp->confidence = fminf(1.0f, exp->confidence + 0.1f * (1.0f - exp->confidence));
        }
    } else {
        exp->failed_queries++;
        if (new_confidence >= 0.0f) {
            exp->confidence = new_confidence;
        } else {
            // Auto-calculate: decrease confidence
            exp->confidence = fmaxf(0.0f, exp->confidence - 0.1f);
        }
    }

    entry->agent.is_verified = true;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Directory Lookup
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_lookup(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    directory_result_t* result
) {
    return transactive_lookup_constrained(
        tm, query_signature, 0.0f, 0.0f, NULL, 0, result
    );
}

NIMCP_EXPORT transactive_error_t transactive_lookup_constrained(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    float min_expertise,
    float min_reliability,
    const uint64_t* exclude_agents,
    size_t num_exclude,
    directory_result_t* result
) {
    if (!tm || !query_signature || !result) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    // Initialize result
    memset(result, 0, sizeof(directory_result_t));
    result->query_signature = *query_signature;

    // Allocate temporary storage for scoring
    typedef struct {
        uint64_t agent_id;
        float score;
    } score_pair_t;

    score_pair_t* scores = (score_pair_t*)nimcp_calloc(tm->num_agents, sizeof(score_pair_t));
    if (!scores && tm->num_agents > 0) {
        set_error("Failed to allocate score array");
        return TRANSACTIVE_ERROR_NO_MEMORY;
    }

    size_t num_scores = 0;

    // Score all agents
    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            // Check exclusion
            bool excluded = false;
            if (exclude_agents) {
                for (size_t j = 0; j < num_exclude; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && num_exclude > 256) {
                        transactive_heartbeat("transactive_loop",
                                         (float)(j + 1) / (float)num_exclude);
                    }

                    if (exclude_agents[j] == entry->agent_id) {
                        excluded = true;
                        break;
                    }
                }
            }

            if (!excluded && entry->agent.is_active) {
                // Check minimum reliability
                if (entry->agent.overall_reliability >= min_reliability) {
                    float score = calculate_agent_score(tm, &entry->agent, query_signature);

                    if (score >= min_expertise) {
                        scores[num_scores].agent_id = entry->agent_id;
                        scores[num_scores].score = score;
                        num_scores++;
                    }
                }
            }

            entry = entry->next;
        }
    }

    // Sort by score (descending)
    for (size_t i = 0; i < num_scores; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_scores > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)num_scores);
        }

        for (size_t j = i + 1; j < num_scores; j++) {
            if (scores[j].score > scores[i].score) {
                score_pair_t tmp = scores[i];
                scores[i] = scores[j];
                scores[j] = tmp;
            }
        }
    }

    // Build result
    size_t result_count = num_scores;
    if (result_count > TRANSACTIVE_MAX_RECOMMENDATIONS) {
        result_count = TRANSACTIVE_MAX_RECOMMENDATIONS;
    }

    if (result_count > 0) {
        result->recommended_agents = (uint64_t*)nimcp_calloc(result_count, sizeof(uint64_t));
        result->agent_scores = (float*)nimcp_calloc(result_count, sizeof(float));

        if (!result->recommended_agents || !result->agent_scores) {
            nimcp_free(result->recommended_agents);
            nimcp_free(result->agent_scores);
            nimcp_free(scores);
            scores = NULL;
            result->recommended_agents = NULL;
            result->agent_scores = NULL;
            set_error("Failed to allocate result arrays");
            return TRANSACTIVE_ERROR_NO_MEMORY;
        }

        for (size_t i = 0; i < result_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result_count > 256) {
                transactive_heartbeat("transactive_loop",
                                 (float)(i + 1) / (float)result_count);
            }

            result->recommended_agents[i] = scores[i].agent_id;
            result->agent_scores[i] = scores[i].score;
        }

        result->num_recommendations = result_count;
        result->best_score = scores[0].score;
    }

    // Calculate coverage
    float total_coverage = 0.0f;
    for (size_t i = 0; i < num_scores; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_scores > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)num_scores);
        }

        total_coverage += scores[i].score;
    }
    result->coverage = fminf(1.0f, total_coverage);

    nimcp_free(scores);
    scores = NULL;
    tm->stats.total_lookups++;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_get_top_experts(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    size_t k,
    uint64_t* agent_ids,
    float* scores,
    size_t* count
) {
    if (!tm || !domain_signature || !agent_ids || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    directory_result_t result;
    transactive_error_t err = transactive_lookup(tm, domain_signature, &result);
    if (err != TRANSACTIVE_SUCCESS) {
        return err;
    }

    size_t n = result.num_recommendations;
    if (n > k) n = k;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)n);
        }

        agent_ids[i] = result.recommended_agents[i];
        if (scores) {
            scores[i] = result.agent_scores[i];
        }
    }

    *count = n;

    transactive_free_directory_result(&result);

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT void transactive_free_directory_result(directory_result_t* result) {
    if (!result) return;

    nimcp_free(result->recommended_agents);
    nimcp_free(result->agent_scores);
    result->recommended_agents = NULL;
    result->agent_scores = NULL;
    result->num_recommendations = 0;
}

//=============================================================================
// Delegation
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_delegate(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    uint64_t target_agent,
    uint64_t timeout_ms,
    uint64_t* delegation_id
) {
    if (!tm || !query_signature || !delegation_id) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    // Auto-select target if not specified
    if (target_agent == 0) {
        directory_result_t result;
        transactive_error_t err = transactive_lookup(tm, query_signature, &result);
        if (err != TRANSACTIVE_SUCCESS) {
            return err;
        }

        if (result.num_recommendations == 0) {
            transactive_free_directory_result(&result);
            set_error("No experts found");
            return TRANSACTIVE_ERROR_NOT_FOUND;
        }

        target_agent = result.recommended_agents[0];
        transactive_free_directory_result(&result);
    }

    // Verify target exists and is active
    agent_entry_t* agent_entry = find_agent_entry(tm, target_agent);
    if (!agent_entry) {
        set_error("Target agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    if (!agent_entry->agent.is_active) {
        set_error("Target agent is unavailable");
        return TRANSACTIVE_ERROR_UNAVAILABLE;
    }

    // Find free delegation slot
    delegation_state_t* deleg = find_free_delegation(tm);
    if (!deleg) {
        set_error("No delegation slots available");
        return TRANSACTIVE_ERROR_FULL;
    }

    // Initialize delegation
    deleg->delegation_id = tm->next_delegation_id++;
    deleg->query_id = deleg->delegation_id;  // Same as delegation for now
    deleg->target_agent = target_agent;
    deleg->query_signature = *query_signature;
    deleg->start_time_ms = transactive_current_time_ms();
    deleg->timeout_ms = timeout_ms > 0 ? timeout_ms : tm->config.default_timeout_ms;
    deleg->is_active = true;
    deleg->has_response = false;
    deleg->confidence_prediction = transactive_predict_retrieval_success(
        tm, query_signature, target_agent
    );

    tm->num_delegations++;
    tm->stats.active_delegations = tm->num_delegations;
    tm->stats.total_delegations++;

    // Update agent's delegation count
    agent_entry->agent.total_delegations++;

    *delegation_id = deleg->delegation_id;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_receive_answer(
    transactive_memory_t tm,
    uint64_t delegation_id,
    const delegation_result_t* result
) {
    if (!tm || !result) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    delegation_state_t* deleg = find_delegation(tm, delegation_id);
    if (!deleg) {
        set_error("Delegation not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    // Mark as complete
    deleg->has_response = true;
    deleg->is_active = false;
    tm->num_delegations--;
    tm->stats.active_delegations = tm->num_delegations;

    // Update statistics
    if (result->success) {
        tm->stats.successful_delegations++;
    }

    // Update from interaction
    transactive_update_from_interaction(
        tm,
        deleg->target_agent,
        &deleg->query_signature,
        result->success,
        result->response_quality,
        result->response_time_ms
    );

    // Auto-verify if configured
    if (tm->config.auto_verify && result->success) {
        transactive_verify_expertise(
            tm,
            deleg->target_agent,
            &deleg->query_signature,
            true,
            -1.0f
        );
    }

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_cancel_delegation(
    transactive_memory_t tm,
    uint64_t delegation_id
) {
    if (!tm) {
        set_error("NULL transactive memory");
        return TRANSACTIVE_ERROR_NULL;
    }

    delegation_state_t* deleg = find_delegation(tm, delegation_id);
    if (!deleg) {
        set_error("Delegation not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    deleg->is_active = false;
    tm->num_delegations--;
    tm->stats.active_delegations = tm->num_delegations;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_get_delegation_state(
    transactive_memory_t tm,
    uint64_t delegation_id,
    delegation_state_t* state
) {
    if (!tm || !state) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    delegation_state_t* deleg = find_delegation(tm, delegation_id);
    if (!deleg) {
        set_error("Delegation not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    *state = *deleg;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_check_timeouts(
    transactive_memory_t tm,
    uint64_t current_time_ms,
    uint64_t* timed_out,
    size_t max_results,
    size_t* count
) {
    if (!tm || !timed_out || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < tm->delegation_capacity && n < max_results; i++) {
        if (tm->delegations[i].is_active) {
            uint64_t elapsed = current_time_ms - tm->delegations[i].start_time_ms;
            if (elapsed > tm->delegations[i].timeout_ms) {
                timed_out[n++] = tm->delegations[i].delegation_id;
                tm->delegations[i].is_active = false;
                tm->num_delegations--;
            }
        }
    }

    tm->stats.active_delegations = tm->num_delegations;
    *count = n;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Prediction and Analysis
//=============================================================================

NIMCP_EXPORT float transactive_predict_retrieval_success(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    uint64_t target_agent
) {
    if (!tm || !query_signature) {
        return -1.0f;
    }

    agent_entry_t* entry = find_agent_entry(tm, target_agent);
    if (!entry) {
        return -1.0f;
    }

    // Find best matching expertise
    float best_match = 0.0f;
    float best_confidence = 0.0f;

    for (size_t i = 0; i < entry->agent.num_expertise; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entry->agent.num_expertise > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)entry->agent.num_expertise);
        }

        float similarity = prime_sig_jaccard(
            &entry->agent.expertise[i].domain_signature,
            query_signature
        );
        if (similarity > best_match) {
            best_match = similarity;
            best_confidence = entry->agent.expertise[i].confidence;
        }
    }

    // Calculate prediction
    float expertise_factor = best_match * entry->agent.expertise[0].expertise_level;
    float reliability_factor = entry->agent.overall_reliability;
    float success_rate_factor = entry->agent.retrieval_success_rate;
    float confidence_factor = best_confidence;

    float prediction = expertise_factor * 0.4f +
                       reliability_factor * 0.3f +
                       success_rate_factor * 0.2f +
                       confidence_factor * 0.1f;

    return fminf(1.0f, fmaxf(0.0f, prediction));
}

NIMCP_EXPORT transactive_error_t transactive_compute_domain_coverage(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    domain_coverage_t* coverage
) {
    if (!tm || !domain_signature || !coverage) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    memset(coverage, 0, sizeof(domain_coverage_t));
    coverage->domain_signature = *domain_signature;

    float total_expertise = 0.0f;
    float max_expertise = 0.0f;
    uint64_t top_expert = TRANSACTIVE_INVALID_AGENT_ID;
    size_t expert_count = 0;

    // Scan all agents
    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            for (size_t j = 0; j < entry->agent.num_expertise; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && entry->agent.num_expertise > 256) {
                    transactive_heartbeat("transactive_loop",
                                     (float)(j + 1) / (float)entry->agent.num_expertise);
                }

                float similarity = prime_sig_jaccard(
                    &entry->agent.expertise[j].domain_signature,
                    domain_signature
                );
                if (similarity > 0.5f) {
                    float effective_expertise =
                        entry->agent.expertise[j].expertise_level * similarity;
                    total_expertise += effective_expertise;
                    expert_count++;

                    if (effective_expertise > max_expertise) {
                        max_expertise = effective_expertise;
                        top_expert = entry->agent_id;
                    }
                }
            }
            entry = entry->next;
        }
    }

    coverage->total_coverage = fminf(1.0f, total_expertise);
    coverage->expert_count = expert_count;
    coverage->avg_expertise = expert_count > 0 ?
        total_expertise / (expert_count > 0 ? expert_count : 1) : 0.0f;
    coverage->max_expertise = max_expertise;
    coverage->top_expert = top_expert;
    coverage->redundancy = expert_count > 1 ?
        fminf(1.0f, (float)(expert_count - 1) / 5.0f) : 0.0f;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT float transactive_compute_overall_coverage(transactive_memory_t tm) {
    if (!tm) return 0.0f;

    if (tm->num_domains == 0 || tm->num_agents == 0) {
        return 0.0f;
    }

    float total_coverage = 0.0f;
    size_t domains_checked = 0;

    for (size_t i = 0; i < tm->domain_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->domain_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->domain_table_size);
        }

        domain_entry_t* domain = tm->domain_table[i];
        while (domain) {
            domain_coverage_t cov;
            if (transactive_compute_domain_coverage(tm, &domain->signature, &cov) ==
                TRANSACTIVE_SUCCESS) {
                total_coverage += cov.total_coverage;
                domains_checked++;
            }
            domain = domain->next;
        }
    }

    return domains_checked > 0 ? total_coverage / domains_checked : 0.0f;
}

NIMCP_EXPORT transactive_error_t transactive_find_coverage_gaps(
    transactive_memory_t tm,
    float min_coverage,
    domain_coverage_t* gaps,
    size_t max_gaps,
    size_t* count
) {
    if (!tm || !gaps || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t n = 0;

    for (size_t i = 0; i < tm->domain_table_size && n < max_gaps; i++) {
        domain_entry_t* domain = tm->domain_table[i];
        while (domain && n < max_gaps) {
            domain_coverage_t cov;
            if (transactive_compute_domain_coverage(tm, &domain->signature, &cov) ==
                TRANSACTIVE_SUCCESS) {
                if (cov.total_coverage < min_coverage) {
                    gaps[n++] = cov;
                }
            }
            domain = domain->next;
        }
    }

    *count = n;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Learning and Adaptation
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_update_from_interaction(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    bool success,
    float response_quality,
    uint64_t response_time_ms
) {
    if (!tm || !domain_signature) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    // Update agent statistics
    entry->agent.total_interactions++;
    entry->agent.last_interaction_ms = transactive_current_time_ms();

    if (success) {
        entry->agent.successful_delegations++;
    }

    // Update success rate (exponential moving average)
    float alpha = 0.1f;
    float new_sample = success ? 1.0f : 0.0f;
    entry->agent.retrieval_success_rate =
        alpha * new_sample + (1.0f - alpha) * entry->agent.retrieval_success_rate;

    // Update response time (exponential moving average)
    entry->agent.avg_response_time =
        alpha * (float)response_time_ms +
        (1.0f - alpha) * entry->agent.avg_response_time;

    // Update reliability based on quality
    if (response_quality >= 0.0f && response_quality <= 1.0f) {
        entry->agent.overall_reliability =
            alpha * response_quality +
            (1.0f - alpha) * entry->agent.overall_reliability;
    }

    // Update familiarity and trust
    entry->agent.familiarity = fminf(1.0f, entry->agent.familiarity + 0.05f);
    if (success && response_quality > 0.7f) {
        entry->agent.trust = fminf(1.0f, entry->agent.trust + 0.02f);
    } else if (!success) {
        entry->agent.trust = fmaxf(0.0f, entry->agent.trust - 0.05f);
    }

    // Update expertise entry
    expertise_entry_t* exp = find_expertise_entry(&entry->agent, domain_signature);
    if (exp) {
        if (success) {
            exp->successful_queries++;
        } else {
            exp->failed_queries++;
        }
    }

    // Update overall stats
    float n = (float)tm->stats.successful_delegations;
    tm->stats.avg_delegation_time_ms =
        (tm->stats.avg_delegation_time_ms * n + (float)response_time_ms) / (n + 1.0f);
    tm->stats.avg_success_rate =
        (float)tm->stats.successful_delegations / (float)tm->stats.total_delegations;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT size_t transactive_apply_decay(
    transactive_memory_t tm,
    float elapsed_seconds
) {
    if (!tm || elapsed_seconds <= 0.0f) return 0;

    size_t affected = 0;
    float decay_factor = expf(-tm->config.verification_decay_rate * elapsed_seconds / 86400.0f);

    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            for (size_t j = 0; j < entry->agent.num_expertise; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && entry->agent.num_expertise > 256) {
                    transactive_heartbeat("transactive_loop",
                                     (float)(j + 1) / (float)entry->agent.num_expertise);
                }

                float old_confidence = entry->agent.expertise[j].confidence;
                entry->agent.expertise[j].confidence *= decay_factor;
                if (entry->agent.expertise[j].confidence != old_confidence) {
                    affected++;
                }
            }
            entry = entry->next;
        }
    }

    return affected;
}

NIMCP_EXPORT transactive_error_t transactive_infer_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    size_t min_successes,
    expertise_entry_t* inferred,
    size_t max_inferred,
    size_t* count
) {
    if (!tm || !inferred || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        set_error("Agent not found");
        return TRANSACTIVE_ERROR_NOT_FOUND;
    }

    // Find expertise entries with sufficient successful queries
    // but not high confidence (indicating they were inferred)
    size_t n = 0;
    for (size_t i = 0; i < entry->agent.num_expertise && n < max_inferred; i++) {
        if (entry->agent.expertise[i].successful_queries >= min_successes &&
            entry->agent.expertise[i].confidence < 0.7f) {
            inferred[n++] = entry->agent.expertise[i];
        }
    }

    *count = n;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Domain Management
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_register_domain(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    const char* domain_name
) {
    if (!tm || !domain_signature) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    // Check if exists
    if (find_domain_entry(tm, domain_signature)) {
        // Already registered, not an error
        return TRANSACTIVE_SUCCESS;
    }

    // Check capacity
    if (tm->num_domains >= TRANSACTIVE_MAX_DOMAINS) {
        set_error("Maximum domains reached");
        return TRANSACTIVE_ERROR_FULL;
    }

    // Create entry
    domain_entry_t* entry = (domain_entry_t*)nimcp_calloc(1, sizeof(domain_entry_t));
    if (!entry) {
        set_error("Failed to allocate domain entry");
        return TRANSACTIVE_ERROR_NO_MEMORY;
    }

    entry->signature = *domain_signature;
    if (domain_name) {
        entry->name = strdup(domain_name);
        if (!entry->name) {
            nimcp_free(entry);
            entry = NULL;
            return TRANSACTIVE_ERROR_NO_MEMORY;
        }
    }

    // Insert into hash table
    size_t index = hash_signature(domain_signature, tm->domain_table_size);
    entry->next = tm->domain_table[index];
    tm->domain_table[index] = entry;
    tm->num_domains++;
    tm->stats.num_domains = tm->num_domains;

    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_get_all_domains(
    transactive_memory_t tm,
    prime_signature_t* domains,
    size_t max_domains,
    size_t* count
) {
    if (!tm || !domains || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < tm->domain_table_size && n < max_domains; i++) {
        domain_entry_t* entry = tm->domain_table[i];
        while (entry && n < max_domains) {
            domains[n++] = entry->signature;
            entry = entry->next;
        }
    }

    *count = n;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT transactive_error_t transactive_find_similar_domains(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    float min_similarity,
    prime_signature_t* similar,
    size_t max_similar,
    size_t* count
) {
    if (!tm || !query_signature || !similar || !count) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < tm->domain_table_size && n < max_similar; i++) {
        domain_entry_t* entry = tm->domain_table[i];
        while (entry && n < max_similar) {
            float sim = prime_sig_jaccard(&entry->signature, query_signature);
            if (sim >= min_similarity) {
                similar[n++] = entry->signature;
            }
            entry = entry->next;
        }
    }

    *count = n;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

NIMCP_EXPORT transactive_error_t transactive_get_stats(
    transactive_memory_t tm,
    transactive_stats_t* stats
) {
    if (!tm || !stats) {
        set_error("NULL argument");
        return TRANSACTIVE_ERROR_NULL;
    }

    // Calculate memory usage
    size_t mem = sizeof(*tm);
    mem += tm->agent_table_size * sizeof(agent_entry_t*);
    mem += tm->domain_table_size * sizeof(domain_entry_t*);
    mem += tm->delegation_capacity * sizeof(delegation_state_t);

    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            mem += sizeof(agent_entry_t);
            if (entry->agent.agent_name) {
                mem += strlen(entry->agent.agent_name) + 1;
            }
            mem += entry->agent.expertise_capacity * sizeof(expertise_entry_t);
            entry = entry->next;
        }
    }

    tm->stats.memory_bytes = mem;
    tm->stats.overall_coverage = transactive_compute_overall_coverage(tm);

    *stats = tm->stats;
    set_error(NULL);
    return TRANSACTIVE_SUCCESS;
}

NIMCP_EXPORT void transactive_reset_stats(transactive_memory_t tm) {
    if (!tm) return;

    tm->stats.total_lookups = 0;
    tm->stats.total_delegations = 0;
    tm->stats.successful_delegations = 0;
    tm->stats.avg_delegation_time_ms = 0.0f;
    tm->stats.avg_success_rate = 0.0f;
}

NIMCP_EXPORT const char* transactive_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}

NIMCP_EXPORT void transactive_print_summary(transactive_memory_t tm) {
    if (!tm) {
        printf("Transactive Memory: NULL\n");
        return;
    }

    transactive_stats_t stats;
    transactive_get_stats(tm, &stats);

    printf("=== Transactive Memory System ===\n");
    printf("Agents: %zu\n", stats.num_agents);
    printf("Domains: %zu\n", stats.num_domains);
    printf("Expertise entries: %zu\n", stats.num_expertise_entries);
    printf("Active delegations: %zu\n", stats.active_delegations);
    printf("Total lookups: %lu\n", (unsigned long)stats.total_lookups);
    printf("Total delegations: %lu\n", (unsigned long)stats.total_delegations);
    printf("Successful delegations: %lu (%.1f%%)\n",
           (unsigned long)stats.successful_delegations,
           stats.total_delegations > 0 ?
               100.0f * stats.successful_delegations / stats.total_delegations : 0.0f);
    printf("Avg delegation time: %.1f ms\n", stats.avg_delegation_time_ms);
    printf("Overall coverage: %.2f\n", stats.overall_coverage);
    printf("Memory usage: %zu bytes\n", stats.memory_bytes);
}

NIMCP_EXPORT void transactive_print_agent(
    transactive_memory_t tm,
    uint64_t agent_id
) {
    if (!tm) return;

    agent_entry_t* entry = find_agent_entry(tm, agent_id);
    if (!entry) {
        printf("Agent %lu: not found\n", (unsigned long)agent_id);
        return;
    }

    printf("=== Agent %lu ===\n", (unsigned long)agent_id);
    printf("Name: %s\n", entry->agent.agent_name ? entry->agent.agent_name : "(unnamed)");
    printf("Active: %s\n", entry->agent.is_active ? "yes" : "no");
    printf("Verified: %s\n", entry->agent.is_verified ? "yes" : "no");
    printf("Reliability: %.2f\n", entry->agent.overall_reliability);
    printf("Success rate: %.2f\n", entry->agent.retrieval_success_rate);
    printf("Avg response time: %.1f ms\n", entry->agent.avg_response_time);
    printf("Familiarity: %.2f\n", entry->agent.familiarity);
    printf("Trust: %.2f\n", entry->agent.trust);
    printf("Expertise entries: %zu\n", entry->agent.num_expertise);

    for (size_t i = 0; i < entry->agent.num_expertise; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entry->agent.num_expertise > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)entry->agent.num_expertise);
        }

        expertise_entry_t* exp = &entry->agent.expertise[i];
        printf("  - Domain (hash=%lx): level=%.2f, conf=%.2f, success=%lu, fail=%lu\n",
               (unsigned long)exp->domain_signature.hash,
               exp->expertise_level,
               exp->confidence,
               (unsigned long)exp->successful_queries,
               (unsigned long)exp->failed_queries);
    }
}

NIMCP_EXPORT bool transactive_validate(transactive_memory_t tm) {
    if (!tm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "transactive_validate: tm is NULL");
        return false;
    }

    // Verify agent count
    size_t agent_count = 0;
    for (size_t i = 0; i < tm->agent_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->agent_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->agent_table_size);
        }

        agent_entry_t* entry = tm->agent_table[i];
        while (entry) {
            agent_count++;
            entry = entry->next;
        }
    }

    if (agent_count != tm->num_agents) {
        set_error("Agent count mismatch");
        return false;
    }

    // Verify domain count
    size_t domain_count = 0;
    for (size_t i = 0; i < tm->domain_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->domain_table_size > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->domain_table_size);
        }

        domain_entry_t* entry = tm->domain_table[i];
        while (entry) {
            domain_count++;
            entry = entry->next;
        }
    }

    if (domain_count != tm->num_domains) {
        set_error("Domain count mismatch");
        return false;
    }

    // Verify delegation count
    size_t active_delegations = 0;
    for (size_t i = 0; i < tm->delegation_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tm->delegation_capacity > 256) {
            transactive_heartbeat("transactive_loop",
                             (float)(i + 1) / (float)tm->delegation_capacity);
        }

        if (tm->delegations[i].is_active) {
            active_delegations++;
        }
    }

    if (active_delegations != tm->num_delegations) {
        set_error("Delegation count mismatch");
        return false;
    }

    return true;
}

NIMCP_EXPORT const char* transactive_error_string(transactive_error_t error) {
    switch (error) {
        case TRANSACTIVE_SUCCESS:       return "Success";
        case TRANSACTIVE_ERROR_NULL:    return "NULL argument";
        case TRANSACTIVE_ERROR_NOT_FOUND: return "Not found";
        case TRANSACTIVE_ERROR_EXISTS:  return "Already exists";
        case TRANSACTIVE_ERROR_FULL:    return "Capacity exceeded";
        case TRANSACTIVE_ERROR_INVALID: return "Invalid parameter";
        case TRANSACTIVE_ERROR_NO_MEMORY: return "Memory allocation failed";
        case TRANSACTIVE_ERROR_TIMEOUT: return "Delegation timed out";
        case TRANSACTIVE_ERROR_UNAVAILABLE: return "Agent unavailable";
        default:                        return "Unknown error";
    }
}

NIMCP_EXPORT uint64_t transactive_current_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    // Fallback
    return (uint64_t)time(NULL) * 1000;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void transactive_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_transactive_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int transactive_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "transactive_training_begin: NULL argument");
        return -1;
    }
    transactive_heartbeat_instance(NULL, "transactive_training_begin", 0.0f);
    (void)(struct agent_entry_struct*)instance; /* Module state available for reset */
    return 0;
}

int transactive_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "transactive_training_end: NULL argument");
        return -1;
    }
    transactive_heartbeat_instance(NULL, "transactive_training_end", 1.0f);
    (void)(struct agent_entry_struct*)instance; /* Module state available for finalization */
    return 0;
}

int transactive_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "transactive_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    transactive_heartbeat_instance(NULL, "transactive_training_step", progress);
    (void)(struct agent_entry_struct*)instance; /* Module state available for step adaptation */
    return 0;
}

/**
 * @file nimcp_swarm_logic_bridge.c
 * @brief Implementation of Logic-Swarm Bridge
 *
 * Bridges neural logic gates with swarm intelligence for consensus,
 * validation, and distributed inference.
 */

#include "swarm/nimcp_swarm_consensus.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_security.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_logic_bridge)

/*=============================================================================
 * KG-DRIVEN WIRING INFRASTRUCTURE
 *============================================================================*/

/* Forward declaration of message handler */
static nimcp_error_t logic_bridge_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * Handler map for swarm logic bridge module.
 * Handles logic gate evaluation and quorum vote messages.
 */
DEFINE_HANDLER_MAP_BEGIN(swarm_logic_bridge)
    HANDLER_MAP_ENTRY(BIO_MSG_LOGIC_GATE_EVALUATE, logic_bridge_message_handler)
    HANDLER_MAP_ENTRY(BIO_MSG_SWARM_QUORUM_VOTE, logic_bridge_message_handler)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(swarm_logic_bridge, swarm_logic_bridge_t, bridge)

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

#define SWARM_LOGIC_DEFAULT_MAX_RULES 1000
#define SWARM_LOGIC_DEFAULT_CACHE_SIZE 256
#define SWARM_LOGIC_DEFAULT_MAX_AGENTS 100
#define SWARM_LOGIC_DEFAULT_INFERENCE_THRESHOLD 0.7f
#define SWARM_LOGIC_DEFAULT_TIMEOUT_MS 1000.0f
#define SWARM_LOGIC_CACHE_KEY_SIZE 64

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Cached evaluation entry
 */
typedef struct {
    uint32_t rule_id;
    uint64_t state_hash;
    swarm_logic_result_t result;
    uint64_t timestamp_us;
    bool valid;
} cache_entry_t;

/**
 * @brief Logic-swarm bridge internal structure
 */
struct swarm_logic_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    // Configuration
    swarm_logic_bridge_config_t config;

    // Rules
    swarm_logic_rule_t* rules;
    uint32_t rule_count;
    uint32_t rule_capacity;

    // Cache
    cache_entry_t* cache;
    uint32_t cache_size;
    uint32_t cache_next_index;

    // Neural logic network
    neural_logic_network_t logic_network;

    // Bio-async integration
    // Statistics
    swarm_logic_bridge_stats_t stats;

    // Thread safety
    nimcp_platform_mutex_t* mutex;

    // Security
    bool security_registered;

    // Enhanced integrations
    void* brain;                 /**< Brain handle for neuromodulation */
    void* immune_system;         /**< Immune system handle */
    void* umm;                   /**< UMM handle */
};

/*=============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

/**
 * @brief Calculate hash of agent states
 *
 * WHAT: Computes hash of agent states for cache lookup
 * WHY:  Enables fast cache hit detection
 * HOW:  Uses FNV-1a hash algorithm
 */
static uint64_t calculate_state_hash(const swarm_agent_state_t* states, uint32_t num_agents) {
    if (!states || num_agents == 0) {
        return 0;
    }

    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;

    for (uint32_t i = 0; i < num_agents; i++) {
        // Hash agent_id
        hash ^= states[i].agent_id;
        hash *= 1099511628211ULL;

        // Hash belief_value (convert to bits)
        uint32_t belief_bits;
        memcpy(&belief_bits, &states[i].belief_value, sizeof(uint32_t));
        hash ^= belief_bits;
        hash *= 1099511628211ULL;
    }

    return hash;
}

/**
 * @brief Find rule by ID
 *
 * WHAT: Finds rule in rules array
 * WHY:  Enables rule lookup
 * HOW:  Linear search through rules
 */
static swarm_logic_rule_t* find_rule(swarm_logic_bridge_t* bridge, uint32_t rule_id) {
    if (!bridge || !bridge->rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_rule: required parameter is NULL (bridge, bridge->rules)");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->rule_count; i++) {
        if (bridge->rules[i].rule_id == rule_id) {
            return &bridge->rules[i];
        }
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Check cache for evaluation result
 *
 * WHAT: Checks if evaluation result is cached
 * WHY:  Avoids redundant evaluations
 * HOW:  Compares rule_id and state_hash
 */
static bool check_cache(swarm_logic_bridge_t* bridge, uint32_t rule_id,
                       uint64_t state_hash, swarm_logic_result_t* result) {
    if (!bridge || !bridge->cache || !result) {
        return false;
    }

    for (uint32_t i = 0; i < bridge->cache_size; i++) {
        if (bridge->cache[i].valid &&
            bridge->cache[i].rule_id == rule_id &&
            bridge->cache[i].state_hash == state_hash) {

            // Check if cache entry is still fresh (< 1 second old)
            uint64_t now_us = nimcp_time_get_us();
            if (now_us - bridge->cache[i].timestamp_us < 1000000) {
                *result = bridge->cache[i].result;
                bridge->stats.cache_hits++;
                return true;
            }
        }
    }

    bridge->stats.cache_misses++;
    return false;
}

/**
 * @brief Add result to cache
 *
 * WHAT: Adds evaluation result to cache
 * WHY:  Speeds up repeated evaluations
 * HOW:  Ring buffer replacement
 */
static void add_to_cache(swarm_logic_bridge_t* bridge, uint32_t rule_id,
                        uint64_t state_hash, const swarm_logic_result_t* result) {
    if (!bridge || !bridge->cache || !result) {
        return;
    }

    uint32_t idx = bridge->cache_next_index;
    bridge->cache[idx].rule_id = rule_id;
    bridge->cache[idx].state_hash = state_hash;
    bridge->cache[idx].result = *result;
    bridge->cache[idx].timestamp_us = nimcp_time_get_us();
    bridge->cache[idx].valid = true;

    bridge->cache_next_index = (bridge->cache_next_index + 1) % bridge->cache_size;
}

/**
 * @brief Evaluate logic gate on agent states
 *
 * WHAT: Evaluates logic gate given agent belief values
 * WHY:  Core logic evaluation function
 * HOW:  Applies gate type to input values
 */
static float evaluate_logic_gate(logic_gate_type_t gate_type,
                                 const float* inputs,
                                 uint32_t num_inputs,
                                 float threshold) {
    if (!inputs || num_inputs == 0) {
        return 0.0F;
    }

    switch (gate_type) {
        case LOGIC_GATE_AND: {
            // AND: All inputs above threshold
            float min_value = inputs[0];
            for (uint32_t i = 1; i < num_inputs; i++) {
                if (inputs[i] < min_value) {
                    min_value = inputs[i];
                }
            }
            return min_value;
        }

        case LOGIC_GATE_OR: {
            // OR: Any input above threshold
            float max_value = inputs[0];
            for (uint32_t i = 1; i < num_inputs; i++) {
                if (inputs[i] > max_value) {
                    max_value = inputs[i];
                }
            }
            return max_value;
        }

        case LOGIC_GATE_NOT: {
            // NOT: Invert first input
            return 1.0F - inputs[0];
        }

        case LOGIC_GATE_XOR: {
            // XOR: Exactly one input above threshold
            uint32_t count_high = 0;
            float sum = 0.0F;
            for (uint32_t i = 0; i < num_inputs; i++) {
                if (inputs[i] >= threshold) {
                    count_high++;
                }
                sum += inputs[i];
            }

            // True if odd number of high inputs
            return (count_high % 2 == 1) ? sum / num_inputs : 0.0F;
        }

        case LOGIC_GATE_IMPLIES: {
            // IMPLIES: If A then B (¬A ∨ B)
            if (num_inputs < 2) {
                return 0.0F;
            }
            float a = inputs[0];
            float b = inputs[1];
            // (1 - A) OR B = max(1 - A, B)
            return fmaxf(1.0F - a, b);
        }

        default:
            return 0.0F;
    }
}

/**
 * @brief Bio-async message handler
 *
 * WHAT: Handles incoming bio-async messages
 * WHY:  Enables inter-module communication
 * HOW:  Processes message based on type
 */
static nimcp_error_t logic_bridge_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_INVALID_PARAM;
    }

    swarm_logic_bridge_t* bridge = (swarm_logic_bridge_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    (void)response_promise; // Not used yet

    LOG_DEBUG("Logic bridge received message type 0x%04X", header->type);

    // Handle different message types
    switch (header->type) {
        case BIO_MSG_LOGIC_GATE_EVALUATE:
            // Future: Handle logic gate evaluation request
            break;

        case BIO_MSG_SWARM_QUORUM_VOTE:
            // Future: Handle swarm vote
            break;

        default:
            LOG_DEBUG("Unhandled message type: 0x%04X", header->type);
            break;
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

void swarm_logic_bridge_get_default_config(swarm_logic_bridge_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(swarm_logic_bridge_config_t));

    config->max_rules = SWARM_LOGIC_DEFAULT_MAX_RULES;
    config->rule_cache_size = SWARM_LOGIC_DEFAULT_CACHE_SIZE;
    config->enable_bio_async = true;
    config->inference_threshold = SWARM_LOGIC_DEFAULT_INFERENCE_THRESHOLD;
    config->max_agents = SWARM_LOGIC_DEFAULT_MAX_AGENTS;
    config->timeout_ms = SWARM_LOGIC_DEFAULT_TIMEOUT_MS;
}

swarm_logic_bridge_t* swarm_logic_bridge_create(const swarm_logic_bridge_config_t* config) {
    LOG_INFO("Creating logic-swarm bridge");

    // Allocate bridge
    swarm_logic_bridge_t* bridge = (swarm_logic_bridge_t*)nimcp_calloc(1, sizeof(swarm_logic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_logic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&bridge->config, config, sizeof(swarm_logic_bridge_config_t));
    } else {
        swarm_logic_bridge_get_default_config(&bridge->config);
    }

    // Allocate rules array
    bridge->rule_capacity = bridge->config.max_rules;
    bridge->rules = (swarm_logic_rule_t*)nimcp_calloc(bridge->rule_capacity,
                                                       sizeof(swarm_logic_rule_t));
    if (!bridge->rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_logic_bridge_create: failed to allocate rules array");
        nimcp_free(bridge);
        return NULL;
    }

    // Allocate cache
    bridge->cache_size = bridge->config.rule_cache_size;
    bridge->cache = (cache_entry_t*)nimcp_calloc(bridge->cache_size,
                                                  sizeof(cache_entry_t));
    if (!bridge->cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_logic_bridge_create: failed to allocate cache");
        nimcp_free(bridge->rules);
        nimcp_free(bridge);
        return NULL;
    }

    // Create mutex
    if (bridge_base_init(&bridge->base, 0, "swarm_logic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_logic_bridge_create: failed to create mutex");
        nimcp_free(bridge->cache);
        nimcp_free(bridge->rules);
        nimcp_free(bridge);
        return NULL;
    }

    // Create neural logic network
    neural_logic_config_t logic_config;
    memset(&logic_config, 0, sizeof(neural_logic_config_t));
    logic_config.max_logic_neurons = bridge->config.max_rules * 2;
    logic_config.max_variables = bridge->config.max_agents;
    logic_config.variable_pattern_dim = 16;
    logic_config.use_gpu = false; // CPU only for now
    logic_config.enable_bio_async = bridge->config.enable_bio_async;
    logic_config.timestep_us = 100.0F;
    logic_config.integration_window_ms = 10.0F;

    bridge->logic_network = neural_logic_create(&logic_config);
    if (!bridge->logic_network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_logic_bridge_create: failed to create neural logic network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge->cache);
        nimcp_free(bridge->rules);
        nimcp_free(bridge);
        return NULL;
    }

    // Register with bio-async if enabled
    if (bridge->config.enable_bio_async) {
        bio_module_info_t module_info;
        memset(&module_info, 0, sizeof(bio_module_info_t));
        module_info.module_id = BIO_MODULE_UNKNOWN; // Will be assigned
        module_info.module_name = "swarm_logic_bridge";
        module_info.inbox_capacity = 0; // Use default
        module_info.user_data = bridge;

        bridge->base.bio_ctx = bio_router_register_module(&module_info);
        if (bridge->base.bio_ctx) {
            /* Register handlers via KG-driven wiring callback */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_SWARM_LOGIC_BRIDGE,
                (void*)swarm_logic_bridge_handler_callback,
                bridge
            );

            if (wiring_result != NIMCP_SUCCESS) {
                /* Legacy fallback: direct handler registration */
                LOG_DEBUG("KG wiring unavailable, using legacy registration");
                LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(bridge->base.bio_ctx,
                                           BIO_MSG_LOGIC_GATE_EVALUATE,
                                           logic_bridge_message_handler));
                LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(bridge->base.bio_ctx,
                                           BIO_MSG_SWARM_QUORUM_VOTE,
                                           logic_bridge_message_handler));
            }

            bridge->base.bio_async_enabled = true;
            LOG_INFO("Logic bridge registered with bio-async router");
        } else {
            LOG_WARN("Failed to register with bio-async router");
        }
    }

    // Security integration (optional - can be added later with proper context)
    bridge->security_registered = false;

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(swarm_logic_bridge_stats_t));

    LOG_INFO("Logic-swarm bridge created successfully");
    return bridge;
}

void swarm_logic_bridge_destroy(swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    LOG_INFO("Destroying logic-swarm bridge");

    // Unregister from bio-async
    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
    }

    // Destroy neural logic network
    if (bridge->logic_network) {
        neural_logic_destroy(bridge->logic_network);
    }

    // Free rules and their input arrays
    if (bridge->rules) {
        for (uint32_t i = 0; i < bridge->rule_count; i++) {
            if (bridge->rules[i].input_agent_ids) {
                nimcp_free(bridge->rules[i].input_agent_ids);
            }
        }
        nimcp_free(bridge->rules);
    }

    // Free cache
    if (bridge->cache) {
        nimcp_free(bridge->cache);
    }

    // Destroy mutex
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    LOG_INFO("Logic-swarm bridge destroyed");
}

/*=============================================================================
 * RULE MANAGEMENT
 *============================================================================*/

nimcp_error_t swarm_logic_bridge_add_rule(swarm_logic_bridge_t* bridge,
                                           const swarm_logic_rule_t* rule) {
    if (!bridge || !rule) {
        LOG_ERROR("Invalid parameters for add_rule");
        return NIMCP_INVALID_PARAM;
    }

    if (!rule->input_agent_ids || rule->num_inputs == 0) {
        LOG_ERROR("Rule must have at least one input");
        return NIMCP_INVALID_PARAM;
    }

    if (rule->gate_type >= LOGIC_GATE_COUNT) {
        LOG_ERROR("Invalid gate type: %d", rule->gate_type);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Check capacity
    if (bridge->rule_count >= bridge->rule_capacity) {
        LOG_ERROR("Rule capacity exceeded: %u/%u", bridge->rule_count, bridge->rule_capacity);
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NO_MEMORY;
    }

    // Check for duplicate rule ID
    if (find_rule(bridge, rule->rule_id) != NULL) {
        LOG_ERROR("Rule ID %u already exists", rule->rule_id);
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_INVALID_PARAM;
    }

    // Copy rule
    swarm_logic_rule_t* new_rule = &bridge->rules[bridge->rule_count];
    memcpy(new_rule, rule, sizeof(swarm_logic_rule_t));

    // Allocate and copy input agent IDs
    new_rule->input_agent_ids = (uint32_t*)nimcp_malloc(rule->num_inputs * sizeof(uint32_t));
    if (!new_rule->input_agent_ids) {
        LOG_ERROR("Failed to allocate input agent IDs");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NO_MEMORY;
    }

    memcpy(new_rule->input_agent_ids, rule->input_agent_ids,
           rule->num_inputs * sizeof(uint32_t));

    bridge->rule_count++;
    bridge->stats.active_rules = bridge->rule_count;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Added rule %u: gate=%d, inputs=%u", rule->rule_id,
             rule->gate_type, rule->num_inputs);

    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_bridge_remove_rule(swarm_logic_bridge_t* bridge,
                                              uint32_t rule_id) {
    if (!bridge) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Find rule
    int index = -1;
    for (uint32_t i = 0; i < bridge->rule_count; i++) {
        if (bridge->rules[i].rule_id == rule_id) {
            index = (int)i;
            break;
        }
    }

    if (index < 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        LOG_ERROR("Rule %u not found", rule_id);
        return NIMCP_NOT_FOUND;
    }

    // Free input array
    if (bridge->rules[index].input_agent_ids) {
        nimcp_free(bridge->rules[index].input_agent_ids);
    }

    // Shift remaining rules
    if ((uint32_t)index < bridge->rule_count - 1) {
        memmove(&bridge->rules[index], &bridge->rules[index + 1],
                (bridge->rule_count - index - 1) * sizeof(swarm_logic_rule_t));
    }

    bridge->rule_count--;
    bridge->stats.active_rules = bridge->rule_count;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Removed rule %u", rule_id);
    return NIMCP_SUCCESS;
}

const swarm_logic_rule_t* swarm_logic_bridge_get_rule(swarm_logic_bridge_t* bridge,
                                                       uint32_t rule_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_logic_bridge_get_rule: bridge is NULL");
        return NULL;
    }

    return find_rule(bridge, rule_id);
}

uint32_t swarm_logic_bridge_get_all_rules(swarm_logic_bridge_t* bridge,
                                           const swarm_logic_rule_t** rules,
                                           uint32_t max_rules) {
    if (!bridge || !rules || max_rules == 0) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint32_t count = bridge->rule_count < max_rules ? bridge->rule_count : max_rules;
    for (uint32_t i = 0; i < count; i++) {
        rules[i] = &bridge->rules[i];
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return count;
}

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

int swarm_logic_bridge_evaluate(swarm_logic_bridge_t* bridge,
                                 const swarm_agent_state_t* agent_states,
                                 uint32_t num_agents,
                                 swarm_logic_result_t* results,
                                 uint32_t max_results) {
    if (!bridge || !agent_states || num_agents == 0 || !results || max_results == 0) {
        LOG_ERROR("Invalid parameters for evaluate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_logic_bridge_evaluate: required parameter is NULL (bridge, agent_states, results)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint64_t state_hash = calculate_state_hash(agent_states, num_agents);
    uint32_t result_count = 0;

    // Evaluate each rule
    for (uint32_t i = 0; i < bridge->rule_count && result_count < max_results; i++) {
        swarm_logic_rule_t* rule = &bridge->rules[i];

        // Check cache first
        if (check_cache(bridge, rule->rule_id, state_hash, &results[result_count])) {
            result_count++;
            continue;
        }

        // Collect input values from agent states
        float* input_values = (float*)nimcp_malloc(rule->num_inputs * sizeof(float));
        if (!input_values) {
            continue;
        }

        uint32_t valid_inputs = 0;
        for (uint32_t j = 0; j < rule->num_inputs; j++) {
            // Find agent with matching ID
            for (uint32_t k = 0; k < num_agents; k++) {
                if (agent_states[k].agent_id == rule->input_agent_ids[j] &&
                    agent_states[k].is_active) {
                    input_values[valid_inputs++] = agent_states[k].belief_value;
                    break;
                }
            }
        }

        if (valid_inputs == 0) {
            nimcp_free(input_values);
            continue;
        }

        // Evaluate logic gate
        uint64_t start_time = nimcp_time_get_us();
        float gate_output = evaluate_logic_gate(rule->gate_type, input_values,
                                               valid_inputs, rule->threshold);
        uint64_t eval_time = nimcp_time_get_us() - start_time;

        nimcp_free(input_values);

        // Create result
        swarm_logic_result_t* result = &results[result_count];
        memset(result, 0, sizeof(swarm_logic_result_t));
        result->rule_id = rule->rule_id;
        result->result = (gate_output >= rule->threshold);
        result->confidence = gate_output * rule->confidence_weight;
        result->num_inputs_used = valid_inputs;
        result->evaluation_time_us = eval_time;

        snprintf(result->explanation, sizeof(result->explanation),
                "Rule %u: gate=%d, inputs=%u/%u, output=%.3f",
                rule->rule_id, rule->gate_type, valid_inputs,
                rule->num_inputs, gate_output);

        // Add to cache
        add_to_cache(bridge, rule->rule_id, state_hash, result);

        result_count++;
        bridge->stats.total_evaluations++;
        bridge->stats.successful_evaluations++;
    }

    // Update statistics
    if (result_count > 0) {
        uint64_t total_time = 0;
        for (uint32_t i = 0; i < result_count; i++) {
            total_time += results[i].evaluation_time_us;
        }
        bridge->stats.avg_evaluation_time_us = (float)total_time / result_count;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Evaluated %u rules with %u agents", result_count, num_agents);
    return (int)result_count;
}

nimcp_error_t swarm_logic_bridge_evaluate_rule(swarm_logic_bridge_t* bridge,
                                                uint32_t rule_id,
                                                const swarm_agent_state_t* agent_states,
                                                uint32_t num_agents,
                                                swarm_logic_result_t* result) {
    if (!bridge || !agent_states || num_agents == 0 || !result) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Find rule
    swarm_logic_rule_t* rule = find_rule(bridge, rule_id);
    if (!rule) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NOT_FOUND;
    }

    // Check cache
    uint64_t state_hash = calculate_state_hash(agent_states, num_agents);
    if (check_cache(bridge, rule_id, state_hash, result)) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    // Collect input values
    float* input_values = (float*)nimcp_malloc(rule->num_inputs * sizeof(float));
    if (!input_values) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NO_MEMORY;
    }

    uint32_t valid_inputs = 0;
    for (uint32_t j = 0; j < rule->num_inputs; j++) {
        for (uint32_t k = 0; k < num_agents; k++) {
            if (agent_states[k].agent_id == rule->input_agent_ids[j] &&
                agent_states[k].is_active) {
                input_values[valid_inputs++] = agent_states[k].belief_value;
                break;
            }
        }
    }

    if (valid_inputs == 0) {
        nimcp_free(input_values);
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NOT_FOUND;
    }

    // Evaluate
    uint64_t start_time = nimcp_time_get_us();
    float gate_output = evaluate_logic_gate(rule->gate_type, input_values,
                                           valid_inputs, rule->threshold);
    uint64_t eval_time = nimcp_time_get_us() - start_time;

    nimcp_free(input_values);

    // Fill result
    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = rule_id;
    result->result = (gate_output >= rule->threshold);
    result->confidence = gate_output * rule->confidence_weight;
    result->num_inputs_used = valid_inputs;
    result->evaluation_time_us = eval_time;

    snprintf(result->explanation, sizeof(result->explanation),
            "Rule %u: gate=%d, inputs=%u/%u, output=%.3f",
            rule_id, rule->gate_type, valid_inputs, rule->num_inputs, gate_output);

    // Add to cache
    add_to_cache(bridge, rule_id, state_hash, result);

    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * CONSENSUS VALIDATION
 *============================================================================*/

nimcp_error_t swarm_logic_bridge_validate_consensus(swarm_logic_bridge_t* bridge,
                                                     const float* votes,
                                                     uint32_t num_votes,
                                                     logic_gate_type_t consensus_type,
                                                     swarm_logic_result_t* result) {
    if (!bridge || !votes || num_votes == 0 || !result) {
        return NIMCP_INVALID_PARAM;
    }

    if (consensus_type != LOGIC_GATE_AND && consensus_type != LOGIC_GATE_OR) {
        LOG_ERROR("Invalid consensus type: %d (must be AND or OR)", consensus_type);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint64_t start_time = nimcp_time_get_us();
    float gate_output = evaluate_logic_gate(consensus_type, votes, num_votes, 0.5F);
    uint64_t eval_time = nimcp_time_get_us() - start_time;

    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = 0; // Consensus has no rule ID
    result->result = (gate_output >= 0.5F);
    result->confidence = gate_output;
    result->num_inputs_used = num_votes;
    result->evaluation_time_us = eval_time;

    const char* consensus_str = (consensus_type == LOGIC_GATE_AND) ? "unanimous" : "majority";
    snprintf(result->explanation, sizeof(result->explanation),
            "Consensus (%s): %u votes, result=%.3f",
            consensus_str, num_votes, gate_output);

    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    LOG_INFO("Consensus validation: %s with %u votes = %s",
             consensus_str, num_votes, result->result ? "TRUE" : "FALSE");

    return NIMCP_SUCCESS;
}

int swarm_logic_bridge_detect_contradiction(swarm_logic_bridge_t* bridge,
                                            const swarm_agent_state_t* beliefs,
                                            uint32_t num_beliefs,
                                            uint32_t (*contradictions)[2],
                                            uint32_t max_contradictions) {
    if (!bridge || !beliefs || num_beliefs < 2 || !contradictions || max_contradictions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, beliefs, contradictions)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint32_t contradiction_count = 0;

    // Check all pairs of beliefs
    for (uint32_t i = 0; i < num_beliefs && contradiction_count < max_contradictions; i++) {
        if (!beliefs[i].is_active) {
            continue;
        }

        for (uint32_t j = i + 1; j < num_beliefs && contradiction_count < max_contradictions; j++) {
            if (!beliefs[j].is_active) {
                continue;
            }

            // XOR: contradiction if beliefs are opposite
            float inputs[2] = { beliefs[i].belief_value, beliefs[j].belief_value };
            float xor_result = evaluate_logic_gate(LOGIC_GATE_XOR, inputs, 2, 0.5F);

            // High XOR result indicates contradiction
            if (xor_result >= bridge->config.inference_threshold) {
                contradictions[contradiction_count][0] = beliefs[i].agent_id;
                contradictions[contradiction_count][1] = beliefs[j].agent_id;
                contradiction_count++;

                LOG_DEBUG("Contradiction detected: agent %u (%.3f) vs agent %u (%.3f)",
                         beliefs[i].agent_id, beliefs[i].belief_value,
                         beliefs[j].agent_id, beliefs[j].belief_value);
            }
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return (int)contradiction_count;
}

nimcp_error_t swarm_logic_bridge_validate_implication(swarm_logic_bridge_t* bridge,
                                                       uint32_t antecedent_agent,
                                                       uint32_t consequent_agent,
                                                       const swarm_agent_state_t* agent_states,
                                                       uint32_t num_agents,
                                                       swarm_logic_result_t* result) {
    if (!bridge || !agent_states || num_agents == 0 || !result) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Find agents
    float antecedent_value = 0.0F;
    float consequent_value = 0.0F;
    bool found_antecedent = false;
    bool found_consequent = false;

    for (uint32_t i = 0; i < num_agents; i++) {
        if (agent_states[i].agent_id == antecedent_agent && agent_states[i].is_active) {
            antecedent_value = agent_states[i].belief_value;
            found_antecedent = true;
        }
        if (agent_states[i].agent_id == consequent_agent && agent_states[i].is_active) {
            consequent_value = agent_states[i].belief_value;
            found_consequent = true;
        }
    }

    if (!found_antecedent || !found_consequent) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_NOT_FOUND;
    }

    // Evaluate IMPLIES gate
    float inputs[2] = { antecedent_value, consequent_value };
    uint64_t start_time = nimcp_time_get_us();
    float gate_output = evaluate_logic_gate(LOGIC_GATE_IMPLIES, inputs, 2, 0.5F);
    uint64_t eval_time = nimcp_time_get_us() - start_time;

    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = 0;
    result->result = (gate_output >= 0.5F);
    result->confidence = gate_output;
    result->num_inputs_used = 2;
    result->evaluation_time_us = eval_time;

    snprintf(result->explanation, sizeof(result->explanation),
            "Implication: agent %u (%.3f) => agent %u (%.3f) = %.3f",
            antecedent_agent, antecedent_value,
            consequent_agent, consequent_value, gate_output);

    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

int swarm_logic_bridge_process_inbox(swarm_logic_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, 10);
    return (int)processed;
}

nimcp_error_t swarm_logic_bridge_send_result(swarm_logic_bridge_t* bridge,
                                              bio_module_id_t target_module,
                                              const swarm_logic_result_t* result) {
    if (!bridge || !result || !bridge->base.bio_async_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    // Create logic result message
    bio_msg_logic_gate_result_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_GATE_RESULT,
                       bio_module_context_get_id(bridge->base.bio_ctx),
                       target_module, sizeof(msg));

    msg.gate_id = result->rule_id;
    msg.gate_type = 0; // Unknown gate type in result
    msg.output = result->result ? 1.0F : 0.0F;
    msg.spiked = result->result;
    msg.spike_time_us = result->evaluation_time_us;
    msg.threshold_used = 0.5F;

    return bio_router_send(bridge->base.bio_ctx, &msg, sizeof(msg), 0);
}

nimcp_error_t swarm_logic_bridge_broadcast_consensus(swarm_logic_bridge_t* bridge,
                                                      const swarm_logic_result_t* result) {
    if (!bridge || !result || !bridge->base.bio_async_enabled) {
        return NIMCP_INVALID_PARAM;
    }

    // Create consensus message
    bio_msg_logic_gate_result_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_LOGIC_GATE_RESULT,
                       bio_module_context_get_id(bridge->base.bio_ctx),
                       BIO_MODULE_ALL, sizeof(msg));

    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.gate_id = result->rule_id;
    msg.output = result->confidence;
    msg.spiked = result->result;

    return bio_router_broadcast(bridge->base.bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * STATISTICS AND DEBUGGING
 *============================================================================*/

nimcp_error_t swarm_logic_bridge_get_stats(swarm_logic_bridge_t* bridge,
                                            swarm_logic_bridge_stats_t* stats) {
    if (!bridge || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(swarm_logic_bridge_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

void swarm_logic_bridge_reset_stats(swarm_logic_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->stats.total_evaluations = 0;
    bridge->stats.successful_evaluations = 0;
    bridge->stats.failed_evaluations = 0;
    bridge->stats.cache_hits = 0;
    bridge->stats.cache_misses = 0;
    bridge->stats.avg_evaluation_time_us = 0.0F;

    // Keep active counts

    nimcp_platform_mutex_unlock(bridge->base.mutex);
}

void swarm_logic_bridge_clear_cache(swarm_logic_bridge_t* bridge) {
    if (!bridge || !bridge->cache) {
        return;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->cache_size; i++) {
        bridge->cache[i].valid = false;
    }

    bridge->cache_next_index = 0;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    LOG_DEBUG("Logic bridge cache cleared");
}

/*=============================================================================
 * ENHANCED CONSENSUS INTEGRATION
 *============================================================================*/

nimcp_error_t swarm_logic_validate_consensus_votes(swarm_logic_bridge_t* bridge,
                                                     const swarm_vote_response_t* votes,
                                                     uint32_t vote_count,
                                                     swarm_logic_result_t* result) {
    if (!bridge || !votes || vote_count == 0 || !result) {
        NIMCP_LOGGING_ERROR("Invalid parameters for consensus vote validation");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Count vote choices
    uint32_t agree_count = 0;
    uint32_t disagree_count = 0;
    uint32_t abstain_count = 0;
    float total_confidence = 0.0F;

    for (uint32_t i = 0; i < vote_count; i++) {
        if (votes[i].choice == VOTE_CHOICE_AGREE) {
            agree_count++;
            total_confidence += votes[i].confidence;
        } else if (votes[i].choice == VOTE_CHOICE_DISAGREE) {
            disagree_count++;
        } else if (votes[i].choice == VOTE_CHOICE_ABSTAIN) {
            abstain_count++;
        }
    }

    // Calculate majority ratio and average confidence
    float majority_ratio = (vote_count > 0) ? (float)agree_count / vote_count : 0.0F;
    float avg_confidence = (agree_count > 0) ? total_confidence / agree_count : 0.0F;

    // Consensus requires majority (>50%) AND sufficient confidence (>=0.5)
    bool has_majority = (agree_count * 2 > vote_count);
    bool has_confidence = (avg_confidence >= 0.5F);
    bool consensus_reached = has_majority && has_confidence;

    // Fill result
    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = 0;
    result->result = consensus_reached;
    result->confidence = avg_confidence;  // Always report the avg confidence
    result->num_inputs_used = vote_count;
    result->evaluation_time_us = 0;

    snprintf(result->explanation, sizeof(result->explanation),
            "Consensus validation: %u agree, %u disagree, %u abstain, conf=%.2f",
            agree_count, disagree_count, abstain_count, avg_confidence);

    bridge->stats.consensus_validations++;
    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Validated consensus: %u votes, result=%s",
                       vote_count, result->result ? "PASS" : "FAIL");

    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_detect_byzantine_pattern(swarm_logic_bridge_t* bridge,
                                                     const swarm_vote_response_t* votes,
                                                     uint32_t vote_count,
                                                     byzantine_detection_t* byzantine_agents,
                                                     uint32_t* byzantine_count) {
    if (!bridge || !votes || vote_count == 0 || !byzantine_agents || !byzantine_count) {
        NIMCP_LOGGING_ERROR("Invalid parameters for Byzantine detection");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *byzantine_count = 0;

    // Check for duplicate votes from same agent with different choices
    for (uint32_t i = 0; i < vote_count && *byzantine_count < 32; i++) {
        for (uint32_t j = i + 1; j < vote_count; j++) {
            if (votes[i].voter_drone == votes[j].voter_drone) {
                // Same agent voted multiple times
                if (votes[i].choice != votes[j].choice) {
                    // Contradictory votes - Byzantine behavior
                    byzantine_agents[*byzantine_count].agent_id = votes[i].voter_drone;
                    byzantine_agents[*byzantine_count].suspicion_score = 1.0F;
                    byzantine_agents[*byzantine_count].contradiction_count = 1;
                    snprintf(byzantine_agents[*byzantine_count].reason,
                            sizeof(byzantine_agents[*byzantine_count].reason),
                            "Contradictory votes on proposal %u", votes[i].proposal_id);
                    (*byzantine_count)++;

                    bridge->stats.byzantine_detections++;

                    NIMCP_LOGGING_WARN("Byzantine pattern detected: agent %u voted contradictory",
                                      votes[i].voter_drone);
                }
                break;
            }
        }
    }

    bridge->stats.total_evaluations++;
    if (*byzantine_count > 0) {
        bridge->stats.failed_evaluations++;
    } else {
        bridge->stats.successful_evaluations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * ENHANCED QUORUM INTEGRATION
 *============================================================================*/

nimcp_error_t swarm_logic_validate_quorum_signals(swarm_logic_bridge_t* bridge,
                                                    const nimcp_signal_molecule_t* signals,
                                                    uint32_t signal_count,
                                                    swarm_logic_result_t* result) {
    if (!bridge || !signals || signal_count == 0 || !result) {
        NIMCP_LOGGING_ERROR("Invalid parameters for quorum signal validation");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Collect signal concentrations
    float* concentrations = (float*)nimcp_malloc(signal_count * sizeof(float));
    if (!concentrations) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    uint32_t active_signals = 0;
    for (uint32_t i = 0; i < signal_count; i++) {
        concentrations[i] = (float)signals[i].concentration;
        if (signals[i].threshold_reached) {
            active_signals++;
        }
    }

    // Use AND gate to validate all signals are consistent
    float gate_output = evaluate_logic_gate(LOGIC_GATE_AND, concentrations,
                                           signal_count, 0.5F);

    nimcp_free(concentrations);

    // Fill result
    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = 0;
    result->result = (active_signals > 0 && gate_output >= 0.3F);
    result->confidence = gate_output;
    result->num_inputs_used = signal_count;
    result->evaluation_time_us = 0;

    snprintf(result->explanation, sizeof(result->explanation),
            "Quorum validation: %u/%u signals active, output=%.3f",
            active_signals, signal_count, gate_output);

    bridge->stats.quorum_validations++;
    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_check_signal_exclusion(swarm_logic_bridge_t* bridge,
                                                   nimcp_signal_type_t signal_a,
                                                   nimcp_signal_type_t signal_b,
                                                   bool* mutually_exclusive) {
    if (!bridge || !mutually_exclusive) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Define mutually exclusive signal pairs
    // ATTACK XOR RETREAT, EXPLORE XOR DEFEND
    *mutually_exclusive = false;

    if ((signal_a == NIMCP_SIGNAL_ATTACK && signal_b == NIMCP_SIGNAL_RETREAT) ||
        (signal_a == NIMCP_SIGNAL_RETREAT && signal_b == NIMCP_SIGNAL_ATTACK)) {
        *mutually_exclusive = true;
    } else if ((signal_a == NIMCP_SIGNAL_EXPLORE && signal_b == NIMCP_SIGNAL_DEFEND) ||
               (signal_a == NIMCP_SIGNAL_DEFEND && signal_b == NIMCP_SIGNAL_EXPLORE)) {
        *mutually_exclusive = true;
    }

    NIMCP_LOGGING_DEBUG("Signal exclusion check: %d vs %d = %s",
                        signal_a, signal_b, *mutually_exclusive ? "EXCLUSIVE" : "COMPATIBLE");

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * ENHANCED EMERGENCE INTEGRATION
 *============================================================================*/

nimcp_error_t swarm_logic_validate_tier_transition(swarm_logic_bridge_t* bridge,
                                                     swarm_emergence_tier_t current_tier,
                                                     swarm_emergence_tier_t target_tier,
                                                     const swarm_state_t* state,
                                                     bool* valid) {
    if (!bridge || !state || !valid) {
        NIMCP_LOGGING_ERROR("Invalid parameters for tier transition validation");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    *valid = false;

    // Get minimum drones required for target tier
    uint32_t min_drones = 1;
    switch (target_tier) {
        case SWARM_TIER_INDIVIDUAL: min_drones = 1; break;
        case SWARM_TIER_PAIR: min_drones = 2; break;
        case SWARM_TIER_SQUAD: min_drones = 4; break;
        case SWARM_TIER_PLATOON: min_drones = 8; break;
        case SWARM_TIER_COMPANY: min_drones = 16; break;
        case SWARM_TIER_BATTALION: min_drones = 32; break;
        default: min_drones = 1; break;
    }

    // Check prerequisites using AND gate
    float inputs[3];
    inputs[0] = (state->connected_drones >= min_drones) ? 1.0F : 0.0F;
    inputs[1] = state->collective_coherence;
    inputs[2] = (state->healthy_drones >= (uint32_t)(state->connected_drones * 0.75F)) ? 1.0F : 0.0F;

    float gate_output = evaluate_logic_gate(LOGIC_GATE_AND, inputs, 3, 0.7F);

    // Check if sequential (no tier skipping)
    bool sequential = (target_tier == current_tier + 1) ||
                     (target_tier == current_tier - 1) ||
                     (target_tier == current_tier);

    *valid = (gate_output >= 0.7F) && sequential;

    bridge->stats.tier_validations++;
    bridge->stats.total_evaluations++;
    if (*valid) {
        bridge->stats.successful_evaluations++;
    } else {
        bridge->stats.failed_evaluations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Tier transition validation: %d -> %d = %s (drones=%u, coherence=%.2f)",
                       current_tier, target_tier, *valid ? "VALID" : "INVALID",
                       state->connected_drones, state->collective_coherence);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BRAIN INTEGRATION
 *============================================================================*/

nimcp_error_t swarm_logic_connect_brain(swarm_logic_bridge_t* bridge, void* brain) {
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->brain = brain;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Logic bridge connected to brain");
    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_connect_immune(swarm_logic_bridge_t* bridge, void* immune_system) {
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->immune_system = immune_system;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Logic bridge connected to immune system");
    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_connect_umm(swarm_logic_bridge_t* bridge, void* umm) {
    if (!bridge) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->umm = umm;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Logic bridge connected to UMM");
    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_logic_evaluate_with_modulation(swarm_logic_bridge_t* bridge,
                                                     uint32_t rule_id,
                                                     float dopamine_level,
                                                     float acetylcholine_level,
                                                     swarm_logic_result_t* result) {
    if (!bridge || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // Find rule
    swarm_logic_rule_t* rule = find_rule(bridge, rule_id);
    if (!rule) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Modulate threshold
    // Dopamine lowers threshold (increases excitability)
    // Acetylcholine increases precision (raises threshold slightly)
    float modulated_threshold = rule->threshold;
    modulated_threshold *= (1.0F - dopamine_level * 0.3F);  // DA lowers by up to 30%
    modulated_threshold *= (1.0F + acetylcholine_level * 0.2F);  // ACh raises by up to 20%

    // Clamp to valid range
    if (modulated_threshold < 0.0F) modulated_threshold = 0.0F;
    if (modulated_threshold > 1.0F) modulated_threshold = 1.0F;

    // Store original threshold
    float original_threshold = rule->threshold;
    rule->threshold = modulated_threshold;

    // Evaluate with modulated threshold
    // Since we need agent states but don't have them, we create a minimal result
    memset(result, 0, sizeof(swarm_logic_result_t));
    result->rule_id = rule_id;
    result->result = false;
    result->confidence = 0.5F;
    result->num_inputs_used = 0;
    result->evaluation_time_us = 0;

    snprintf(result->explanation, sizeof(result->explanation),
            "Modulated evaluation: rule=%u, DA=%.2f, ACh=%.2f, threshold=%.3f->%.3f",
            rule_id, dopamine_level, acetylcholine_level,
            original_threshold, modulated_threshold);

    // Restore original threshold
    rule->threshold = original_threshold;

    bridge->stats.total_evaluations++;
    bridge->stats.successful_evaluations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

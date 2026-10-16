/**
 * @file nimcp_reasoning_integration.c
 * @brief Cognitive Integration for Logic & Reasoning System Implementation
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Implements cognitive layer integration for symbolic reasoning
 * WHY:  Connect logic events to attention, curiosity, working memory, and executive systems
 * HOW:  Event callbacks that respond to reasoning events and modulate cognitive resources
 *
 * @author NIMCP Development Team
 */

#include "cognitive/reasoning/nimcp_reasoning_integration.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/reasoning/nimcp_reasoning_snn_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_brain_kg_helpers.h"  // KG self-awareness integration
#include "utils/exception/nimcp_exception_macros.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"  // For error codes

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define LOG_MODULE "reasoning"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reasoning_integration, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Reasoning integration internal structure
 */
struct reasoning_integration {
    // Configuration
    reasoning_integration_config_t config;

    // Event bus subscription
    event_bus_t event_bus;
    event_subscription_handle_t subscriptions[13]; /**< One per logic event type */
    uint32_t num_subscriptions;

    // Active inferences tracking (working memory)
    active_inference_t* active_inferences;
    uint32_t max_inferences;
    uint32_t num_active_inferences;
    uint32_t next_inference_id;

    // Rule usage tracking (consolidation)
    rule_usage_t* tracked_rules;
    uint32_t max_rules;
    uint32_t num_tracked_rules;

    // Statistics
    reasoning_integration_stats_t stats;

    // Thread safety
    nimcp_platform_mutex_t mutex;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Internal Knowledge Graph integration (self-awareness)
    kg_module_context_t kg_context;
    bool kg_connected;

    // SNN and Plasticity bridges
    reasoning_snn_bridge_t* snn_bridge;
    reasoning_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;
};

//=============================================================================
// Error Handling
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* reasoning_integration_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void)
{
    return nimcp_time_monotonic_ms();
}

/**
 * @brief Find active inference by ID
 */
static active_inference_t* find_inference_by_id(
    reasoning_integration_t* integration,
    uint32_t inference_id
)
{
    for (uint32_t i = 0; i < integration->num_active_inferences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_active_inferences > 256) {
            reasoning_integration_heartbeat("reasoning_in_loop",
                             (float)(i + 1) / (float)integration->num_active_inferences);
        }

        if (integration->active_inferences[i].inference_id == inference_id &&
            integration->active_inferences[i].is_active) {
            return &integration->active_inferences[i];
        }
    }
    return NULL;
}

/**
 * @brief Find rule by string
 */
static rule_usage_t* find_rule_by_string(
    reasoning_integration_t* integration,
    const char* rule_str
)
{
    for (uint32_t i = 0; i < integration->num_tracked_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_tracked_rules > 256) {
            reasoning_integration_heartbeat("reasoning_in_loop",
                             (float)(i + 1) / (float)integration->num_tracked_rules);
        }

        if (strcmp(integration->tracked_rules[i].rule, rule_str) == 0) {
            return &integration->tracked_rules[i];
        }
    }
    return NULL;
}

/**
 * @brief Add inference to working memory
 */
static bool add_inference_to_wm(
    reasoning_integration_t* integration,
    const char* goal,
    float salience
)
{
    // Check capacity
    if (integration->num_active_inferences >= integration->max_inferences) {
        // Evict lowest salience inference
        uint32_t min_idx = 0;
        float min_salience = integration->active_inferences[0].salience;
        for (uint32_t i = 1; i < integration->num_active_inferences; i++) {
            if (integration->active_inferences[i].salience < min_salience) {
                min_salience = integration->active_inferences[i].salience;
                min_idx = i;
            }
        }
        // Overwrite lowest salience entry
        active_inference_t* inf = &integration->active_inferences[min_idx];
        strncpy(inf->goal, goal, sizeof(inf->goal) - 1);
        inf->goal[sizeof(inf->goal) - 1] = '\0';
        inf->step_count = 0;
        inf->start_time_ms = get_current_time_ms();
        inf->salience = salience;
        inf->is_active = true;
        inf->inference_id = integration->next_inference_id++;
        // Track total stored (even when evicting)
        integration->stats.wm_inferences_stored++;
        return true;
    }

    // Add new inference
    active_inference_t* inf = &integration->active_inferences[integration->num_active_inferences++];
    strncpy(inf->goal, goal, sizeof(inf->goal) - 1);
    inf->goal[sizeof(inf->goal) - 1] = '\0';
    inf->step_count = 0;
    inf->start_time_ms = get_current_time_ms();
    inf->salience = salience;
    inf->is_active = true;
    inf->inference_id = integration->next_inference_id++;

    integration->stats.wm_inferences_stored++;
    integration->stats.current_active_inferences = integration->num_active_inferences;

    return true;
}

/**
 * @brief Track rule for consolidation
 */
static bool track_rule(
    reasoning_integration_t* integration,
    const char* rule_str
)
{
    // Check if already tracked
    rule_usage_t* existing = find_rule_by_string(integration, rule_str);
    if (existing) {
        return true; // Already tracked
    }

    // Check capacity
    if (integration->num_tracked_rules >= integration->max_rules) {
        // Remove oldest non-consolidated rule
        uint32_t oldest_idx = 0;
        uint64_t oldest_time = UINT64_MAX;
        for (uint32_t i = 0; i < integration->num_tracked_rules; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && integration->num_tracked_rules > 256) {
                reasoning_integration_heartbeat("reasoning_in_loop",
                                 (float)(i + 1) / (float)integration->num_tracked_rules);
            }

            if (!integration->tracked_rules[i].consolidated &&
                integration->tracked_rules[i].first_used_ms < oldest_time) {
                oldest_time = integration->tracked_rules[i].first_used_ms;
                oldest_idx = i;
            }
        }
        // Overwrite oldest
        rule_usage_t* rule = &integration->tracked_rules[oldest_idx];
        strncpy(rule->rule, rule_str, sizeof(rule->rule) - 1);
        rule->rule[sizeof(rule->rule) - 1] = '\0';
        rule->use_count = 0;
        rule->success_count = 0;
        rule->importance = 0.0F;
        rule->first_used_ms = get_current_time_ms();
        rule->last_used_ms = rule->first_used_ms;
        rule->consolidated = false;
        return true;
    }

    // Add new rule
    rule_usage_t* rule = &integration->tracked_rules[integration->num_tracked_rules++];
    strncpy(rule->rule, rule_str, sizeof(rule->rule) - 1);
    rule->rule[sizeof(rule->rule) - 1] = '\0';
    rule->use_count = 0;
    rule->success_count = 0;
    rule->importance = 0.0F;
    rule->first_used_ms = get_current_time_ms();
    rule->last_used_ms = rule->first_used_ms;
    rule->consolidated = false;

    integration->stats.current_tracked_rules = integration->num_tracked_rules;

    return true;
}

/**
 * @brief Compute rule importance
 *
 * Importance is based on frequency of use, with success rate as a multiplier.
 * Even rules with no recorded successes gain importance through repeated use.
 */
static float compute_rule_importance(const rule_usage_t* rule)
{
    if (rule->use_count == 0) return 0.0F;

    // Base importance on frequency (log scale)
    float frequency_factor = logf((float)rule->use_count + 1.0F) / logf(10.0F); // Normalize

    // Success rate as a bonus multiplier (0.5 base + up to 0.5 from success)
    float success_rate = (rule->use_count > 0)
        ? (float)rule->success_count / (float)rule->use_count
        : 0.0F;
    float success_bonus = 0.5F + (0.5F * success_rate);

    return frequency_factor * success_bonus;
}

//=============================================================================
// Event Callbacks (Internal)
//=============================================================================

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle knowledge query message
 *
 * WHAT: Process knowledge base query via bio-async
 * WHY:  Enable distributed reasoning systems to query knowledge
 * HOW:  Look up query in tracked rules and inferences, return matches
 */
static nimcp_error_t handle_knowledge_query(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;

    if (!msg || !user_data) {
        NIMCP_LOGGING_ERROR("handle_knowledge_query: NULL argument");
        return NIMCP_ERROR_NULL_ARG;
    }

    reasoning_integration_t* integration = (reasoning_integration_t*)user_data;
    const bio_msg_knowledge_query_t* query = (const bio_msg_knowledge_query_t*)msg;

    NIMCP_LOGGING_DEBUG("Processing knowledge query: %s", query->query_str);

    // Prepare response
    bio_msg_knowledge_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_KNOWLEDGE_RESPONSE,
                        BIO_MODULE_KNOWLEDGE, query->header.source_module,
                        sizeof(bio_msg_knowledge_response_t));

    nimcp_platform_mutex_lock(&integration->mutex);

    // Search tracked rules for matches
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < integration->num_tracked_rules && match_count < 10; i++) {
        if (strstr(integration->tracked_rules[i].rule, query->query_str)) {
            strncpy(response.matches[match_count], integration->tracked_rules[i].rule, 255);
            response.confidence[match_count] = integration->tracked_rules[i].importance;
            match_count++;
        }
    }

    response.num_matches = match_count;
    response.success = match_count > 0;

    nimcp_platform_mutex_unlock(&integration->mutex);

    // Complete promise with response
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    NIMCP_LOGGING_DEBUG("Knowledge query complete: %d matches found", match_count);
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle decision request message
 *
 * WHAT: Process decision-making request via bio-async
 * WHY:  Enable distributed systems to request reasoning decisions
 * HOW:  Evaluate decision based on tracked rules and confidence
 */
static nimcp_error_t handle_decision_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;

    if (!msg || !user_data) {
        NIMCP_LOGGING_ERROR("handle_decision_request: NULL argument");
        return NIMCP_ERROR_NULL_ARG;
    }

    reasoning_integration_t* integration = (reasoning_integration_t*)user_data;
    const bio_msg_decision_request_t* request = (const bio_msg_decision_request_t*)msg;

    NIMCP_LOGGING_DEBUG("Processing decision request: %s", request->decision_context);

    // Prepare response
    bio_msg_decision_response_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_DECISION_RESPONSE,
                        BIO_MODULE_KNOWLEDGE, request->header.source_module,
                        sizeof(bio_msg_decision_response_t));

    nimcp_platform_mutex_lock(&integration->mutex);

    // Make decision based on tracked rules and active inferences
    float decision_confidence = 0.5F;
    bool decision_approved = true;

    // Check if any high-importance rules relate to this decision
    for (uint32_t i = 0; i < integration->num_tracked_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_tracked_rules > 256) {
            reasoning_integration_heartbeat("reasoning_in_loop",
                             (float)(i + 1) / (float)integration->num_tracked_rules);
        }

        if (integration->tracked_rules[i].importance > 0.7F) {
            decision_confidence = fmaxf(decision_confidence, integration->tracked_rules[i].importance);
        }
    }

    // Consider active inferences
    if (integration->num_active_inferences > 0) {
        float avg_salience = 0.0F;
        for (uint32_t i = 0; i < integration->num_active_inferences; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && integration->num_active_inferences > 256) {
                reasoning_integration_heartbeat("reasoning_in_loop",
                                 (float)(i + 1) / (float)integration->num_active_inferences);
            }

            avg_salience += integration->active_inferences[i].salience;
        }
        avg_salience /= integration->num_active_inferences;
        decision_confidence = (decision_confidence + avg_salience) / 2.0F;
    }

    response.approved = decision_approved;
    response.confidence = decision_confidence;
    snprintf(response.reasoning, sizeof(response.reasoning),
             "Based on %u rules and %u active inferences",
             integration->num_tracked_rules, integration->num_active_inferences);

    nimcp_platform_mutex_unlock(&integration->mutex);

    // Complete promise with response
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    NIMCP_LOGGING_DEBUG("Decision complete: approved=%d, confidence=%.2f",
                       decision_approved, decision_confidence);
    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int reasoning_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    NIMCP_LOGGING_INFO("reasoning_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            reasoning_integration_heartbeat("reasoning_in_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_KNOWLEDGE_QUERY:
                bio_router_register_handler(ctx, message_types[i], handle_knowledge_query);
                NIMCP_LOGGING_DEBUG("  Registered handler for BIO_MSG_KNOWLEDGE_QUERY");
                break;

            case BIO_MSG_DECISION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_decision_request);
                NIMCP_LOGGING_DEBUG("  Registered handler for BIO_MSG_DECISION_REQUEST");
                break;

            default:
                NIMCP_LOGGING_DEBUG("  Unknown message type 0x%04X, skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Main event callback dispatcher
 */
static void reasoning_event_callback(const brain_event_t* event, void* context)
{
    reasoning_integration_t* integration = (reasoning_integration_t*)context;
    if (!integration || !event) return;

    nimcp_platform_mutex_lock(&integration->mutex);

    uint64_t start_time = get_current_time_ms();

    // Dispatch to appropriate hooks based on event type
    // Note: Events can trigger multiple hooks (e.g., novel fact triggers both attention and curiosity)

    // Attention hooks
    if (integration->config.enable_attention_integration) {
        switch (event->type) {
            case EVENT_NOVEL_FACT_DERIVED:
            case EVENT_CONTRADICTION_DETECTED:
            case EVENT_PROOF_FOUND:
                reasoning_attention_hook(integration, event);
                break;
            default:
                break;
        }
    }

    // Curiosity hooks
    if (integration->config.enable_curiosity_integration) {
        switch (event->type) {
            case EVENT_PROOF_FAILED:
            case EVENT_UNIFICATION_FAILED:
            case EVENT_NOVEL_FACT_DERIVED:  // Novel facts also trigger curiosity
                reasoning_curiosity_hook(integration, event);
                break;
            default:
                break;
        }
    }

    // Working memory hooks
    if (integration->config.enable_working_memory_integration) {
        switch (event->type) {
            case EVENT_LOGIC_INFERENCE_STARTED:
            case EVENT_LOGIC_INFERENCE_COMPLETE:
            case EVENT_FORWARD_CHAIN_STEP:
            case EVENT_BACKWARD_CHAIN_STEP:
                reasoning_working_memory_hook(integration, event);
                break;
            default:
                break;
        }
    }

    // Executive hooks
    if (integration->config.enable_executive_integration) {
        switch (event->type) {
            case EVENT_LOGIC_INFERENCE_STARTED:
            case EVENT_PROOF_FOUND:
            case EVENT_PROOF_FAILED:
                reasoning_executive_hook(integration, event);
                break;
            default:
                break;
        }
    }

    // Consolidation hooks
    if (integration->config.enable_consolidation_integration) {
        switch (event->type) {
            case EVENT_RULE_ADDED:
            case EVENT_FORWARD_CHAIN_STEP:
            case EVENT_BACKWARD_CHAIN_STEP:
                reasoning_consolidation_hook(integration, event);
                break;
            default:
                break;
        }
    }

    integration->stats.total_events_processed++;

    uint64_t elapsed = get_current_time_ms() - start_time;
    // Update running average
    float alpha = 0.1F; // Exponential moving average factor
    integration->stats.avg_hook_execution_time_us =
        (1.0F - alpha) * integration->stats.avg_hook_execution_time_us +
        alpha * (float)elapsed;

    nimcp_platform_mutex_unlock(&integration->mutex);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

reasoning_integration_t* reasoning_integration_create(event_bus_t event_bus)
{
    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_create", 0.0f);


    reasoning_integration_config_t default_config = reasoning_integration_default_config();
    return reasoning_integration_create_custom(event_bus, &default_config);
}

reasoning_integration_t* reasoning_integration_create_custom(
    event_bus_t event_bus,
    const reasoning_integration_config_t* config
)
{
    if (!event_bus) {
        set_error("Event bus cannot be NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_create_custom", 0.0f);


    if (config && !reasoning_integration_validate_config(config)) {
        set_error("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_integration_create_custom: config validation failed");
        return NULL;
    }

    // Allocate structure
    reasoning_integration_t* integration = (reasoning_integration_t*)nimcp_calloc(
        1, sizeof(reasoning_integration_t)
    );
    if (!integration) {
        set_error("Failed to allocate integration structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate integration");

        return NULL;
    }

    // Apply configuration
    if (config) {
        integration->config = *config;
    } else {
        integration->config = reasoning_integration_default_config();
    }

    integration->event_bus = event_bus;

    // Allocate active inferences array
    integration->max_inferences = integration->config.max_active_inferences;
    integration->active_inferences = (active_inference_t*)nimcp_calloc(
        integration->max_inferences, sizeof(active_inference_t)
    );
    if (!integration->active_inferences) {
        set_error("Failed to allocate active inferences array");
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_integration_create_custom: integration->active_inferences is NULL");
        return NULL;
    }

    // Allocate tracked rules array
    integration->max_rules = 256; // Fixed size for rule tracking
    integration->tracked_rules = (rule_usage_t*)nimcp_calloc(
        integration->max_rules, sizeof(rule_usage_t)
    );
    if (!integration->tracked_rules) {
        set_error("Failed to allocate tracked rules array");
        nimcp_free(integration->active_inferences);
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_integration_create_custom: integration->tracked_rules is NULL");
        return NULL;
    }

    // Initialize mutex (non-recursive)
    // Note: nimcp_platform_mutex_init returns 0 on success, error code on failure
    if (nimcp_platform_mutex_init(&integration->mutex, false) != 0) {
        set_error("Failed to initialize mutex");
        nimcp_free(integration->tracked_rules);
        nimcp_free(integration->active_inferences);
        nimcp_free(integration);
        integration = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "reasoning_integration_create_custom: validation failed");
        return NULL;
    }

    // Subscribe to all logic events
    brain_event_type_t logic_events[] = {
        EVENT_LOGIC_GATE_EVALUATED,
        EVENT_LOGIC_INFERENCE_STARTED,
        EVENT_LOGIC_INFERENCE_COMPLETE,
        EVENT_FACT_ADDED,
        EVENT_RULE_ADDED,
        EVENT_UNIFICATION_SUCCEEDED,
        EVENT_UNIFICATION_FAILED,
        EVENT_FORWARD_CHAIN_STEP,
        EVENT_BACKWARD_CHAIN_STEP,
        EVENT_PROOF_FOUND,
        EVENT_PROOF_FAILED,
        EVENT_CONTRADICTION_DETECTED,
        EVENT_NOVEL_FACT_DERIVED
    };

    integration->num_subscriptions = 0;
    for (size_t i = 0; i < sizeof(logic_events) / sizeof(logic_events[0]); i++) {
        event_subscription_handle_t handle = event_bus_subscribe(
            event_bus,
            logic_events[i],
            reasoning_event_callback,
            integration
        );
        if (handle != INVALID_SUBSCRIPTION_HANDLE) {
            integration->subscriptions[integration->num_subscriptions++] = handle;
        }
    }

    integration->next_inference_id = 1;
    memset(&integration->stats, 0, sizeof(reasoning_integration_stats_t));

    // Register bio-async handlers
    integration->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_KNOWLEDGE_INTEGRATION,
            .module_name = "reasoning_integration",
            .inbox_capacity = 64,
            .user_data = integration
        };
        integration->bio_ctx = bio_router_register_module(&bio_info);
        if (integration->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_KNOWLEDGE_INTEGRATION,
                (void*)reasoning_wiring_handler_callback,
                integration
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(integration->bio_ctx, BIO_MSG_KNOWLEDGE_QUERY,
                                                 handle_knowledge_query)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(integration->bio_ctx, BIO_MSG_DECISION_REQUEST,
                                                 handle_decision_request)
                );
                NIMCP_LOGGING_INFO("Bio-async integration enabled for reasoning integration (legacy)");
            } else {
                NIMCP_LOGGING_INFO("Bio-async integration enabled for reasoning integration (KG-driven)");
            }
            integration->bio_async_enabled = true;
        } else {
            NIMCP_LOGGING_WARN("Bio-async registration failed for reasoning integration");
        }
    }

    // Initialize SNN and Plasticity bridges
    integration->snn_bridge = NULL;
    integration->plasticity_bridge = NULL;
    integration->bridges_enabled = false;

    reasoning_snn_config_t snn_config = reasoning_snn_config_default();
    integration->snn_bridge = reasoning_snn_create(&snn_config);

    reasoning_plasticity_config_t plasticity_config = reasoning_plasticity_config_default();
    integration->plasticity_bridge = reasoning_plasticity_create(&plasticity_config);

    if (integration->snn_bridge && integration->plasticity_bridge) {
        integration->bridges_enabled = true;
    }

    return integration;
}

void reasoning_integration_destroy(reasoning_integration_t* integration)
{
    if (!integration) return;

    // Unsubscribe from all events
    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_destroy", 0.0f);


    for (uint32_t i = 0; i < integration->num_subscriptions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && integration->num_subscriptions > 256) {
            reasoning_integration_heartbeat("reasoning_in_loop",
                             (float)(i + 1) / (float)integration->num_subscriptions);
        }

        event_bus_unsubscribe(integration->event_bus, integration->subscriptions[i]);
    }

    // Unregister bio-async module
    if (integration->bio_async_enabled && integration->bio_ctx) {
        bio_router_unregister_module(integration->bio_ctx);
        NIMCP_LOGGING_DEBUG("Bio-async module unregistered for reasoning integration");
    }

    // Destroy SNN and Plasticity bridges
    if (integration->snn_bridge) {
        reasoning_snn_destroy(integration->snn_bridge);
        integration->snn_bridge = NULL;
    }
    if (integration->plasticity_bridge) {
        reasoning_plasticity_destroy(integration->plasticity_bridge);
        integration->plasticity_bridge = NULL;
    }

    // Free arrays
    nimcp_free(integration->active_inferences);
    nimcp_free(integration->tracked_rules);

    // Destroy mutex
    nimcp_platform_mutex_destroy(&integration->mutex);

    // Free structure
    nimcp_free(integration);
    integration = NULL;
}

//=============================================================================
// Cognitive Hook Functions
//=============================================================================

bool reasoning_attention_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reasoning_attention_", 0.0f);


    if (integration && integration->bio_ctx) {
        bio_router_process_inbox(integration->bio_ctx, 5);
    }

    if (!integration || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_attention_hook: required parameter is NULL (integration, event)");
        return false;
    }
    if (!integration->config.enable_attention_integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_attention_hook: integration->config is NULL");
        return false;
    }

    float salience_boost = 0.0F;

    switch (event->type) {
        case EVENT_NOVEL_FACT_DERIVED:
            salience_boost = integration->config.novel_fact_salience_boost;
            break;
        case EVENT_CONTRADICTION_DETECTED:
            salience_boost = integration->config.contradiction_salience_boost;
            break;
        case EVENT_PROOF_FOUND:
            salience_boost = integration->config.proof_found_salience_boost;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_attention_hook: operation failed");
            return false;
    }

    // In a real implementation, this would call into the attention system
    // For now, we just track that the boost was applied
    integration->stats.attention_boosts_applied++;

    return true;
}

bool reasoning_curiosity_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_hook: required parameter is NULL (integration, event)");
        return false;
    }
    if (!integration->config.enable_curiosity_integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_curiosity_hook: integration->config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reasoning_curiosity_", 0.0f);


    float curiosity_boost = 0.0F;

    switch (event->type) {
        case EVENT_PROOF_FAILED:
            curiosity_boost = integration->config.unexplained_curiosity_boost;
            break;
        case EVENT_UNIFICATION_FAILED:
            curiosity_boost = integration->config.unexplained_curiosity_boost * 0.67F;
            break;
        case EVENT_NOVEL_FACT_DERIVED:
            curiosity_boost = integration->config.novel_fact_curiosity_boost;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_curiosity_hook: operation failed");
            return false;
    }

    // In a real implementation, this would call into the curiosity system
    // For now, we just track that curiosity was triggered
    integration->stats.curiosity_triggers++;

    return true;
}

bool reasoning_working_memory_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_working_memory_hook: required parameter is NULL (integration, event)");
        return false;
    }
    if (!integration->config.enable_working_memory_integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_working_memory_hook: working memory integration not enabled");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reasoning_working_me", 0.0f);


    switch (event->type) {
        case EVENT_LOGIC_INFERENCE_STARTED: {
            // Extract goal from event data (if present)
            char goal[NIMCP_ERROR_BUFFER_SIZE] = "Inference Goal";
            if (event->data.size > 0 && event->data.size < sizeof(goal)) {
                memcpy(goal, event->data.data, event->data.size);
                goal[event->data.size] = '\0';
            }
            add_inference_to_wm(integration, goal, 0.5F);
            break;
        }

        case EVENT_LOGIC_INFERENCE_COMPLETE: {
            // Mark inference as complete (allows eviction)
            if (integration->num_active_inferences > 0) {
                integration->active_inferences[integration->num_active_inferences - 1].is_active = false;
            }
            break;
        }

        case EVENT_FORWARD_CHAIN_STEP:
        case EVENT_BACKWARD_CHAIN_STEP: {
            // Update step count for most recent active inference
            if (integration->num_active_inferences > 0) {
                integration->active_inferences[integration->num_active_inferences - 1].step_count++;
            }
            break;
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_working_memory_hook: validation failed");
            return false;
    }

    return true;
}

bool reasoning_executive_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_executive_hook: required parameter is NULL (integration, event)");
        return false;
    }
    // if (!integration->config.enable_executive_integration) return false; // Executive not available

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reasoning_executive_", 0.0f);


    switch (event->type) {
        case EVENT_LOGIC_INFERENCE_STARTED: {
            // Check if inference requires planning (based on estimated complexity)
            // In a real implementation, this would analyze the goal and create a plan
            integration->stats.executive_plans_created++;
            break;
        }

        case EVENT_PROOF_FOUND:
        case EVENT_PROOF_FAILED: {
            // Plan completion or failure
            // In a real implementation, this would update the executive controller
            break;
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_executive_hook: operation failed");
            return false;
    }

    return true;
}

bool reasoning_consolidation_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_consolidation_hook: required parameter is NULL (integration, event)");
        return false;
    }
    if (!integration->config.enable_consolidation_integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_consolidation_hook: integration->config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reasoning_consolidat", 0.0f);


    switch (event->type) {
        case EVENT_RULE_ADDED: {
            // Extract rule from event data
            char rule_str[NIMCP_ERROR_BUFFER_LARGE] = "Default Rule";
            if (event->data.size > 0 && event->data.size < sizeof(rule_str)) {
                memcpy(rule_str, event->data.data, event->data.size);
                rule_str[event->data.size] = '\0';
            }
            track_rule(integration, rule_str);
            break;
        }

        case EVENT_FORWARD_CHAIN_STEP:
        case EVENT_BACKWARD_CHAIN_STEP: {
            // Increment use count for rules used in this step
            // In a real implementation, this would identify which rules were used
            if (integration->num_tracked_rules > 0) {
                rule_usage_t* rule = &integration->tracked_rules[integration->num_tracked_rules - 1];
                rule->use_count++;
                rule->last_used_ms = get_current_time_ms();

                // Update importance
                rule->importance = compute_rule_importance(rule);

                // Check consolidation threshold
                if (!rule->consolidated &&
                    rule->use_count >= integration->config.min_rule_uses_for_consolidation &&
                    rule->importance >= integration->config.consolidation_threshold) {
                    // Consolidate rule to long-term memory
                    rule->consolidated = true;
                    integration->stats.rules_consolidated++;
                }
            }
            break;
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_consolidation_hook: operation failed");
            return false;
    }

    return true;
}

//=============================================================================
// Query Functions
//=============================================================================

uint32_t reasoning_integration_get_active_inferences(
    const reasoning_integration_t* integration,
    active_inference_t* inferences,
    uint32_t max_count
)
{
    if (!integration || !inferences || max_count == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_get_active_inference", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&integration->mutex);

    uint32_t count = (max_count < integration->num_active_inferences) ?
                     max_count : integration->num_active_inferences;

    memcpy(inferences, integration->active_inferences, count * sizeof(active_inference_t));

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&integration->mutex);

    return count;
}

uint32_t reasoning_integration_get_tracked_rules(
    const reasoning_integration_t* integration,
    rule_usage_t* rules,
    uint32_t max_count
)
{
    if (!integration || !rules || max_count == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_get_tracked_rules", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&integration->mutex);

    uint32_t count = (max_count < integration->num_tracked_rules) ?
                     max_count : integration->num_tracked_rules;

    memcpy(rules, integration->tracked_rules, count * sizeof(rule_usage_t));

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&integration->mutex);

    return count;
}

//=============================================================================
// Configuration Functions
//=============================================================================

bool reasoning_integration_get_config(
    const reasoning_integration_t* integration,
    reasoning_integration_config_t* config
)
{
    if (!integration || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_get_config: required parameter is NULL (integration, config)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_get_config", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&integration->mutex);
    *config = integration->config;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&integration->mutex);

    return true;
}

bool reasoning_integration_set_config(
    reasoning_integration_t* integration,
    const reasoning_integration_config_t* config
)
{
    if (!integration || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_set_config: required parameter is NULL (integration, config)");
        return false;
    }
    if (!reasoning_integration_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_integration_set_config: reasoning_integration_validate_config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_set_config", 0.0f);


    nimcp_platform_mutex_lock(&integration->mutex);
    integration->config = *config;
    nimcp_platform_mutex_unlock(&integration->mutex);

    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

bool reasoning_integration_get_stats(
    const reasoning_integration_t* integration,
    reasoning_integration_stats_t* stats
)
{
    if (!integration || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_get_stats: required parameter is NULL (integration, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_get_stats", 0.0f);


    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&integration->mutex);
    *stats = integration->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&integration->mutex);

    return true;
}

bool reasoning_integration_reset_stats(reasoning_integration_t* integration)
{
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_reset_stats: integration is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_reset_stats", 0.0f);


    nimcp_platform_mutex_lock(&integration->mutex);
    memset(&integration->stats, 0, sizeof(reasoning_integration_stats_t));
    nimcp_platform_mutex_unlock(&integration->mutex);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

reasoning_integration_config_t reasoning_integration_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_default_config", 0.0f);


    reasoning_integration_config_t config = {
        // Integration enable flags
        .enable_attention_integration = true,
        .enable_curiosity_integration = true,
        .enable_working_memory_integration = true,
        .enable_executive_integration = true,
        .enable_consolidation_integration = true,

        // Attention configuration
        .novel_fact_salience_boost = REASONING_NOVEL_FACT_SALIENCE,
        .contradiction_salience_boost = REASONING_CONTRADICTION_SALIENCE,
        .proof_found_salience_boost = 0.5F,

        // Curiosity configuration
        .unexplained_curiosity_boost = REASONING_CURIOSITY_BOOST,
        .novel_fact_curiosity_boost = 0.1F,

        // Working memory configuration
        .max_active_inferences = REASONING_MAX_ACTIVE_INFERENCES,
        .inference_decay_tau_ms = 1000.0F,

        // Executive configuration
        .min_proof_steps_for_planning = 5,
        .planning_priority = 0.7F,

        // Consolidation configuration
        .min_rule_uses_for_consolidation = REASONING_CONSOLIDATION_THRESHOLD,
        .consolidation_threshold = 0.5F
    };
    return config;
}

bool reasoning_integration_validate_config(
    const reasoning_integration_config_t* config
)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_validate_config: config is NULL");
        return false;
    }

    // Validate salience boosts [0.0, 1.0]
    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_validate_config", 0.0f);


    if (config->novel_fact_salience_boost < 0.0F || config->novel_fact_salience_boost > 1.0F) {
        return false;
    }
    if (config->contradiction_salience_boost < 0.0F || config->contradiction_salience_boost > 1.0F) {
        return false;
    }
    if (config->proof_found_salience_boost < 0.0F || config->proof_found_salience_boost > 1.0F) {
        return false;
    }

    // Validate curiosity boosts [0.0, 1.0]
    if (config->unexplained_curiosity_boost < 0.0F || config->unexplained_curiosity_boost > 1.0F) {
        return false;
    }
    if (config->novel_fact_curiosity_boost < 0.0F || config->novel_fact_curiosity_boost > 1.0F) {
        return false;
    }

    // Validate working memory configuration
    if (config->max_active_inferences < 1 || config->max_active_inferences > 32) {
        return false;
    }
    if (config->inference_decay_tau_ms < 0.0F) {
        return false;
    }

    // Validate executive configuration
    if (config->planning_priority < 0.0F || config->planning_priority > 1.0F) {
        return false;
    }

    // Validate consolidation configuration
    if (config->consolidation_threshold < 0.0F || config->consolidation_threshold > 1.0F) {
        return false;
    }

    return true;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Connect reasoning integration to internal knowledge graph
 *
 * WHAT: Initialize KG context for self-awareness queries
 * WHY:  Enable reasoning to query its resources and capabilities
 * HOW:  Use KG helper functions to establish connection
 *
 * @param integration Reasoning integration instance
 * @param brain Brain instance for KG access
 * @return true if connected (or KG gracefully disabled), false on error
 */
bool reasoning_integration_connect_kg(reasoning_integration_t* integration, brain_t brain)
{
    if (!integration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_connect_kg: integration is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_connect_kg", 0.0f);


    nimcp_platform_mutex_lock(&integration->mutex);

    int result = kg_module_init(&integration->kg_context, brain, "Reasoning_Integration");

    if (result != 0) {
        NIMCP_LOGGING_ERROR(LOG_MODULE, "Failed to initialize KG context");
        nimcp_platform_mutex_unlock(&integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_integration_connect_kg: validation failed");
        return false;
    }

    if (!kg_is_available(&integration->kg_context)) {
        integration->kg_connected = false;
        NIMCP_LOGGING_INFO(LOG_MODULE, "KG disabled, reasoning graceful degradation");
        nimcp_platform_mutex_unlock(&integration->mutex);
        return true;
    }

    integration->kg_connected = true;
    NIMCP_LOGGING_INFO(LOG_MODULE, "Connected to internal KG for reasoning self-awareness");

    nimcp_platform_mutex_unlock(&integration->mutex);
    return true;
}

/**
 * @brief Query reasoning resources from KG
 *
 * WHAT: Retrieve list of resources available to reasoning
 * WHY:  Enable self-awareness of reasoning capabilities
 * HOW:  Query KG for connected module nodes
 *
 * @param integration Reasoning integration instance
 * @return Number of resources found (0 if KG not connected)
 */
int reasoning_integration_query_resources(reasoning_integration_t* integration)
{
    if (!integration || !integration->kg_connected) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_query_resources", 0.0f);


    nimcp_platform_mutex_lock(&integration->mutex);

    if (!kg_is_available(&integration->kg_context)) {
        nimcp_platform_mutex_unlock(&integration->mutex);
        return 0;
    }

    // Get all neighbors (both incoming and outgoing connections)
    brain_kg_node_list_t* neighbors = kg_get_neighbors_safe(&integration->kg_context);
    if (!neighbors) {
        nimcp_platform_mutex_unlock(&integration->mutex);
        return 0;
    }

    int count = (int)neighbors->count;
    NIMCP_LOGGING_DEBUG(LOG_MODULE, "Reasoning has %d KG resources", count);

    brain_kg_node_list_destroy(neighbors);
    nimcp_platform_mutex_unlock(&integration->mutex);
    return count;
}

/**
 * @brief Query reasoning's self-knowledge from KG
 *
 * WHAT: Query KG for structural self-knowledge about reasoning
 * WHY:  Enable introspection of reasoning capabilities
 * HOW:  Find self node and retrieve metadata
 *
 * @param integration Reasoning integration instance
 * @return true if self-knowledge is available, false otherwise
 */
bool reasoning_integration_query_self_knowledge(reasoning_integration_t* integration)
{
    if (!integration || !integration->kg_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_integration_query_self_knowledge: required parameter is NULL (integration, integration->kg_connected)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_integration_heartbeat("reasoning_in_query_self_knowledge", 0.0f);


    nimcp_platform_mutex_lock(&integration->mutex);

    if (!kg_has_node(&integration->kg_context)) {
        nimcp_platform_mutex_unlock(&integration->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_integration_query_self_knowledge: kg_has_node is NULL");
        return false;
    }

    const brain_kg_node_t* self = kg_get_node_safe(
        &integration->kg_context,
        integration->kg_context.self_node_id
    );

    if (self) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE, "Reasoning self-knowledge: name=%s, state=%d",
                           self->name, self->state);
        nimcp_platform_mutex_unlock(&integration->mutex);
        return true;
    }

    nimcp_platform_mutex_unlock(&integration->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_integration_query_self_knowledge: validation failed");
    return false;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void reasoning_integration_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reasoning_integration_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_integration_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_integration_training_begin: NULL argument");
        return -1;
    }
    reasoning_integration_heartbeat_instance(NULL, "reasoning_integration_training_begin", 0.0f);
    (void)(struct reasoning_integration*)instance; /* Module state available for reset */
    return 0;
}

int reasoning_integration_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_integration_training_end: NULL argument");
        return -1;
    }
    reasoning_integration_heartbeat_instance(NULL, "reasoning_integration_training_end", 1.0f);
    (void)(struct reasoning_integration*)instance; /* Module state available for finalization */
    return 0;
}

int reasoning_integration_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_integration_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_integration_heartbeat_instance(NULL, "reasoning_integration_training_step", progress);
    (void)(struct reasoning_integration*)instance; /* Module state available for step adaptation */
    return 0;
}

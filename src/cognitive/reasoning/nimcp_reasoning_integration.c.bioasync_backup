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
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
};

//=============================================================================
// Error Handling
//=============================================================================

static __thread char last_error[256] = {0};

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
        rule->importance = 0.0f;
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
    rule->importance = 0.0f;
    rule->first_used_ms = get_current_time_ms();
    rule->last_used_ms = rule->first_used_ms;
    rule->consolidated = false;

    integration->stats.current_tracked_rules = integration->num_tracked_rules;

    return true;
}

/**
 * @brief Compute rule importance
 */
static float compute_rule_importance(const rule_usage_t* rule)
{
    if (rule->use_count == 0) return 0.0f;

    float success_rate = (float)rule->success_count / (float)rule->use_count;
    float frequency_factor = logf((float)rule->use_count + 1.0f);
    return success_rate * frequency_factor;
}

//=============================================================================
// Event Callbacks (Internal)
//=============================================================================

/**
 * @brief Main event callback dispatcher
 */
static void reasoning_event_callback(const brain_event_t* event, void* context)
{
    reasoning_integration_t* integration = (reasoning_integration_t*)context;
    if (!integration || !event) return;

    nimcp_platform_mutex_lock(&integration->mutex);

    uint64_t start_time = get_current_time_ms();

    // Dispatch to appropriate hook based on event type
    switch (event->type) {
        // Attention hooks
        case EVENT_NOVEL_FACT_DERIVED:
        case EVENT_CONTRADICTION_DETECTED:
        case EVENT_PROOF_FOUND:
            if (integration->config.enable_attention_integration) {
                reasoning_attention_hook(integration, event);
            }
            break;

        // Curiosity hooks
        case EVENT_PROOF_FAILED:
        case EVENT_UNIFICATION_FAILED:
            if (integration->config.enable_curiosity_integration) {
                reasoning_curiosity_hook(integration, event);
            }
            break;

        // Working memory hooks
        case EVENT_LOGIC_INFERENCE_STARTED:
        case EVENT_LOGIC_INFERENCE_COMPLETE:
        case EVENT_FORWARD_CHAIN_STEP:
        case EVENT_BACKWARD_CHAIN_STEP:
            if (integration->config.enable_working_memory_integration) {
                reasoning_working_memory_hook(integration, event);
            }
            break;

        // Executive hooks
        case EVENT_LOGIC_INFERENCE_STARTED:
        case EVENT_PROOF_FOUND:
        case EVENT_PROOF_FAILED:
            if (integration->config.enable_executive_integration) {
                reasoning_executive_hook(integration, event);
            }
            break;

        // Consolidation hooks
        case EVENT_RULE_ADDED:
        case EVENT_FORWARD_CHAIN_STEP:
        case EVENT_BACKWARD_CHAIN_STEP:
            if (integration->config.enable_consolidation_integration) {
                reasoning_consolidation_hook(integration, event);
            }
            break;

        default:
            break;
    }

    integration->stats.total_events_processed++;

    uint64_t elapsed = get_current_time_ms() - start_time;
    // Update running average
    float alpha = 0.1f; // Exponential moving average factor
    integration->stats.avg_hook_execution_time_us =
        (1.0f - alpha) * integration->stats.avg_hook_execution_time_us +
        alpha * (float)elapsed;

    nimcp_platform_mutex_unlock(&integration->mutex);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

reasoning_integration_t* reasoning_integration_create(event_bus_t event_bus)
{
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
        return NULL;
    }

    if (config && !reasoning_integration_validate_config(config)) {
        set_error("Invalid configuration");
        return NULL;
    }

    // Allocate structure
    reasoning_integration_t* integration = (reasoning_integration_t*)nimcp_calloc(
        1, sizeof(reasoning_integration_t)
    );
    if (!integration) {
        set_error("Failed to allocate integration structure");
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
        return NULL;
    }

    // Initialize mutex
    if (!nimcp_platform_mutex_init(&integration->mutex)) {
        set_error("Failed to initialize mutex");
        nimcp_free(integration->tracked_rules);
        nimcp_free(integration->active_inferences);
        nimcp_free(integration);
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

    return integration;
}

void reasoning_integration_destroy(reasoning_integration_t* integration)
{
    if (!integration) return;

    // Unsubscribe from all events
    for (uint32_t i = 0; i < integration->num_subscriptions; i++) {
        event_bus_unsubscribe(integration->event_bus, integration->subscriptions[i]);
    }

    // Free arrays
    nimcp_free(integration->active_inferences);
    nimcp_free(integration->tracked_rules);

    // Destroy mutex
    nimcp_platform_mutex_destroy(&integration->mutex);

    // Free structure
    nimcp_free(integration);
}

//=============================================================================
// Cognitive Hook Functions
//=============================================================================

bool reasoning_attention_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) return false;
    if (!integration->config.enable_attention_integration) return false;

    float salience_boost = 0.0f;

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
    if (!integration || !event) return false;
    if (!integration->config.enable_curiosity_integration) return false;

    float curiosity_boost = 0.0f;

    switch (event->type) {
        case EVENT_PROOF_FAILED:
            curiosity_boost = integration->config.unexplained_curiosity_boost;
            break;
        case EVENT_UNIFICATION_FAILED:
            curiosity_boost = integration->config.unexplained_curiosity_boost * 0.67f;
            break;
        case EVENT_NOVEL_FACT_DERIVED:
            curiosity_boost = integration->config.novel_fact_curiosity_boost;
            break;
        default:
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
    if (!integration || !event) return false;
    if (!integration->config.enable_working_memory_integration) return false;

    switch (event->type) {
        case EVENT_LOGIC_INFERENCE_STARTED: {
            // Extract goal from event data (if present)
            char goal[256] = "Inference Goal";
            if (event->data.size > 0 && event->data.size < sizeof(goal)) {
                memcpy(goal, event->data.data, event->data.size);
                goal[event->data.size] = '\0';
            }
            add_inference_to_wm(integration, goal, 0.5f);
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
            return false;
    }

    return true;
}

bool reasoning_executive_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) return false;
    if (!integration->config.enable_executive_integration) return false;

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
            return false;
    }

    return true;
}

bool reasoning_consolidation_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
)
{
    if (!integration || !event) return false;
    if (!integration->config.enable_consolidation_integration) return false;

    switch (event->type) {
        case EVENT_RULE_ADDED: {
            // Extract rule from event data
            char rule_str[512] = "Default Rule";
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
    if (!integration || !config) return false;

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
    if (!integration || !config) return false;
    if (!reasoning_integration_validate_config(config)) return false;

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
    if (!integration || !stats) return false;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&integration->mutex);
    *stats = integration->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&integration->mutex);

    return true;
}

bool reasoning_integration_reset_stats(reasoning_integration_t* integration)
{
    if (!integration) return false;

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
        .proof_found_salience_boost = 0.5f,

        // Curiosity configuration
        .unexplained_curiosity_boost = REASONING_CURIOSITY_BOOST,
        .novel_fact_curiosity_boost = 0.1f,

        // Working memory configuration
        .max_active_inferences = REASONING_MAX_ACTIVE_INFERENCES,
        .inference_decay_tau_ms = 1000.0f,

        // Executive configuration
        .min_proof_steps_for_planning = 5,
        .planning_priority = 0.7f,

        // Consolidation configuration
        .min_rule_uses_for_consolidation = REASONING_CONSOLIDATION_THRESHOLD,
        .consolidation_threshold = 0.5f
    };
    return config;
}

bool reasoning_integration_validate_config(
    const reasoning_integration_config_t* config
)
{
    if (!config) return false;

    // Validate salience boosts [0.0, 1.0]
    if (config->novel_fact_salience_boost < 0.0f || config->novel_fact_salience_boost > 1.0f)
        return false;
    if (config->contradiction_salience_boost < 0.0f || config->contradiction_salience_boost > 1.0f)
        return false;
    if (config->proof_found_salience_boost < 0.0f || config->proof_found_salience_boost > 1.0f)
        return false;

    // Validate curiosity boosts [0.0, 1.0]
    if (config->unexplained_curiosity_boost < 0.0f || config->unexplained_curiosity_boost > 1.0f)
        return false;
    if (config->novel_fact_curiosity_boost < 0.0f || config->novel_fact_curiosity_boost > 1.0f)
        return false;

    // Validate working memory configuration
    if (config->max_active_inferences < 1 || config->max_active_inferences > 32)
        return false;
    if (config->inference_decay_tau_ms < 0.0f)
        return false;

    // Validate executive configuration
    if (config->planning_priority < 0.0f || config->planning_priority > 1.0f)
        return false;

    // Validate consolidation configuration
    if (config->consolidation_threshold < 0.0f || config->consolidation_threshold > 1.0f)
        return false;

    return true;
}

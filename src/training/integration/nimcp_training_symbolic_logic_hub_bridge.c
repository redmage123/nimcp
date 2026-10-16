/**
 * @file nimcp_training_symbolic_logic_hub_bridge.c
 * @brief Implementation of Symbolic Logic - Training Hub Integration Bridge
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "training/integration/nimcp_training_symbolic_logic_hub_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_symbolic_logic_hub_bridge)

#define LOG_MODULE "TRAINING_SYMBOLIC_LOGIC_HUB_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Rule tracking entry for learning
 */
typedef struct {
    int rule_id;                             /**< Rule ID */
    uint64_t fired_time;                     /**< When rule fired */
    float loss_at_fire;                      /**< Loss when rule fired */
    bool outcome_recorded;                   /**< Whether outcome was recorded */
} rule_tracking_entry_t;

/**
 * @brief Internal bridge structure
 */
struct training_logic_hub_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    training_logic_hub_config_t config;

    /* Connections */
    training_integration_hub_t hub;          /**< Training hub connection */
    symbolic_logic_t* cognitive_logic;       /**< Optional cognitive logic */

    /* State */
    training_logic_hub_state_t state;
    training_logic_hub_stats_t stats;

    /* Rules */
    training_logic_rule_t* rules;            /**< Array of rules */
    uint32_t num_rules;                      /**< Number of rules */
    uint32_t max_rules;                      /**< Maximum rules */

    /* Rule tracking for learning */
    rule_tracking_entry_t* tracking;         /**< Recent rule fires */
    uint32_t tracking_head;                  /**< Circular buffer head */
    uint32_t tracking_count;                 /**< Current tracking entries */
    uint32_t max_tracking;                   /**< Maximum tracking entries */

    /* Internal state */
    bool initialized;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void handle_loss_event(training_logic_hub_bridge_t* bridge,
                              const training_event_data_t* event);
static void handle_gradient_event(training_logic_hub_bridge_t* bridge,
                                  const training_event_data_t* event);
static void handle_difficulty_event(training_logic_hub_bridge_t* bridge,
                                    const training_event_data_t* event);
static void handle_lr_event(training_logic_hub_bridge_t* bridge,
                            const training_event_data_t* event);
static void handle_epoch_event(training_logic_hub_bridge_t* bridge,
                               const training_event_data_t* event);
static void handle_validation_event(training_logic_hub_bridge_t* bridge,
                                    const training_event_data_t* event);

static bool evaluate_rule_condition(training_logic_hub_bridge_t* bridge,
                                    const training_logic_rule_t* rule);
static void track_rule_fire(training_logic_hub_bridge_t* bridge,
                            int rule_id, float loss);
static void update_rule_confidence(training_logic_hub_bridge_t* bridge,
                                   int rule_id, bool positive);
static uint64_t get_timestamp_us(void);

/* ============================================================================
 * Event Callback
 * ============================================================================ */

/**
 * @brief Training event callback
 */
static int training_event_callback(const training_event_data_t* event, void* user_data) {
    training_logic_hub_bridge_t* bridge = (training_logic_hub_bridge_t*)user_data;
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_event_callback: required parameter is NULL (bridge, event)");
        return -1;
    }

    bridge->stats.events_received++;

    switch (event->event_type) {
        case TRAINING_EVENT_LOSS_COMPUTED:
            handle_loss_event(bridge, event);
            break;
        case TRAINING_EVENT_GRADIENT_READY:
            handle_gradient_event(bridge, event);
            break;
        case TRAINING_EVENT_DIFFICULTY_UPDATED:
            handle_difficulty_event(bridge, event);
            break;
        case TRAINING_EVENT_LR_ADJUSTED:
            handle_lr_event(bridge, event);
            break;
        case TRAINING_EVENT_EPOCH_COMPLETE:
            handle_epoch_event(bridge, event);
            break;
        case TRAINING_EVENT_VALIDATION_COMPLETE:
            handle_validation_event(bridge, event);
            break;
        default:
            break;
    }

    bridge->state.last_event_time = get_timestamp_us();
    bridge->stats.events_processed++;
    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int training_logic_hub_default_config(training_logic_hub_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Event subscriptions */
    config->subscribe_loss_computed = true;
    config->subscribe_gradient_ready = true;
    config->subscribe_difficulty_updated = true;
    config->subscribe_lr_adjusted = true;
    config->subscribe_epoch_complete = true;
    config->subscribe_validation_complete = true;
    config->subscribe_task_switched = false;

    /* Event publishing */
    config->publish_rule_results = true;
    config->publish_constraint_violations = true;
    config->publish_recommendations = true;

    /* Rule learning */
    config->enable_rule_learning = true;
    config->rule_learning_rate = 0.1f;
    config->min_rule_confidence = 0.1f;

    /* Processing parameters */
    config->max_rules_per_event = 16;
    config->inference_timeout_ms = 10.0f;
    config->enable_async_inference = false;

    /* Integration */
    config->connect_to_cognitive_logic = false;
    config->share_knowledge_base = false;

    return 0;
}

training_logic_hub_bridge_t* training_logic_hub_create(
    const training_logic_hub_config_t* config)
{
    training_logic_hub_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        training_logic_hub_default_config(&bridge->config);
    }

    /* Allocate rules array */
    bridge->max_rules = TRAINING_LOGIC_MAX_RULES;
    bridge->rules = nimcp_calloc(bridge->max_rules, sizeof(training_logic_rule_t));
    if (!bridge->rules) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_logic_hub_create: bridge->rules is NULL");
        return NULL;
    }

    /* Allocate tracking array */
    bridge->max_tracking = TRAINING_LOGIC_MAX_RULE_TRACKING;
    bridge->tracking = nimcp_calloc(bridge->max_tracking, sizeof(rule_tracking_entry_t));
    if (!bridge->tracking) {
        nimcp_free(bridge->rules);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_logic_hub_create: bridge->tracking is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.is_registered = false;
    bridge->state.is_connected = false;
    bridge->state.is_active = false;
    bridge->state.active_rules = 0;

    /* Initialize metrics with defaults */
    bridge->state.current_metrics.loss_stable = true;
    bridge->state.current_metrics.grad_stable = true;
    bridge->state.current_metrics.difficulty = 0.5f;
    bridge->state.current_metrics.mastery = 0.0f;
    bridge->state.current_metrics.learning_rate = NIMCP_LEARNING_RATE_FINE;

    bridge->initialized = true;

    return bridge;
}

void training_logic_hub_destroy(training_logic_hub_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "training_symbolic_logic_hub");

    /* Disconnect if connected */
    if (bridge->state.is_connected) {
        training_logic_hub_disconnect(bridge);
    }

    /* Free resources */
    nimcp_free(bridge->tracking);
    nimcp_free(bridge->rules);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int training_logic_hub_connect(
    training_logic_hub_bridge_t* bridge,
    training_integration_hub_t hub)
{
    if (!bridge || !hub) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_connect: required parameter is NULL (bridge, hub)");
        return -1;
    }
    if (bridge->state.is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_logic_hub_connect: validation failed");
        return -1;
    }

    bridge->hub = hub;

    /* Register with hub */
    int result = training_hub_register_module(
        hub,
        TRAINING_LOGIC_MODULE_ID,
        TRAINING_CATEGORY_OPTIMIZATION,
        TRAINING_LOGIC_MODULE_NAME,
        bridge
    );
    if (result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_logic_hub_connect: validation failed");
        return -1;
    }

    bridge->state.is_registered = true;

    /* Subscribe to events based on configuration */
    if (bridge->config.subscribe_loss_computed) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_LOSS_COMPUTED,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_gradient_ready) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_GRADIENT_READY,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_difficulty_updated) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_DIFFICULTY_UPDATED,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_lr_adjusted) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_LR_ADJUSTED,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_epoch_complete) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_EPOCH_COMPLETE,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_validation_complete) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_VALIDATION_COMPLETE,
                               training_event_callback, bridge);
    }
    if (bridge->config.subscribe_task_switched) {
        training_hub_subscribe(hub, TRAINING_LOGIC_MODULE_ID,
                               TRAINING_EVENT_TASK_SWITCHED,
                               training_event_callback, bridge);
    }

    bridge->state.is_connected = true;
    bridge->state.is_active = true;

    return 0;
}

int training_logic_hub_disconnect(training_logic_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_disconnect: bridge is NULL");
        return -1;
    }
    if (!bridge->state.is_connected) return 0;

    /* Unregister from hub */
    if (bridge->hub && bridge->state.is_registered) {
        training_hub_unregister_module(bridge->hub, TRAINING_LOGIC_MODULE_ID);
    }

    bridge->hub = NULL;
    bridge->state.is_registered = false;
    bridge->state.is_connected = false;
    bridge->state.is_active = false;

    return 0;
}

int training_logic_hub_connect_cognitive_logic(
    training_logic_hub_bridge_t* bridge,
    symbolic_logic_t* logic)
{
    if (!bridge || !logic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_connect_cognitive_logic: required parameter is NULL (bridge, logic)");
        return -1;
    }

    bridge->cognitive_logic = logic;

    return 0;
}

/* ============================================================================
 * Rule Management Implementation
 * ============================================================================ */

int training_logic_hub_add_rule(
    training_logic_hub_bridge_t* bridge,
    const training_logic_rule_t* rule)
{
    if (!bridge || !rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_add_rule: required parameter is NULL (bridge, rule)");
        return -1;
    }
    if (bridge->num_rules >= bridge->max_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "training_logic_hub_add_rule: capacity exceeded");
        return -1;
    }

    /* Copy rule */
    int rule_id = (int)bridge->num_rules;
    bridge->rules[rule_id] = *rule;
    bridge->num_rules++;
    bridge->state.active_rules = bridge->num_rules;

    return rule_id;
}

int training_logic_hub_remove_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_remove_rule: bridge is NULL");
        return -1;
    }
    if (rule_id < 0 || (uint32_t)rule_id >= bridge->num_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_logic_hub_remove_rule: capacity exceeded");
        return -1;
    }

    /* Shift remaining rules */
    for (uint32_t i = (uint32_t)rule_id; i < bridge->num_rules - 1; i++) {
        bridge->rules[i] = bridge->rules[i + 1];
    }
    bridge->num_rules--;
    bridge->state.active_rules = bridge->num_rules;

    return 0;
}

int training_logic_hub_get_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id,
    training_logic_rule_t* rule)
{
    if (!bridge || !rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_get_rule: required parameter is NULL (bridge, rule)");
        return -1;
    }
    if (rule_id < 0 || (uint32_t)rule_id >= bridge->num_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_logic_hub_get_rule: capacity exceeded");
        return -1;
    }

    *rule = bridge->rules[rule_id];
    return 0;
}

int training_logic_hub_add_default_rules(training_logic_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_add_default_rules: bridge is NULL");
        return -1;
    }

    int rules_added = 0;
    training_logic_rule_t rule;

    /* Rule 1: LR safety - safe to increase LR if loss and gradients stable */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_LR_SAFETY;
    strncpy(rule.name, "safe_lr_increase", sizeof(rule.name) - 1);
    strncpy(rule.condition, "loss_stable AND grad_stable AND NOT grad_exploding",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.8f;
    rule.priority = 0.7f;
    rule.is_safety_critical = true;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 2: Gradient clipping - clip if gradient norm high */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_GRADIENT_CLIP;
    strncpy(rule.name, "should_clip_gradients", sizeof(rule.name) - 1);
    strncpy(rule.condition, "grad_exploding OR grad_norm > grad_norm_avg * 5",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.9f;
    rule.priority = 0.9f;
    rule.is_safety_critical = true;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 3: Early stopping - stop if no improvement for patience epochs */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_EARLY_STOP;
    strncpy(rule.name, "should_early_stop", sizeof(rule.name) - 1);
    strncpy(rule.condition, "epochs_since_improvement > 10 AND NOT validation_improving",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.7f;
    rule.priority = 0.6f;
    rule.is_safety_critical = false;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 4: Checkpoint trigger - checkpoint if validation improved */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_CHECKPOINT_TRIGGER;
    strncpy(rule.name, "should_checkpoint", sizeof(rule.name) - 1);
    strncpy(rule.condition, "validation_improving AND current_loss < best_loss",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.85f;
    rule.priority = 0.5f;
    rule.is_safety_critical = false;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 5: Difficulty progression - increase if mastered */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_CURRICULUM_PREREQUISITE;
    strncpy(rule.name, "allow_difficulty_increase", sizeof(rule.name) - 1);
    strncpy(rule.condition, "mastery > 0.8 AND performance > 0.7 AND difficulty < 1.0",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.75f;
    rule.priority = 0.6f;
    rule.is_safety_critical = false;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 6: Difficulty constraint - don't increase if struggling */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_DIFFICULTY_CONSTRAINT;
    strncpy(rule.name, "block_difficulty_increase", sizeof(rule.name) - 1);
    strncpy(rule.condition, "performance < 0.5 OR mastery < 0.6",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.8f;
    rule.priority = 0.8f;
    rule.is_safety_critical = true;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    /* Rule 7: Batch size adjustment safety */
    memset(&rule, 0, sizeof(rule));
    rule.type = TRAINING_RULE_BATCH_SIZE_ADJUST;
    strncpy(rule.name, "safe_batch_increase", sizeof(rule.name) - 1);
    strncpy(rule.condition, "loss_stable AND grad_stable AND loss_trend <= 0",
            sizeof(rule.condition) - 1);
    rule.confidence = 0.7f;
    rule.priority = 0.5f;
    rule.is_safety_critical = false;
    if (training_logic_hub_add_rule(bridge, &rule) >= 0) rules_added++;

    return rules_added;
}

/* ============================================================================
 * Metrics Update Implementation
 * ============================================================================ */

int training_logic_hub_update_metrics(
    training_logic_hub_bridge_t* bridge,
    const training_logic_metrics_t* metrics)
{
    if (!bridge || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_update_metrics: required parameter is NULL (bridge, metrics)");
        return -1;
    }

    bridge->state.current_metrics = *metrics;

    return 0;
}

/* ============================================================================
 * Rule Evaluation Implementation
 * ============================================================================ */

int training_logic_hub_evaluate_rules(
    training_logic_hub_bridge_t* bridge,
    training_rule_type_t type,
    training_rule_result_t* results,
    uint32_t max_results)
{
    if (!bridge || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_evaluate_rules: required parameter is NULL (bridge, results)");
        return -1;
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < bridge->num_rules && count < max_results; i++) {
        if (bridge->rules[i].type == type || type == TRAINING_RULE_CUSTOM) {
            bool satisfied = evaluate_rule_condition(bridge, &bridge->rules[i]);

            results[count].type = bridge->rules[i].type;
            results[count].satisfied = satisfied;
            results[count].confidence = bridge->rules[i].confidence;
            snprintf(results[count].explanation, sizeof(results[count].explanation),
                     "Rule '%s': %s", bridge->rules[i].name,
                     satisfied ? "satisfied" : "not satisfied");

            if (satisfied) {
                bridge->rules[i].times_fired++;
                bridge->rules[i].last_fired_time = get_timestamp_us();

                /* Track for learning */
                if (bridge->config.enable_rule_learning) {
                    track_rule_fire(bridge, (int)i,
                                    bridge->state.current_metrics.current_loss);
                }

                bridge->stats.rules_satisfied++;
            }

            bridge->stats.rules_evaluated++;
            count++;
        }
    }

    return (int)count;
}

int training_logic_hub_evaluate_rule(
    training_logic_hub_bridge_t* bridge,
    int rule_id,
    training_rule_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_evaluate_rule: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (rule_id < 0 || (uint32_t)rule_id >= bridge->num_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_logic_hub_evaluate_rule: capacity exceeded");
        return -1;
    }

    training_logic_rule_t* rule = &bridge->rules[rule_id];
    bool satisfied = evaluate_rule_condition(bridge, rule);

    result->type = rule->type;
    result->satisfied = satisfied;
    result->confidence = rule->confidence;
    snprintf(result->explanation, sizeof(result->explanation),
             "Rule '%s': %s", rule->name,
             satisfied ? "satisfied" : "not satisfied");

    if (satisfied) {
        rule->times_fired++;
        rule->last_fired_time = get_timestamp_us();

        if (bridge->config.enable_rule_learning) {
            track_rule_fire(bridge, rule_id,
                            bridge->state.current_metrics.current_loss);
        }

        bridge->stats.rules_satisfied++;
    }

    bridge->stats.rules_evaluated++;

    return 0;
}

bool training_logic_hub_is_action_safe(
    training_logic_hub_bridge_t* bridge,
    const char* action,
    float* confidence)
{
    if (!bridge || !action) {
        if (confidence) *confidence = 0.0f;
        return false;
    }

    /* Map action to rule type */
    training_rule_type_t type;
    if (strcmp(action, "increase_lr") == 0) {
        type = TRAINING_RULE_LR_SAFETY;
    } else if (strcmp(action, "increase_difficulty") == 0) {
        type = TRAINING_RULE_CURRICULUM_PREREQUISITE;
    } else if (strcmp(action, "increase_batch") == 0) {
        type = TRAINING_RULE_BATCH_SIZE_ADJUST;
    } else {
        /* Unknown action, assume safe */
        if (confidence) *confidence = 0.5f;
        return true;
    }

    /* Evaluate relevant rules */
    training_rule_result_t results[8];
    int count = training_logic_hub_evaluate_rules(bridge, type, results, 8);

    if (count <= 0) {
        if (confidence) *confidence = 0.5f;
        return true;
    }

    /* Check if any safety-critical rule blocks the action */
    float max_confidence = 0.0f;
    bool any_satisfied = false;
    bool any_blocking = false;

    for (int i = 0; i < count; i++) {
        if (results[i].satisfied) {
            any_satisfied = true;
            if (results[i].confidence > max_confidence) {
                max_confidence = results[i].confidence;
            }
        }

        /* Check for blocking rules (constraints) */
        if (type == TRAINING_RULE_CURRICULUM_PREREQUISITE) {
            /* Check difficulty constraint rules */
            training_rule_result_t constraint_results[4];
            int constraint_count = training_logic_hub_evaluate_rules(
                bridge, TRAINING_RULE_DIFFICULTY_CONSTRAINT,
                constraint_results, 4);
            for (int j = 0; j < constraint_count; j++) {
                if (constraint_results[j].satisfied) {
                    any_blocking = true;
                    break;
                }
            }
        }
    }

    if (confidence) *confidence = max_confidence;

    if (any_blocking) {
        bridge->stats.constraints_violated++;
        return false;
    }

    return any_satisfied;
}

/* ============================================================================
 * Rule Learning Implementation
 * ============================================================================ */

int training_logic_hub_report_outcome(
    training_logic_hub_bridge_t* bridge,
    bool loss_improved,
    bool validation_improved)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_report_outcome: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_rule_learning) return 0;

    float current_loss = bridge->state.current_metrics.current_loss;

    /* Update confidence for recently fired rules */
    for (uint32_t i = 0; i < bridge->tracking_count; i++) {
        uint32_t idx = (bridge->tracking_head + bridge->max_tracking - 1 - i)
                       % bridge->max_tracking;
        rule_tracking_entry_t* entry = &bridge->tracking[idx];

        if (entry->outcome_recorded) continue;

        /* Determine if this rule's action led to improvement */
        bool positive = (loss_improved && current_loss < entry->loss_at_fire) ||
                        validation_improved;

        update_rule_confidence(bridge, entry->rule_id, positive);
        entry->outcome_recorded = true;

        bridge->stats.rule_updates++;

        if (positive) {
            bridge->stats.predictions_correct++;
        } else {
            bridge->stats.predictions_incorrect++;
        }
    }

    /* Update prediction accuracy */
    uint64_t total = bridge->stats.predictions_correct +
                     bridge->stats.predictions_incorrect;
    if (total > 0) {
        bridge->stats.prediction_accuracy =
            (float)bridge->stats.predictions_correct / (float)total;
    }

    return 0;
}

float training_logic_hub_get_rule_confidence(
    training_logic_hub_bridge_t* bridge,
    int rule_id)
{
    if (!bridge) return -1.0f;
    if (rule_id < 0 || (uint32_t)rule_id >= bridge->num_rules) return -1.0f;

    return bridge->rules[rule_id].confidence;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int training_logic_hub_query_lr(
    training_logic_hub_bridge_t* bridge,
    float current_lr,
    float* suggested_lr,
    float* confidence)
{
    if (!bridge || !suggested_lr || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_query_lr: required parameter is NULL (bridge, suggested_lr, confidence)");
        return -1;
    }

    /* Evaluate LR safety rules */
    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results, 4);

    if (count <= 0) {
        *suggested_lr = current_lr;
        *confidence = 0.5f;
        return 0;
    }

    /* If safe to increase and training is going well */
    bool can_increase = false;
    float max_conf = 0.0f;

    for (int i = 0; i < count; i++) {
        if (results[i].satisfied && results[i].confidence > max_conf) {
            can_increase = true;
            max_conf = results[i].confidence;
        }
    }

    if (can_increase && bridge->state.current_metrics.loss_trend < 0) {
        /* Training improving, suggest small increase */
        *suggested_lr = current_lr * 1.1f;
        if (*suggested_lr > bridge->state.current_metrics.lr_max) {
            *suggested_lr = bridge->state.current_metrics.lr_max;
        }
    } else if (!bridge->state.current_metrics.loss_stable) {
        /* Training unstable, suggest decrease */
        *suggested_lr = current_lr * 0.5f;
        if (*suggested_lr < bridge->state.current_metrics.lr_min) {
            *suggested_lr = bridge->state.current_metrics.lr_min;
        }
    } else {
        *suggested_lr = current_lr;
    }

    *confidence = max_conf;
    bridge->stats.recommendations_made++;

    return 0;
}

int training_logic_hub_query_difficulty(
    training_logic_hub_bridge_t* bridge,
    float current_difficulty,
    float* suggested_difficulty,
    float* confidence)
{
    if (!bridge || !suggested_difficulty || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_query_difficulty: required parameter is NULL (bridge, suggested_difficulty, confidence)");
        return -1;
    }

    /* Check prerequisite rules */
    training_rule_result_t prereq_results[4];
    int prereq_count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_CURRICULUM_PREREQUISITE, prereq_results, 4);

    /* Check constraint rules */
    training_rule_result_t constraint_results[4];
    int constraint_count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_DIFFICULTY_CONSTRAINT, constraint_results, 4);

    /* Check if blocked by constraints */
    bool blocked = false;
    for (int i = 0; i < constraint_count; i++) {
        if (constraint_results[i].satisfied) {
            blocked = true;
            break;
        }
    }

    /* Check if prerequisites met */
    bool can_increase = false;
    float max_conf = 0.0f;

    for (int i = 0; i < prereq_count; i++) {
        if (prereq_results[i].satisfied && prereq_results[i].confidence > max_conf) {
            can_increase = true;
            max_conf = prereq_results[i].confidence;
        }
    }

    if (blocked) {
        *suggested_difficulty = current_difficulty;
        *confidence = 0.8f;  /* High confidence in blocking */
        bridge->stats.constraints_violated++;
    } else if (can_increase) {
        *suggested_difficulty = fminf(current_difficulty + 0.1f, 1.0f);
        *confidence = max_conf;
    } else {
        *suggested_difficulty = current_difficulty;
        *confidence = 0.5f;
    }

    bridge->stats.recommendations_made++;

    return 0;
}

int training_logic_hub_query_early_stop(
    training_logic_hub_bridge_t* bridge,
    bool* should_stop,
    float* confidence)
{
    if (!bridge || !should_stop || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_query_early_stop: required parameter is NULL (bridge, should_stop, confidence)");
        return -1;
    }

    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_EARLY_STOP, results, 4);

    *should_stop = false;
    *confidence = 0.5f;

    for (int i = 0; i < count; i++) {
        if (results[i].satisfied) {
            *should_stop = true;
            if (results[i].confidence > *confidence) {
                *confidence = results[i].confidence;
            }
        }
    }

    bridge->stats.recommendations_made++;

    return 0;
}

/* ============================================================================
 * State and Statistics Implementation
 * ============================================================================ */

int training_logic_hub_get_state(
    training_logic_hub_bridge_t* bridge,
    training_logic_hub_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int training_logic_hub_get_stats(
    training_logic_hub_bridge_t* bridge,
    training_logic_hub_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Calculate average rule confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        total_conf += bridge->rules[i].confidence;
    }
    bridge->stats.avg_rule_confidence = bridge->num_rules > 0 ?
        total_conf / (float)bridge->num_rules : 0.0f;

    *stats = bridge->stats;
    return 0;
}

int training_logic_hub_reset_stats(training_logic_hub_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_logic_hub_reset_stats: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * Event Handlers
 * ============================================================================ */

static void handle_loss_event(training_logic_hub_bridge_t* bridge,
                              const training_event_data_t* event)
{
    if (!event->payload) return;

    /* Update loss metrics from payload */
    /* Payload structure assumed to have loss value */
    float* loss_ptr = (float*)event->payload;
    float new_loss = *loss_ptr;

    bridge->state.current_metrics.previous_loss =
        bridge->state.current_metrics.current_loss;
    bridge->state.current_metrics.current_loss = new_loss;

    /* Update loss trend */
    float delta = bridge->state.current_metrics.previous_loss - new_loss;
    bridge->state.current_metrics.loss_trend = delta > 0.01f ? -1.0f :
                                               delta < -0.01f ? 1.0f : 0.0f;

    /* Update best loss */
    if (new_loss < bridge->state.current_metrics.best_loss ||
        bridge->state.current_metrics.best_loss == 0.0f) {
        bridge->state.current_metrics.best_loss = new_loss;
        bridge->state.current_metrics.epochs_since_improvement = 0;
    }

    /* Check loss stability */
    float loss_var = fabsf(delta) /
        (bridge->state.current_metrics.previous_loss + 1e-8f);
    bridge->state.current_metrics.loss_stable = loss_var < 0.1f;

    /* Report outcome for rule learning */
    bool improved = new_loss < bridge->state.current_metrics.previous_loss;
    training_logic_hub_report_outcome(bridge, improved, false);
}

static void handle_gradient_event(training_logic_hub_bridge_t* bridge,
                                  const training_event_data_t* event)
{
    if (!event->payload) return;

    /* Update gradient metrics from payload */
    float* grad_norm = (float*)event->payload;

    /* Update running average */
    float alpha = 0.1f;
    bridge->state.current_metrics.grad_norm_avg =
        alpha * (*grad_norm) +
        (1.0f - alpha) * bridge->state.current_metrics.grad_norm_avg;

    bridge->state.current_metrics.grad_norm = *grad_norm;

    /* Check gradient health */
    float avg = bridge->state.current_metrics.grad_norm_avg;
    bridge->state.current_metrics.grad_exploding = (*grad_norm) > avg * 10.0f;
    bridge->state.current_metrics.grad_vanishing = (*grad_norm) < avg * 0.01f;
    bridge->state.current_metrics.grad_stable =
        !bridge->state.current_metrics.grad_exploding &&
        !bridge->state.current_metrics.grad_vanishing;

    /* Evaluate gradient clipping rules */
    training_rule_result_t results[4];
    int count = training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_GRADIENT_CLIP, results, 4);

    /* Publish constraint violation if clipping needed */
    if (bridge->config.publish_constraint_violations) {
        for (int i = 0; i < count; i++) {
            if (results[i].satisfied) {
                /* Would publish event here if hub connected */
                bridge->stats.constraints_violated++;
                break;
            }
        }
    }
}

static void handle_difficulty_event(training_logic_hub_bridge_t* bridge,
                                    const training_event_data_t* event)
{
    if (!event->payload) return;

    float* difficulty = (float*)event->payload;
    bridge->state.current_metrics.difficulty = *difficulty;

    /* Evaluate curriculum rules */
    training_rule_result_t results[8];
    training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_CURRICULUM_PREREQUISITE, results, 4);
    training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_DIFFICULTY_CONSTRAINT, results + 4, 4);
}

static void handle_lr_event(training_logic_hub_bridge_t* bridge,
                            const training_event_data_t* event)
{
    if (!event->payload) return;

    float* lr = (float*)event->payload;
    bridge->state.current_metrics.learning_rate = *lr;

    /* Evaluate LR safety rules */
    training_rule_result_t results[4];
    training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_LR_SAFETY, results, 4);
}

static void handle_epoch_event(training_logic_hub_bridge_t* bridge,
                               const training_event_data_t* event)
{
    bridge->state.current_metrics.epoch++;
    bridge->state.current_metrics.epochs_since_improvement++;

    /* Apply confidence decay to all rules */
    for (uint32_t i = 0; i < bridge->num_rules; i++) {
        bridge->rules[i].confidence *= TRAINING_LOGIC_CONFIDENCE_DECAY;
        if (bridge->rules[i].confidence < bridge->config.min_rule_confidence) {
            bridge->rules[i].confidence = bridge->config.min_rule_confidence;
            bridge->stats.rules_deprecated++;
        }
    }

    /* Evaluate early stopping and checkpoint rules */
    training_rule_result_t results[4];
    training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_EARLY_STOP, results, 4);
    training_logic_hub_evaluate_rules(
        bridge, TRAINING_RULE_CHECKPOINT_TRIGGER, results, 4);

    (void)event;  /* Unused currently */
}

static void handle_validation_event(training_logic_hub_bridge_t* bridge,
                                    const training_event_data_t* event)
{
    if (!event->payload) return;

    /* Assume payload has validation loss and accuracy */
    float* val_data = (float*)event->payload;
    float prev_val_loss = bridge->state.current_metrics.validation_loss;

    bridge->state.current_metrics.validation_loss = val_data[0];
    if (event->payload_size >= 2 * sizeof(float)) {
        bridge->state.current_metrics.validation_accuracy = val_data[1];
    }

    bridge->state.current_metrics.validation_improving =
        bridge->state.current_metrics.validation_loss < prev_val_loss;

    /* Report outcome for rule learning */
    training_logic_hub_report_outcome(bridge, false,
        bridge->state.current_metrics.validation_improving);
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static bool evaluate_rule_condition(training_logic_hub_bridge_t* bridge,
                                    const training_logic_rule_t* rule)
{
    /* Simple condition evaluation based on metrics */
    /* In a full implementation, this would parse the FOL condition string */
    const training_logic_metrics_t* m = &bridge->state.current_metrics;

    switch (rule->type) {
        case TRAINING_RULE_LR_SAFETY:
            return m->loss_stable && m->grad_stable && !m->grad_exploding;

        case TRAINING_RULE_GRADIENT_CLIP:
            return m->grad_exploding || m->grad_norm > m->grad_norm_avg * 5.0f;

        case TRAINING_RULE_EARLY_STOP:
            return m->epochs_since_improvement > 10 && !m->validation_improving;

        case TRAINING_RULE_CHECKPOINT_TRIGGER:
            return m->validation_improving && m->current_loss <= m->best_loss;

        case TRAINING_RULE_CURRICULUM_PREREQUISITE:
            return m->mastery > 0.8f && m->performance > 0.7f && m->difficulty < 1.0f;

        case TRAINING_RULE_DIFFICULTY_CONSTRAINT:
            return m->performance < 0.5f || m->mastery < 0.6f;

        case TRAINING_RULE_BATCH_SIZE_ADJUST:
            return m->loss_stable && m->grad_stable && m->loss_trend <= 0.0f;

        case TRAINING_RULE_TASK_SWITCH:
            return m->mastery > 0.9f;

        case TRAINING_RULE_MEMORY_REPLAY:
            return m->epochs_since_improvement > 5;

        default:
            return false;
    }
}

static void track_rule_fire(training_logic_hub_bridge_t* bridge,
                            int rule_id, float loss)
{
    rule_tracking_entry_t* entry = &bridge->tracking[bridge->tracking_head];
    entry->rule_id = rule_id;
    entry->fired_time = get_timestamp_us();
    entry->loss_at_fire = loss;
    entry->outcome_recorded = false;

    bridge->tracking_head = (bridge->tracking_head + 1) % bridge->max_tracking;
    if (bridge->tracking_count < bridge->max_tracking) {
        bridge->tracking_count++;
    }
}

static void update_rule_confidence(training_logic_hub_bridge_t* bridge,
                                   int rule_id, bool positive)
{
    if (rule_id < 0 || (uint32_t)rule_id >= bridge->num_rules) return;

    training_logic_rule_t* rule = &bridge->rules[rule_id];
    float lr = bridge->config.rule_learning_rate;

    if (positive) {
        rule->confidence += lr * TRAINING_LOGIC_CONFIDENCE_BOOST *
                           (1.0f - rule->confidence);
        rule->times_correct++;
    } else {
        rule->confidence -= lr * TRAINING_LOGIC_CONFIDENCE_PENALTY *
                           rule->confidence;
    }

    /* Clamp confidence */
    if (rule->confidence < bridge->config.min_rule_confidence) {
        rule->confidence = bridge->config.min_rule_confidence;
    }
    if (rule->confidence > 1.0f) {
        rule->confidence = 1.0f;
    }
}

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

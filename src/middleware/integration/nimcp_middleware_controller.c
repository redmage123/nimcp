/**
 * @file nimcp_middleware_controller.c
 * @brief Unified middleware control interface implementation
 *
 * WHAT: Direct API for cognitive → middleware command execution
 * WHY:  Enable top-down cognitive control with <5µs latency
 * HOW:  Unified controller delegating to specialized subsystems
 *
 * PHASE: 1.5.5 (Command Interface - Cognitive → Middleware)
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/brain/nimcp_brain_kg_helpers.h"  /* W16-C: KG attention_focus query */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include "middleware/routing/nimcp_routing_table.h"
#include "api/nimcp_api_exception.h"
#include "middleware/patterns/nimcp_pattern_library.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include <stddef.h>  /* for NULL */
#include "security/nimcp_blood_brain_barrier.h"        // Phase IS-1: BBB perimeter defense
#include "security/nimcp_security.h"                   // Security module for input validation
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "middleware_controller"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(middleware_controller)

#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

// Phase IS-1: External declaration of BBB getter (avoid header conflicts)
extern bbb_system_t nimcp_bbb_get_global_system(void);

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Middleware controller internal state
 */
struct middleware_controller {
    /* Configuration */
    middleware_controller_config_t config;

    /* Brain reference */
    brain_t brain;

    /* Subsystem references (may be NULL if not available) */
    attention_gate_t* attention_gate;
    routing_table_t* routing_table;
    pattern_library_t* pattern_library;
    shannon_monitor_t* shannon_monitor;

    /* Pattern subscriptions */
    pattern_subscription_t subscriptions[MIDDLEWARE_CTRL_MAX_SUBSCRIPTIONS];
    uint32_t num_subscriptions;
    uint32_t next_subscription_id;

    /* Regional settings cache */
    float attention_thresholds[TARGET_CUSTOM + 1];
    float attention_priorities[TARGET_CUSTOM + 1];
    float activity_scales[TARGET_CUSTOM + 1];

    /* Metrics */
    middleware_controller_metrics_t metrics;

    /* Thread safety */
    nimcp_mutex_t mutex;

    /* Timestamps */
    uint64_t created_at_us;
    uint64_t last_command_us;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get current time in microseconds
 */
static inline uint64_t get_time_us(void)
{
    return nimcp_time_monotonic_ms() * 1000;
}

/**
 * @brief Calculate Shannon information content of command
 *
 * I(cmd) = -log2(P(cmd))
 * Assume uniform distribution over command space for simplicity
 */
static float calculate_command_information(middleware_command_type_t type)
{
    /* Base information for command type selection */
    const float type_info = 3.17F;  /* log2(9 types) ≈ 3.17 bits */

    /* Additional info based on payload complexity */
    float payload_info = 0.0F;
    switch (type) {
        case COMMAND_CONFIGURE_ATTENTION:
            payload_info = 4.0F;  /* region + priority + selectivity + top_k */
            break;
        case COMMAND_ADJUST_ROUTING:
            payload_info = 3.0F;  /* source + dest + weight */
            break;
        case COMMAND_SUBSCRIBE_PATTERN:
        case COMMAND_UNSUBSCRIBE_PATTERN:
            payload_info = 2.0F;  /* pattern_id + threshold */
            break;
        case COMMAND_REDUCE_ACTIVITY:
        case COMMAND_INCREASE_ACTIVITY:
            payload_info = 1.0F;  /* region only */
            break;
        case COMMAND_RESET_BUFFERS:
            payload_info = 1.0F;  /* region only */
            break;
        default:
            payload_info = 2.0F;
            break;
    }

    return type_info + payload_info;
}

/**
 * @brief Record command execution metrics
 */
static void record_command(
    middleware_controller_t* ctrl,
    middleware_command_type_t type,
    uint64_t start_us,
    bool success)
{
    uint64_t end_us = get_time_us();
    float latency_us = (float)(end_us - start_us);
    float info_bits = calculate_command_information(type);

    nimcp_mutex_lock(&ctrl->mutex);

    ctrl->metrics.total_commands++;
    ctrl->metrics.total_latency_us += latency_us;
    ctrl->metrics.total_information_bits += info_bits;

    if (!success) {
        ctrl->metrics.failed_commands++;
    }

    /* Update latency stats */
    if (ctrl->metrics.total_commands == 1) {
        ctrl->metrics.min_latency_us = latency_us;
        ctrl->metrics.max_latency_us = latency_us;
    } else {
        if (latency_us < ctrl->metrics.min_latency_us) {
            ctrl->metrics.min_latency_us = latency_us;
        }
        if (latency_us > ctrl->metrics.max_latency_us) {
            ctrl->metrics.max_latency_us = latency_us;
        }
    }

    ctrl->metrics.avg_latency_us =
        ctrl->metrics.total_latency_us / ctrl->metrics.total_commands;

    if (latency_us > MIDDLEWARE_CTRL_LATENCY_TARGET_US) {
        ctrl->metrics.commands_exceeding_target++;
    }

    /* Update category counts */
    switch (type) {
        case COMMAND_CONFIGURE_ATTENTION:
            ctrl->metrics.attention_commands++;
            break;
        case COMMAND_ADJUST_ROUTING:
            ctrl->metrics.routing_commands++;
            break;
        case COMMAND_SUBSCRIBE_PATTERN:
        case COMMAND_UNSUBSCRIBE_PATTERN:
            ctrl->metrics.pattern_commands++;
            break;
        case COMMAND_REDUCE_ACTIVITY:
        case COMMAND_INCREASE_ACTIVITY:
            ctrl->metrics.activity_commands++;
            break;
        default:
            break;
    }

    /* Update Shannon metrics */
    ctrl->metrics.avg_information_per_command =
        ctrl->metrics.total_information_bits / ctrl->metrics.total_commands;

    if (ctrl->metrics.total_latency_us > 0) {
        ctrl->metrics.command_efficiency =
            ctrl->metrics.total_information_bits / ctrl->metrics.total_latency_us;
    }

    ctrl->last_command_us = end_us;

    nimcp_mutex_unlock(&ctrl->mutex);
}

/**
 * @brief Validate region parameter
 */
static inline bool is_valid_region(command_target_region_t region)
{
    return region <= TARGET_CUSTOM;
}

//=============================================================================
// Configuration
//=============================================================================

middleware_controller_config_t middleware_controller_default_config(void)
{
    middleware_controller_config_t config;
    memset(&config, 0, sizeof(config));

    /* Attention defaults */
    config.default_attention_threshold = 0.5F;
    config.attention_decay_rate = 0.99F;
    config.enable_adaptive_attention = true;

    /* Routing defaults */
    config.default_routing_weight = 0.5F;
    config.enable_route_learning = true;
    config.route_learning_rate = 0.1F;

    /* Pattern matching */
    config.default_pattern_threshold = 0.7F;
    config.max_subscriptions = MIDDLEWARE_CTRL_MAX_SUBSCRIPTIONS;
    config.enable_pattern_notifications = true;

    /* Performance tuning */
    config.enable_command_batching = true;
    config.batch_timeout_us = NIMCP_FAST_TIMEOUT_MS;
    config.enable_shannon_tracking = true;

    /* Safety limits */
    config.max_activity_scale = 2.0F;
    config.min_activity_scale = 0.1F;

    return config;
}

//=============================================================================
// Lifecycle API
//=============================================================================

middleware_controller_t* middleware_controller_create(brain_t brain)
{
    middleware_controller_config_t config = middleware_controller_default_config();
    return middleware_controller_create_custom(brain, &config);
}

middleware_controller_t* middleware_controller_create_custom(
    brain_t brain,
    const middleware_controller_config_t* config)
{
    LOG_DEBUG("Creating middleware controller with custom config");

    /* Guard: require brain */
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_controller_create_custom: brain is NULL");
        LOG_ERROR("Cannot create middleware controller: brain is NULL");
        return NULL;
    }

    /* Guard: require config */
    if (config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_controller_create_custom: config is NULL");
        LOG_ERROR("Cannot create middleware controller: config is NULL");
        return NULL;
    }

    /* Allocate controller */
    middleware_controller_t* ctrl = nimcp_calloc(1, sizeof(middleware_controller_t));
    if (ctrl == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_controller_create_custom: failed to allocate controller");
        LOG_ERROR("Failed to allocate middleware controller");
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_mutex_init(&ctrl->mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_controller_create_custom: failed to initialize mutex");
        LOG_ERROR("Failed to initialize mutex for middleware controller");
        nimcp_free(ctrl);
        return NULL;
    }

    /* Store configuration */
    ctrl->config = *config;
    ctrl->brain = brain;
    ctrl->created_at_us = get_time_us();
    ctrl->next_subscription_id = 1;

    /* Initialize regional settings to defaults */
    for (int i = 0; i <= TARGET_CUSTOM; i++) {
        ctrl->attention_thresholds[i] = config->default_attention_threshold;
        ctrl->attention_priorities[i] = 0.5F;
        ctrl->activity_scales[i] = 1.0F;
    }

    /* Initialize metrics */
    memset(&ctrl->metrics, 0, sizeof(ctrl->metrics));
    ctrl->metrics.min_latency_us = INFINITY;

    /* Bio-async registration */
    ctrl->bio_ctx = NULL;
    ctrl->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MIDDLEWARE_CONTROLLER,
            .module_name = "middleware_controller",
            .inbox_capacity = 64,
            .user_data = ctrl
        };
        ctrl->bio_ctx = bio_router_register_module(&bio_info);
        if (ctrl->bio_ctx) {
            ctrl->bio_async_enabled = true;
            LOG_INFO("Bio-async integration enabled for middleware controller");
        } else {
            LOG_WARN("Failed to register middleware controller with bio-router");
        }
    } else {
        LOG_DEBUG("Bio-router not initialized, skipping bio-async integration");
    }

    LOG_INFO("Middleware controller created successfully (attention_threshold=%.3f, routing_weight=%.3f)",
             config->default_attention_threshold, config->default_routing_weight);
    return ctrl;
}

void middleware_controller_destroy(middleware_controller_t* controller)
{
    if (controller == NULL) {
        LOG_WARN("Attempted to destroy NULL middleware controller");
        return;
    }

    LOG_DEBUG("Destroying middleware controller (total_commands=%lu, avg_latency=%.2f us)",
              controller->metrics.total_commands, controller->metrics.avg_latency_us);

    /* Unregister from bio-async */
    if (controller->bio_async_enabled && controller->bio_ctx) {
        LOG_DEBUG("Unregistering middleware controller from bio-router");
        bio_router_unregister_module(controller->bio_ctx);
        controller->bio_ctx = NULL;
        controller->bio_async_enabled = false;
    }

    nimcp_mutex_destroy(&controller->mutex);
    nimcp_free(controller);
    LOG_INFO("Middleware controller destroyed successfully");
}

//=============================================================================
// Attention Control API
//=============================================================================

bool middleware_controller_set_attention_threshold(
    middleware_controller_t* controller,
    command_target_region_t region,
    float threshold)
{
    /* Guard clauses */
    if (controller == NULL) {
        LOG_ERROR("Cannot set attention threshold: controller is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_threshold: validation failed");
        return false;
    }
    if (!is_valid_region(region)) {
        LOG_ERROR("Cannot set attention threshold: invalid region %d", region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_threshold: is_valid_region is NULL");
        return false;
    }

    uint64_t start_us = get_time_us();
    float original_threshold = threshold;
    threshold = nimcp_clampf(threshold, 0.0F, 1.0F);
    if (threshold != original_threshold) {
        LOG_DEBUG("Attention threshold clamped from %.3f to %.3f for region %d",
                  original_threshold, threshold, region);
    }

    nimcp_mutex_lock(&controller->mutex);

    /* Update cache */
    if (region == TARGET_ALL_REGIONS) {
        for (int i = 0; i <= TARGET_CUSTOM; i++) {
            controller->attention_thresholds[i] = threshold;
        }
    } else {
        controller->attention_thresholds[region] = threshold;
    }

    /* Apply to attention gate if available */
    if (controller->attention_gate != NULL) {
        attention_gate_set_weight(controller->attention_gate,
                                   0, (uint32_t)region, threshold);
    }

    nimcp_mutex_unlock(&controller->mutex);

    record_command(controller, COMMAND_CONFIGURE_ATTENTION, start_us, true);
    return true;
}

bool middleware_controller_set_attention_priority(
    middleware_controller_t* controller,
    command_target_region_t region,
    float priority)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_priority: validation failed");
        return false;
    }
    if (!is_valid_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_priority: is_valid_region is NULL");
        return false;
    }

    uint64_t start_us = get_time_us();
    priority = nimcp_clampf(priority, 0.0F, 1.0F);

    nimcp_mutex_lock(&controller->mutex);

    /* Update cache */
    if (region == TARGET_ALL_REGIONS) {
        for (int i = 0; i <= TARGET_CUSTOM; i++) {
            controller->attention_priorities[i] = priority;
        }
    } else {
        controller->attention_priorities[region] = priority;
    }

    nimcp_mutex_unlock(&controller->mutex);

    record_command(controller, COMMAND_CONFIGURE_ATTENTION, start_us, true);
    return true;
}

bool middleware_controller_set_attention_selectivity(
    middleware_controller_t* controller,
    command_target_region_t region,
    float selectivity,
    uint32_t top_k)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_selectivity: validation failed");
        return false;
    }
    if (!is_valid_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_attention_selectivity: is_valid_region is NULL");
        return false;
    }

    uint64_t start_us = get_time_us();
    selectivity = nimcp_clampf(selectivity, 0.0F, 1.0F);

    /* Apply to attention gate if available */
    if (controller->attention_gate != NULL) {
        /* Selectivity maps to top-k parameter */
        (void)selectivity;  /* Used in future implementation */
        (void)top_k;
    }

    record_command(controller, COMMAND_CONFIGURE_ATTENTION, start_us, true);
    return true;
}

bool middleware_controller_reset_attention(
    middleware_controller_t* controller,
    command_target_region_t region)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_reset_attention: validation failed");
        return false;
    }

    uint64_t start_us = get_time_us();

    nimcp_mutex_lock(&controller->mutex);

    /* Reset to defaults */
    if (region == TARGET_ALL_REGIONS) {
        for (int i = 0; i <= TARGET_CUSTOM; i++) {
            controller->attention_thresholds[i] =
                controller->config.default_attention_threshold;
            controller->attention_priorities[i] = 0.5F;
        }

        if (controller->attention_gate != NULL) {
            attention_gate_reset(controller->attention_gate);
        }
    } else {
        controller->attention_thresholds[region] =
            controller->config.default_attention_threshold;
        controller->attention_priorities[region] = 0.5F;
    }

    nimcp_mutex_unlock(&controller->mutex);

    record_command(controller, COMMAND_CONFIGURE_ATTENTION, start_us, true);
    return true;
}

//=============================================================================
// Routing Control API
//=============================================================================

bool middleware_controller_set_routing_priority(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination,
    float weight)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_routing_priority: validation failed");
        return false;
    }
    if (!is_valid_region(source) || !is_valid_region(destination)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_routing_priority: required parameter is NULL (is_valid_region, is_valid_region)");
        return false;
    }

    uint64_t start_us = get_time_us();
    weight = nimcp_clampf(weight, 0.0F, 1.0F);

    /* Apply to routing table if available */
    if (controller->routing_table != NULL) {
        routing_table_add_route(controller->routing_table,
                                (uint32_t)source, (uint32_t)destination, weight);
    }

    record_command(controller, COMMAND_ADJUST_ROUTING, start_us, true);
    return true;
}

bool middleware_controller_set_route_learning(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination,
    bool enable)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_route_learning: validation failed");
        return false;
    }
    if (!is_valid_region(source) || !is_valid_region(destination)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_route_learning: required parameter is NULL (is_valid_region, is_valid_region)");
        return false;
    }

    uint64_t start_us = get_time_us();

    /* Enable/disable learning is a config change */
    (void)enable;  /* Applied via routing table config in future */

    record_command(controller, COMMAND_ADJUST_ROUTING, start_us, true);
    return true;
}

bool middleware_controller_block_route(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination)
{
    return middleware_controller_set_routing_priority(
        controller, source, destination, 0.0F);
}

bool middleware_controller_unblock_route(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination)
{
    /* Guard: NULL check before dereference */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_unblock_route: validation failed");
        return false;
    }

    return middleware_controller_set_routing_priority(
        controller, source, destination,
        controller->config.default_routing_weight);
}

//=============================================================================
// Pattern Control API
//=============================================================================

bool middleware_controller_subscribe_pattern(
    middleware_controller_t* controller,
    uint32_t pattern_id,
    float confidence_threshold,
    pattern_match_callback_t callback,
    void* user_data,
    uint32_t* subscription_id)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_subscribe_pattern: validation failed");
        return false;
    }
    if (callback == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_subscribe_pattern: validation failed");
        return false;
    }
    if (subscription_id == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_subscribe_pattern: validation failed");
        return false;
    }

    uint64_t start_us = get_time_us();
    confidence_threshold = nimcp_clampf(confidence_threshold, 0.0F, 1.0F);

    nimcp_mutex_lock(&controller->mutex);

    /* Check capacity */
    if (controller->num_subscriptions >= controller->config.max_subscriptions) {
        nimcp_mutex_unlock(&controller->mutex);
        record_command(controller, COMMAND_SUBSCRIBE_PATTERN, start_us, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "middleware_controller_subscribe_pattern: capacity exceeded");
        return false;
    }

    /* Find empty slot */
    int slot = -1;
    for (uint32_t i = 0; i < controller->config.max_subscriptions; i++) {
        if (!controller->subscriptions[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(&controller->mutex);
        record_command(controller, COMMAND_SUBSCRIBE_PATTERN, start_us, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_subscribe_pattern: validation failed");
        return false;
    }

    /* Create subscription */
    pattern_subscription_t* sub = &controller->subscriptions[slot];
    sub->subscription_id = controller->next_subscription_id++;
    sub->pattern_id = pattern_id;
    sub->confidence_threshold = confidence_threshold;
    sub->callback = callback;
    sub->user_data = user_data;
    sub->created_at_us = get_time_us();
    sub->notifications_sent = 0;
    sub->active = true;

    controller->num_subscriptions++;
    controller->metrics.active_subscriptions = controller->num_subscriptions;

    *subscription_id = sub->subscription_id;

    nimcp_mutex_unlock(&controller->mutex);

    record_command(controller, COMMAND_SUBSCRIBE_PATTERN, start_us, true);
    return true;
}

bool middleware_controller_unsubscribe_pattern(
    middleware_controller_t* controller,
    uint32_t subscription_id)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_unsubscribe_pattern: validation failed");
        return false;
    }

    uint64_t start_us = get_time_us();
    bool found = false;

    nimcp_mutex_lock(&controller->mutex);

    /* Find and deactivate subscription */
    for (uint32_t i = 0; i < controller->config.max_subscriptions; i++) {
        if (controller->subscriptions[i].active &&
            controller->subscriptions[i].subscription_id == subscription_id) {
            controller->subscriptions[i].active = false;
            controller->num_subscriptions--;
            controller->metrics.active_subscriptions = controller->num_subscriptions;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&controller->mutex);

    record_command(controller, COMMAND_UNSUBSCRIBE_PATTERN, start_us, found);
    return found;
}

bool middleware_controller_set_pattern_threshold(
    middleware_controller_t* controller,
    float threshold)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_pattern_threshold: validation failed");
        return false;
    }

    uint64_t start_us = get_time_us();
    threshold = nimcp_clampf(threshold, 0.0F, 1.0F);

    controller->config.default_pattern_threshold = threshold;

    record_command(controller, COMMAND_SUBSCRIBE_PATTERN, start_us, true);
    return true;
}

bool middleware_controller_get_subscription(
    const middleware_controller_t* controller,
    uint32_t subscription_id,
    pattern_subscription_t* subscription)
{
    /* Guard clauses */
    if (controller == NULL || subscription == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_get_subscription: validation failed");
        return false;
    }

    for (uint32_t i = 0; i < controller->config.max_subscriptions; i++) {
        if (controller->subscriptions[i].active &&
            controller->subscriptions[i].subscription_id == subscription_id) {
            *subscription = controller->subscriptions[i];
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_get_subscription: operation failed");
    return false;
}

//=============================================================================
// Activity Control API
//=============================================================================

bool middleware_controller_set_activity_scale(
    middleware_controller_t* controller,
    command_target_region_t region,
    float scale)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_activity_scale: validation failed");
        return false;
    }
    if (!is_valid_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_set_activity_scale: is_valid_region is NULL");
        return false;
    }

    uint64_t start_us = get_time_us();
    scale = nimcp_clampf(scale, controller->config.min_activity_scale,
                   controller->config.max_activity_scale);

    nimcp_mutex_lock(&controller->mutex);

    if (region == TARGET_ALL_REGIONS) {
        for (int i = 0; i <= TARGET_CUSTOM; i++) {
            controller->activity_scales[i] = scale;
        }
    } else {
        controller->activity_scales[region] = scale;
    }

    nimcp_mutex_unlock(&controller->mutex);

    middleware_command_type_t cmd_type = (scale < 1.0F) ?
        COMMAND_REDUCE_ACTIVITY : COMMAND_INCREASE_ACTIVITY;
    record_command(controller, cmd_type, start_us, true);
    return true;
}

bool middleware_controller_reduce_activity(
    middleware_controller_t* controller,
    command_target_region_t region)
{
    /* Reduce by 50% */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_reduce_activity: validation failed");
        return false;
    }

    nimcp_mutex_lock(&controller->mutex);
    float current = controller->activity_scales[region];
    nimcp_mutex_unlock(&controller->mutex);

    return middleware_controller_set_activity_scale(
        controller, region, current * 0.5F);
}

bool middleware_controller_boost_activity(
    middleware_controller_t* controller,
    command_target_region_t region)
{
    /* Boost by 50% */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_boost_activity: validation failed");
        return false;
    }

    nimcp_mutex_lock(&controller->mutex);
    float current = controller->activity_scales[region];
    nimcp_mutex_unlock(&controller->mutex);

    return middleware_controller_set_activity_scale(
        controller, region, current * 1.5F);
}

//=============================================================================
// Buffer Control API
//=============================================================================

bool middleware_controller_reset_buffers(
    middleware_controller_t* controller,
    command_target_region_t region)
{
    /* Guard clauses */
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_reset_buffers: validation failed");
        return false;
    }

    uint64_t start_us = get_time_us();

    /* Buffer reset would delegate to buffer subsystems */
    (void)region;

    record_command(controller, COMMAND_RESET_BUFFERS, start_us, true);
    return true;
}

//=============================================================================
// Batch Command API
//=============================================================================

bool middleware_controller_begin_batch(
    middleware_controller_t* controller,
    middleware_command_batch_t* batch)
{
    /* Guard clauses */
    if (controller == NULL || batch == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_begin_batch: validation failed");
        return false;
    }

    memset(batch, 0, sizeof(middleware_command_batch_t));
    return true;
}

uint32_t middleware_controller_execute_batch(
    middleware_controller_t* controller,
    const middleware_command_batch_t* batch,
    command_execution_result_t* results)
{
    /* Guard clauses */
    if (controller == NULL || batch == NULL) return 0;
    if (batch->num_commands == 0) return 0;

    /* Process pending bio-async messages */
    if (controller->bio_async_enabled && controller->bio_ctx) {
        bio_router_process_inbox(controller->bio_ctx, 5);
    }

    /* Phase IS-1: BBB validation for command batch data */
    bbb_system_t bbb = nimcp_bbb_get_global_system();
    if (bbb) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(bbb, batch, sizeof(*batch), &bbb_result)) {
            return 0;  /* BBB rejected the command batch */
        }
    }

    uint64_t start_us = get_time_us();
    uint32_t success_count = 0;

    for (uint32_t i = 0; i < batch->num_commands; i++) {
        const middleware_command_t* cmd = &batch->commands[i];
        bool success = false;

        switch (cmd->type) {
            case COMMAND_CONFIGURE_ATTENTION:
                success = middleware_controller_set_attention_threshold(
                    controller,
                    cmd->payload.attention.target_region,
                    cmd->payload.attention.priority);
                break;

            case COMMAND_ADJUST_ROUTING:
                success = middleware_controller_set_routing_priority(
                    controller,
                    cmd->payload.routing.source_region,
                    cmd->payload.routing.target_region,
                    cmd->payload.routing.weight);
                break;

            case COMMAND_REDUCE_ACTIVITY:
                success = middleware_controller_reduce_activity(
                    controller,
                    cmd->payload.activity.target_region);
                break;

            case COMMAND_INCREASE_ACTIVITY:
                success = middleware_controller_boost_activity(
                    controller,
                    cmd->payload.activity.target_region);
                break;

            case COMMAND_RESET_BUFFERS:
                success = middleware_controller_reset_buffers(
                    controller, TARGET_ALL_REGIONS);
                break;

            default:
                success = false;
                break;
        }

        if (success) {
            success_count++;
        }

        if (results != NULL) {
            results[i].command_id = cmd->command_id;
            results[i].success = success;
            results[i].execution_latency_us = (float)(get_time_us() - start_us);
            results[i].information_delivered = cmd->information_bits;
        }
    }

    /* Update batching metrics */
    nimcp_mutex_lock(&controller->mutex);
    controller->metrics.batches_created++;
    controller->metrics.avg_batch_size =
        (float)controller->metrics.total_commands / controller->metrics.batches_created;
    nimcp_mutex_unlock(&controller->mutex);

    return success_count;
}

//=============================================================================
// Metrics and Diagnostics
//=============================================================================

bool middleware_controller_get_metrics(
    const middleware_controller_t* controller,
    middleware_controller_metrics_t* metrics)
{
    /* Guard clauses */
    if (controller == NULL || metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_get_metrics: validation failed");
        return false;
    }

    /* Thread-safe copy */
    middleware_controller_t* ctrl = (middleware_controller_t*)controller;
    nimcp_mutex_lock(&ctrl->mutex);
    *metrics = controller->metrics;
    nimcp_mutex_unlock(&ctrl->mutex);

    return true;
}

float middleware_controller_get_efficiency(
    const middleware_controller_t* controller)
{
    if (controller == NULL) return 0.0F;
    return controller->metrics.command_efficiency;
}

float middleware_controller_get_avg_latency(
    const middleware_controller_t* controller)
{
    if (controller == NULL) return 0.0F;
    return controller->metrics.avg_latency_us;
}

bool middleware_controller_is_performant(
    const middleware_controller_t* controller)
{
    if (controller == NULL) {
        return false;
    }
    return controller->metrics.avg_latency_us < MIDDLEWARE_CTRL_LATENCY_TARGET_US;
}

void middleware_controller_reset_stats(
    middleware_controller_t* controller)
{
    if (controller == NULL) return;

    nimcp_mutex_lock(&controller->mutex);

    uint32_t active_subs = controller->metrics.active_subscriptions;
    memset(&controller->metrics, 0, sizeof(controller->metrics));
    controller->metrics.min_latency_us = INFINITY;
    controller->metrics.active_subscriptions = active_subs;

    nimcp_mutex_unlock(&controller->mutex);
}

//=============================================================================
// Configuration API
//=============================================================================

void middleware_controller_enable_shannon_tracking(
    middleware_controller_t* controller,
    bool enable)
{
    if (controller == NULL) return;
    controller->config.enable_shannon_tracking = enable;
}

void middleware_controller_enable_batching(
    middleware_controller_t* controller,
    bool enable)
{
    if (controller == NULL) return;
    controller->config.enable_command_batching = enable;
}

void middleware_controller_set_activity_limits(
    middleware_controller_t* controller,
    float min_scale,
    float max_scale)
{
    if (controller == NULL) return;

    controller->config.min_activity_scale = nimcp_clampf(min_scale, 0.01F, 1.0F);
    controller->config.max_activity_scale = nimcp_clampf(max_scale, 1.0F, 10.0F);
}

//=============================================================================
// Integration API
//=============================================================================

bool middleware_controller_connect_shannon(
    middleware_controller_t* controller,
    shannon_monitor_t* monitor)
{
    if (controller == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_controller_connect_shannon: validation failed");
        return false;
    }
    controller->shannon_monitor = monitor;
    return true;
}

void middleware_controller_on_pattern_match(
    middleware_controller_t* controller,
    uint32_t pattern_id,
    float similarity,
    uint32_t region_id)
{
    if (controller == NULL) return;
    if (!controller->config.enable_pattern_notifications) return;

    /* BUG FIX: Collect all matching callbacks while holding lock to prevent
     * use-after-free race condition. Previously, unlocking during the loop
     * allowed another thread to unsubscribe and free user_data while the
     * callback was being invoked.
     *
     * Pattern: Snapshot-then-invoke
     * 1. Hold lock while collecting matching subscriptions
     * 2. Release lock before invoking callbacks (prevents deadlock)
     * 3. Reacquire lock to update metrics
     */
    typedef struct {
        pattern_match_callback_t callback;
        void* user_data;
        uint32_t index;  /* For updating notifications_sent after */
    } pending_callback_t;

    pending_callback_t pending[MIDDLEWARE_CTRL_MAX_SUBSCRIPTIONS];
    uint32_t pending_count = 0;

    /* W16-C: KG-biased attention threshold. If the brain's KG has an
     * "attention_focus" node, effective threshold drops by 10% for this
     * dispatch (more sensitive — more subscribers fire). Skips gracefully
     * if brain handle absent or node missing. Counter is bumped inside
     * the helper. */
    float w16_threshold_scale = 1.0f;
    if (controller->brain &&
        brain_kg_helpers_query_attention_focus(controller->brain)) {
        w16_threshold_scale = 0.9f;
    }

    nimcp_mutex_lock(&controller->mutex);

    /* Phase 1: Collect matching subscriptions (under lock).
     *
     * Walkthrough bug-fix (2026-04-24): A subscription with pattern_id == 0
     * is treated as a WILDCARD that matches any dispatched pattern_id. The
     * W9-finish tracer subscribes with pattern_id=0 intending "catch every
     * decision" (see nimcp_brain_init_monitoring.c middleware_tracer_cb),
     * but the old exact-match rule only fired when brain_decide's argmax
     * happened to be 0 — in practice almost never. Making pattern_id=0 a
     * wildcard matches the documented seed intent AND is the only current
     * subscriber shape, so no other caller regresses. Dispatchers that
     * explicitly use pattern_id=0 still match pattern_id=0 subscribers
     * (wildcard matches itself). */
    for (uint32_t i = 0; i < controller->config.max_subscriptions; i++) {
        pattern_subscription_t* sub = &controller->subscriptions[i];

        if (sub->active &&
            (sub->pattern_id == pattern_id || sub->pattern_id == 0) &&
            similarity >= (sub->confidence_threshold * w16_threshold_scale) &&
            sub->callback != NULL) {

            pending[pending_count].callback = sub->callback;
            pending[pending_count].user_data = sub->user_data;
            pending[pending_count].index = i;
            pending_count++;
        }
    }

    nimcp_mutex_unlock(&controller->mutex);

    /* Phase 2: Fire callbacks (outside lock to prevent deadlock) */
    for (uint32_t i = 0; i < pending_count; i++) {
        pending[i].callback(pattern_id, similarity, region_id, pending[i].user_data);
    }

    /* Phase 3: Update metrics (under lock) */
    if (pending_count > 0) {
        nimcp_mutex_lock(&controller->mutex);

        for (uint32_t i = 0; i < pending_count; i++) {
            /* Re-check that subscription is still active before updating */
            pattern_subscription_t* sub = &controller->subscriptions[pending[i].index];
            if (sub->active) {
                sub->notifications_sent++;
            }
        }
        controller->metrics.pattern_notifications_sent += pending_count;

        nimcp_mutex_unlock(&controller->mutex);
    }
}

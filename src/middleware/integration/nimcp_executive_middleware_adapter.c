/**
 * @file nimcp_executive_middleware_adapter.c
 * @brief Executive middleware adapter implementation
 *
 * WHAT: Bidirectional integration between executive and middleware layers
 * WHY:  Enable top-down cognitive control of middleware processing
 * HOW:  Event handlers + Shannon monitoring + command propagation
 *
 * PHASE: 1.5.2 (Executive Integration)
 * SRP: Event routing and adaptation only
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include "middleware/integration/nimcp_executive_middleware_adapter.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "api/nimcp_api_exception.h"
#include "core/events/nimcp_event_bus.h"
#include "middleware/events/nimcp_event_types.h"

#define LOG_MODULE "middleware_executive_adapter"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(executive_middleware_adapter)

#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

#define DEFAULT_COMMAND_PRIORITY_THRESHOLD 0.3f  // Min priority to issue
#define DEFAULT_INFORMATION_THRESHOLD 2.0f       // Min bits to propagate
#define DEFAULT_BATCH_WINDOW_MS 10               // 10ms batching window

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Maximum number of event subscriptions
 */
#define MAX_EVENT_SUBSCRIPTIONS 16

/**
 * @brief Executive middleware adapter internal structure
 */
struct executive_middleware_adapter {
    // Core components
    executive_controller_t* executive;          /**< Executive controller */
    quantum_command_propagator_t* propagator;   /**< Command propagator */
    shannon_monitor_t* shannon_monitor;         /**< Shannon monitor (optional) */

    // Configuration
    executive_middleware_config_t config;       /**< Configuration parameters */

    // Metrics
    executive_middleware_metrics_t metrics;     /**< Integration metrics */

    // State tracking
    uint32_t next_command_id;                   /**< Next command ID to assign */
    uint64_t last_command_time_us;              /**< Last command timestamp */

    // Event bus integration
    event_bus_t registered_event_bus;          /**< Registered event bus (NULL if not registered) */
    event_subscription_handle_t subscriptions[MAX_EVENT_SUBSCRIPTIONS]; /**< Subscription handles */
    uint32_t num_subscriptions;                 /**< Number of active subscriptions */

    // Bio-async integration
    bio_module_context_t bio_ctx;               /**< Bio-async module context */
    bool bio_async_enabled;                     /**< Bio-async enabled flag */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create middleware command from event
 *
 * WHAT: Convert event to middleware command
 * WHY:  Standardize event-to-command conversion
 * HOW:  Extract event data, populate command structure
 *
 * @param adapter Executive middleware adapter
 * @param event_type Event type
 * @param priority Command priority
 * @param target_region Target brain region
 * @return Middleware command
 */
static middleware_command_t create_command(
    executive_middleware_adapter_t* adapter,
    middleware_command_type_t command_type,
    float priority,
    command_target_region_t target_region
) {
    middleware_command_t cmd;
    memset(&cmd, 0, sizeof(middleware_command_t));

    cmd.type = command_type;
    cmd.command_id = adapter->next_command_id++;
    cmd.timestamp_us = nimcp_time_get_us();
    cmd.priority = priority;
    cmd.executed = false;
    cmd.success = false;

    // Set target region in payload based on command type
    switch (command_type) {
        case COMMAND_CONFIGURE_ATTENTION:
            cmd.payload.attention.target_region = target_region;
            break;
        case COMMAND_ADJUST_ROUTING:
            cmd.payload.routing.target_region = target_region;
            break;
        case COMMAND_REDUCE_ACTIVITY:
        case COMMAND_INCREASE_ACTIVITY:
            cmd.payload.activity.target_region = target_region;
            break;
        default:
            break;
    }

    return cmd;
}

/**
 * @brief Calculate event information content
 *
 * WHAT: Measure Shannon information of event
 * WHY:  Filter low-information events
 * HOW:  I = -log₂(P(event))
 *
 * @param event_type Event type
 * @param value Event value (e.g., priority, confidence)
 * @return Information content in bits
 */
static float calculate_event_information(uint32_t event_type, float value) {
    // Simple information estimate
    // High-value events = high information
    float base_info = 3.0F;  // 3 bits base
    float value_info = -log2f(1.0F - value + 0.01F);  // Higher value = more info
    float type_info = (float)(event_type % 4) * 0.5F;  // Type variation

    return base_info + value_info + type_info;
}

//=============================================================================
// Event Bus Callback Handlers
//=============================================================================

/**
 * @brief Event callback for attention shift events
 *
 * WHAT: Handle attention focus change events from cognitive system
 * WHY:  Translate attention events to middleware commands
 */
static void on_attention_shift_event(const brain_event_t* event, void* context) {
    executive_middleware_adapter_t* adapter = (executive_middleware_adapter_t*)context;
    if (!adapter || !event) {
        return;
    }

    // Extract priority from event data (if available)
    float priority = 0.5F;  // Default priority
    if (event->data.size >= sizeof(float)) {
        priority = *(const float*)event->data.data;
    }

    // Map to task switch handler with attention-related task type
    executive_middleware_adapter_on_task_switched(
        adapter,
        (uint32_t)event->sequence_number,  // Use sequence as task ID
        0,  // TASK_TYPE_CLASSIFICATION for attention
        priority
    );
}

/**
 * @brief Event callback for cognitive load events
 *
 * WHAT: Handle cognitive load change events
 * WHY:  Adjust middleware activity based on load
 */
static void on_health_event(const brain_event_t* event, void* context) {
    executive_middleware_adapter_t* adapter = (executive_middleware_adapter_t*)context;
    if (!adapter || !event) {
        return;
    }

    // Health events indicate cognitive load changes
    float cognitive_load = 0.5F;  // Default
    if (event->data.size >= sizeof(float)) {
        cognitive_load = *(const float*)event->data.data;
    }

    executive_middleware_adapter_on_cognitive_load_changed(adapter, cognitive_load);
}

/**
 * @brief Event callback for pattern detection events
 *
 * WHAT: Handle pattern detection notifications
 * WHY:  Subscribe to detected patterns for middleware processing
 */
static void on_pattern_event(const brain_event_t* event, void* context) {
    executive_middleware_adapter_t* adapter = (executive_middleware_adapter_t*)context;
    if (!adapter || !event) {
        return;
    }

    // Extract pattern info from event data
    uint32_t pattern_id = 0;
    float confidence = 0.5F;
    uint32_t target_region = TARGET_ALL_REGIONS;

    if (event->data.size >= sizeof(uint32_t)) {
        pattern_id = *(const uint32_t*)event->data.data;
    }
    if (event->data.size >= sizeof(uint32_t) + sizeof(float)) {
        confidence = *(const float*)(event->data.data + sizeof(uint32_t));
    }
    if (event->data.size >= 2 * sizeof(uint32_t) + sizeof(float)) {
        target_region = *(const uint32_t*)(event->data.data + sizeof(uint32_t) + sizeof(float));
    }

    executive_middleware_adapter_on_pattern_detected(adapter, pattern_id, confidence, target_region);
}

//=============================================================================
// Configuration
//=============================================================================

executive_middleware_config_t executive_middleware_adapter_default_config(void) {
    executive_middleware_config_t config = {
        .enable_adaptive_routing = true,
        .command_priority_threshold = DEFAULT_COMMAND_PRIORITY_THRESHOLD,
        .information_threshold_bits = DEFAULT_INFORMATION_THRESHOLD,
        .enable_command_batching = false,  // Disabled by default
        .batch_window_ms = DEFAULT_BATCH_WINDOW_MS,
        .enable_bottleneck_avoidance = true
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

executive_middleware_adapter_t* executive_middleware_adapter_create(
    executive_controller_t* executive,
    quantum_command_propagator_t* propagator,
    shannon_monitor_t* shannon_monitor
) {
    executive_middleware_config_t default_config = executive_middleware_adapter_default_config();
    return executive_middleware_adapter_create_custom(
        executive,
        propagator,
        shannon_monitor,
        &default_config
    );
}

executive_middleware_adapter_t* executive_middleware_adapter_create_custom(
    executive_controller_t* executive,
    quantum_command_propagator_t* propagator,
    shannon_monitor_t* shannon_monitor,
    const executive_middleware_config_t* config
) {
    // Guard: NULL checks (shannon_monitor is optional)
    if (!executive || !propagator || !config) {
        LOG_ERROR("executive_middleware_adapter_create_custom: NULL executive, propagator, or config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_middleware_adapter_create_custom: required parameter is NULL (executive, propagator, config)");
        return NULL;
    }

    // Allocate adapter
    executive_middleware_adapter_t* adapter = nimcp_calloc(1, sizeof(executive_middleware_adapter_t));
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_create_custom: Failed to allocate adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adapter is NULL");

        return NULL;
    }

    // Initialize fields
    adapter->executive = executive;
    adapter->propagator = propagator;
    adapter->shannon_monitor = shannon_monitor;
    adapter->config = *config;

    // Initialize metrics
    memset(&adapter->metrics, 0, sizeof(executive_middleware_metrics_t));

    // Initialize state
    adapter->next_command_id = 1;
    adapter->last_command_time_us = nimcp_time_get_us();

    // Initialize event bus integration
    adapter->registered_event_bus = NULL;
    adapter->num_subscriptions = 0;
    for (uint32_t i = 0; i < MAX_EVENT_SUBSCRIPTIONS; i++) {
        adapter->subscriptions[i] = INVALID_SUBSCRIPTION_HANDLE;
    }

    // Bio-async registration
    adapter->bio_ctx = NULL;
    adapter->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MIDDLEWARE_EXEC_ADAPTER,
            .module_name = "exec_mw_adapter",
            .inbox_capacity = 64,
            .user_data = adapter
        };
        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            adapter->bio_async_enabled = true;
            LOG_INFO("Bio-async integration enabled for executive middleware adapter");
        }
    }

    LOG_INFO("Executive middleware adapter created");

    return adapter;
}

void executive_middleware_adapter_destroy(executive_middleware_adapter_t* adapter) {
    if (!adapter) {
        return;
    }

    // Unregister from bio-async
    if (adapter->bio_async_enabled && adapter->bio_ctx) {
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
        adapter->bio_async_enabled = false;
        LOG_INFO("Bio-async integration disabled for executive middleware adapter");
    }

    // Note: We don't destroy executive, propagator, or shannon_monitor
    // as they are owned by the caller

    // Free adapter
    nimcp_free(adapter);
}

//=============================================================================
// Event Handler Registration
//=============================================================================

bool executive_middleware_adapter_register_handlers(
    executive_middleware_adapter_t* adapter,
    event_bus_t event_bus
) {
    // Guard: NULL checks
    if (!adapter || !event_bus) {
        LOG_ERROR("executive_middleware_adapter_register_handlers: NULL adapter or event_bus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_register_handlers: required parameter is NULL (adapter, event_bus)");
        return false;
    }

    // Check if already registered to a different event bus
    if (adapter->registered_event_bus && adapter->registered_event_bus != event_bus) {
        LOG_WARN("Adapter already registered to different event bus, unregistering first");
        executive_middleware_adapter_unregister_handlers(adapter, adapter->registered_event_bus);
    }

    adapter->num_subscriptions = 0;

    // Register for attention shift events
    event_subscription_handle_t handle = event_bus_subscribe(
        event_bus,
        EVENT_ATTENTION_SHIFT,
        on_attention_shift_event,
        adapter
    );
    if (handle != INVALID_SUBSCRIPTION_HANDLE && adapter->num_subscriptions < MAX_EVENT_SUBSCRIPTIONS) {
        adapter->subscriptions[adapter->num_subscriptions++] = handle;
    }

    // Register for health/load change events
    handle = event_bus_subscribe(
        event_bus,
        EVENT_HEALTH_DEGRADED,
        on_health_event,
        adapter
    );
    if (handle != INVALID_SUBSCRIPTION_HANDLE && adapter->num_subscriptions < MAX_EVENT_SUBSCRIPTIONS) {
        adapter->subscriptions[adapter->num_subscriptions++] = handle;
    }

    handle = event_bus_subscribe(
        event_bus,
        EVENT_HEALTH_CRITICAL,
        on_health_event,
        adapter
    );
    if (handle != INVALID_SUBSCRIPTION_HANDLE && adapter->num_subscriptions < MAX_EVENT_SUBSCRIPTIONS) {
        adapter->subscriptions[adapter->num_subscriptions++] = handle;
    }

    handle = event_bus_subscribe(
        event_bus,
        EVENT_HEALTH_RECOVERED,
        on_health_event,
        adapter
    );
    if (handle != INVALID_SUBSCRIPTION_HANDLE && adapter->num_subscriptions < MAX_EVENT_SUBSCRIPTIONS) {
        adapter->subscriptions[adapter->num_subscriptions++] = handle;
    }

    // Register for inference/pattern events
    handle = event_bus_subscribe(
        event_bus,
        EVENT_FORWARD_PASS_COMPLETE,
        on_pattern_event,
        adapter
    );
    if (handle != INVALID_SUBSCRIPTION_HANDLE && adapter->num_subscriptions < MAX_EVENT_SUBSCRIPTIONS) {
        adapter->subscriptions[adapter->num_subscriptions++] = handle;
    }

    adapter->registered_event_bus = event_bus;
    LOG_INFO("Executive middleware adapter registered %u event handlers", adapter->num_subscriptions);

    return adapter->num_subscriptions > 0;
}

void executive_middleware_adapter_unregister_handlers(
    executive_middleware_adapter_t* adapter,
    event_bus_t event_bus
) {
    // Guard: NULL checks
    if (!adapter || !event_bus) {
        return;
    }

    // Verify we're unregistering from the correct event bus
    if (adapter->registered_event_bus != event_bus) {
        LOG_WARN("Attempt to unregister from non-registered event bus");
        return;
    }

    // Unregister all subscriptions
    for (uint32_t i = 0; i < adapter->num_subscriptions; i++) {
        if (adapter->subscriptions[i] != INVALID_SUBSCRIPTION_HANDLE) {
            event_bus_unsubscribe(event_bus, adapter->subscriptions[i]);
            adapter->subscriptions[i] = INVALID_SUBSCRIPTION_HANDLE;
        }
    }

    adapter->num_subscriptions = 0;
    adapter->registered_event_bus = NULL;
    LOG_INFO("Executive middleware adapter handlers unregistered");
}

//=============================================================================
// Manual Event Handling
//=============================================================================

bool executive_middleware_adapter_on_task_switched(
    executive_middleware_adapter_t* adapter,
    uint32_t task_id,
    uint32_t task_type,
    float priority
) {
    // Guard: NULL checks
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_on_task_switched: NULL adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_on_task_switched: adapter is NULL");
        return false;
    }

    // Process pending bio-async messages
    if (adapter->bio_async_enabled && adapter->bio_ctx) {
        bio_router_process_inbox(adapter->bio_ctx, 5);
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update metrics
    adapter->metrics.total_events_received++;

    // Calculate event information
    float event_info = calculate_event_information(task_type, priority);

    // Record with Shannon monitor if available
    if (adapter->shannon_monitor) {
        // Create event for Shannon monitor recording
        event_t mw_event = {
            .type = EVENT_TYPE_ATTENTION_SHIFT,
            .priority = (priority > 0.7F) ? MW_EVENT_PRIORITY_HIGH :
                        (priority > 0.3F) ? MW_EVENT_PRIORITY_NORMAL : MW_EVENT_PRIORITY_LOW,
            .source = EVENT_SOURCE_BRAIN,
            .timestamp_us = start_time,
            .sequence_number = adapter->metrics.total_events_received,
            .data.attention_shift = {
                .previous_item = 0,  // Not tracked at this level
                .current_item = task_id,
                .attention_strength = priority,
                .shift_reason = "task_switch"
            }
        };
        shannon_monitor_record_event(adapter->shannon_monitor, &mw_event);
    }

    // Filter low-priority tasks
    if (priority < adapter->config.command_priority_threshold) {
        LOG_DEBUG("Task switch filtered: priority %.2f < threshold %.2f",
                  priority, adapter->config.command_priority_threshold);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_middleware_adapter_on_task_switched: validation failed");
        return false;
    }

    // Map task type to target region
    command_target_region_t target_region = TARGET_PREFRONTAL;  // Default
    switch (task_type) {
        case 0:  // TASK_TYPE_CLASSIFICATION
            target_region = TARGET_VISUAL_CORTEX;
            break;
        case 1:  // TASK_TYPE_REGRESSION
            target_region = TARGET_PREFRONTAL;
            break;
        case 2:  // TASK_TYPE_SEQUENCE
            target_region = TARGET_HIPPOCAMPUS;
            break;
        case 3:  // TASK_TYPE_PLANNING
            target_region = TARGET_PREFRONTAL;
            break;
        case 4:  // TASK_TYPE_REASONING
            target_region = TARGET_PREFRONTAL;
            break;
        case 5:  // TASK_TYPE_MEMORY_RETRIEVAL
            target_region = TARGET_HIPPOCAMPUS;
            break;
        default:
            target_region = TARGET_ALL_REGIONS;
            break;
    }

    // Create attention configuration command
    middleware_command_t cmd = create_command(
        adapter,
        COMMAND_CONFIGURE_ATTENTION,
        priority,
        target_region
    );

    // Set attention payload
    cmd.payload.attention.priority = priority;
    cmd.payload.attention.selectivity = 0.7F;  // Default selectivity
    cmd.payload.attention.top_k = 50;          // Attend to top 50 neurons

    cmd.information_bits = event_info;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_propagate(
        adapter->propagator,
        &cmd
    );

    // Update metrics
    adapter->metrics.total_commands_issued++;
    if (neurons_reached > 0) {
        adapter->metrics.total_commands_executed++;
        adapter->metrics.total_events_converted++;
        adapter->metrics.total_information_delivered += event_info;
    } else {
        adapter->metrics.total_commands_failed++;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    adapter->metrics.total_adaptation_time_us += elapsed_us;

    // Update rates
    if (adapter->metrics.total_commands_issued > 0) {
        adapter->metrics.command_success_rate =
            (float)adapter->metrics.total_commands_executed /
            (float)adapter->metrics.total_commands_issued;
    }

    if (adapter->metrics.total_events_received > 0) {
        adapter->metrics.event_conversion_rate =
            (float)adapter->metrics.total_events_converted /
            (float)adapter->metrics.total_events_received;
    }

    if (adapter->metrics.total_commands_issued > 0) {
        adapter->metrics.average_adaptation_latency_us =
            (float)adapter->metrics.total_adaptation_time_us /
            (float)adapter->metrics.total_commands_issued;

        adapter->metrics.average_information_per_command =
            adapter->metrics.total_information_delivered /
            (float)adapter->metrics.total_commands_issued;
    }

    LOG_INFO("Task switched: task_id=%u, type=%u, priority=%.2f -> %u neurons reached",
             task_id, task_type, priority, neurons_reached);

    return neurons_reached > 0;
}

bool executive_middleware_adapter_on_cognitive_load_changed(
    executive_middleware_adapter_t* adapter,
    float cognitive_load
) {
    // Guard: NULL checks
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_on_cognitive_load_changed: NULL adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_on_cognitive_load_changed: adapter is NULL");
        return false;
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update metrics
    adapter->metrics.total_events_received++;

    // Calculate event information
    float event_info = calculate_event_information(6, cognitive_load);  // Type 6 = load change

    // Determine command type based on cognitive load
    middleware_command_type_t cmd_type;
    if (cognitive_load > 0.7F) {
        // High load -> reduce middleware activity
        cmd_type = COMMAND_REDUCE_ACTIVITY;
    } else if (cognitive_load < 0.3F) {
        // Low load -> increase middleware activity
        cmd_type = COMMAND_INCREASE_ACTIVITY;
    } else {
        // Normal load -> no action
        return true;
    }

    // Create activity adjustment command
    middleware_command_t cmd = create_command(
        adapter,
        cmd_type,
        cognitive_load,
        TARGET_ALL_REGIONS  // Affect all regions
    );

    // Set activity payload
    cmd.payload.activity.target_region = TARGET_ALL_REGIONS;
    cmd.payload.activity.activity_scale = (cmd_type == COMMAND_REDUCE_ACTIVITY) ? 0.5F : 1.5F;

    cmd.information_bits = event_info;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_broadcast(
        adapter->propagator,
        &cmd
    );

    // Update metrics
    adapter->metrics.total_commands_issued++;
    if (neurons_reached > 0) {
        adapter->metrics.total_commands_executed++;
        adapter->metrics.total_events_converted++;
        adapter->metrics.total_information_delivered += event_info;
    } else {
        adapter->metrics.total_commands_failed++;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    adapter->metrics.total_adaptation_time_us += elapsed_us;

    LOG_INFO("Cognitive load changed: %.2f -> %u neurons reached (cmd=%s)",
             cognitive_load, neurons_reached,
             (cmd_type == COMMAND_REDUCE_ACTIVITY) ? "REDUCE" : "INCREASE");

    return neurons_reached > 0;
}

bool executive_middleware_adapter_on_pattern_detected(
    executive_middleware_adapter_t* adapter,
    uint32_t pattern_id,
    float confidence,
    uint32_t target_region
) {
    // Guard: NULL checks
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_on_pattern_detected: NULL adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_on_pattern_detected: adapter is NULL");
        return false;
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update metrics
    adapter->metrics.total_events_received++;

    // Calculate event information
    float event_info = calculate_event_information(pattern_id, confidence);

    // Filter low-confidence patterns
    if (confidence < 0.5F) {
        LOG_DEBUG("Pattern filtered: confidence %.2f < 0.5", confidence);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_middleware_adapter_on_pattern_detected: validation failed");
        return false;
    }

    // Create pattern subscription command
    middleware_command_t cmd = create_command(
        adapter,
        COMMAND_SUBSCRIBE_PATTERN,
        confidence,
        (command_target_region_t)target_region
    );

    // Set pattern payload
    cmd.payload.pattern.pattern_id = pattern_id;
    cmd.payload.pattern.confidence_threshold = confidence * 0.8F;  // 80% of detection confidence
    cmd.payload.pattern.enable_notifications = true;

    cmd.information_bits = event_info;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_propagate(
        adapter->propagator,
        &cmd
    );

    // Update metrics
    adapter->metrics.total_commands_issued++;
    if (neurons_reached > 0) {
        adapter->metrics.total_commands_executed++;
        adapter->metrics.total_events_converted++;
        adapter->metrics.total_information_delivered += event_info;
    } else {
        adapter->metrics.total_commands_failed++;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    adapter->metrics.total_adaptation_time_us += elapsed_us;

    LOG_INFO("Pattern detected: id=%u, confidence=%.2f -> %u neurons reached",
             pattern_id, confidence, neurons_reached);

    return neurons_reached > 0;
}

bool executive_middleware_adapter_on_oscillation_changed(
    executive_middleware_adapter_t* adapter,
    float frequency_hz,
    float power,
    uint32_t target_region
) {
    // Guard: NULL checks
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_on_oscillation_changed: NULL adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_on_oscillation_changed: adapter is NULL");
        return false;
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update metrics
    adapter->metrics.total_events_received++;

    // Calculate event information
    float event_info = calculate_event_information((uint32_t)frequency_hz, power);

    // Create routing adjustment command
    middleware_command_t cmd = create_command(
        adapter,
        COMMAND_ADJUST_ROUTING,
        power,
        (command_target_region_t)target_region
    );

    // Set routing payload (source = target for self-routing adjustment)
    cmd.payload.routing.source_region = (command_target_region_t)target_region;
    cmd.payload.routing.target_region = (command_target_region_t)target_region;
    cmd.payload.routing.weight = power;  // Use oscillation power as routing weight

    cmd.information_bits = event_info;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_propagate(
        adapter->propagator,
        &cmd
    );

    // Update metrics
    adapter->metrics.total_commands_issued++;
    if (neurons_reached > 0) {
        adapter->metrics.total_commands_executed++;
        adapter->metrics.total_events_converted++;
        adapter->metrics.total_information_delivered += event_info;
    } else {
        adapter->metrics.total_commands_failed++;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    adapter->metrics.total_adaptation_time_us += elapsed_us;

    LOG_INFO("Oscillation changed: freq=%.1fHz, power=%.2f -> %u neurons reached",
             frequency_hz, power, neurons_reached);

    return neurons_reached > 0;
}

bool executive_middleware_adapter_on_salience_peak(
    executive_middleware_adapter_t* adapter,
    float salience,
    uint32_t target_region
) {
    // Guard: NULL checks
    if (!adapter) {
        LOG_ERROR("executive_middleware_adapter_on_salience_peak: NULL adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_on_salience_peak: adapter is NULL");
        return false;
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update metrics
    adapter->metrics.total_events_received++;

    // Calculate event information
    float event_info = calculate_event_information(7, salience);  // Type 7 = salience

    // Create activity increase command
    middleware_command_t cmd = create_command(
        adapter,
        COMMAND_INCREASE_ACTIVITY,
        salience,
        (command_target_region_t)target_region
    );

    // Set activity payload
    cmd.payload.activity.target_region = (command_target_region_t)target_region;
    cmd.payload.activity.activity_scale = 1.0F + salience;  // Scale proportional to salience

    cmd.information_bits = event_info;

    // Propagate command
    uint32_t neurons_reached = quantum_command_propagator_propagate(
        adapter->propagator,
        &cmd
    );

    // Update metrics
    adapter->metrics.total_commands_issued++;
    if (neurons_reached > 0) {
        adapter->metrics.total_commands_executed++;
        adapter->metrics.total_events_converted++;
        adapter->metrics.total_information_delivered += event_info;
    } else {
        adapter->metrics.total_commands_failed++;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    adapter->metrics.total_adaptation_time_us += elapsed_us;

    LOG_INFO("Salience peak: %.2f -> %u neurons reached", salience, neurons_reached);

    return neurons_reached > 0;
}

//=============================================================================
// Metrics and Diagnostics
//=============================================================================

bool executive_middleware_adapter_get_metrics(
    const executive_middleware_adapter_t* adapter,
    executive_middleware_metrics_t* metrics
) {
    if (!adapter || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_middleware_adapter_get_metrics: required parameter is NULL (adapter, metrics)");
        return false;
    }

    *metrics = adapter->metrics;
    return true;
}

float executive_middleware_adapter_get_mutual_information(
    const executive_middleware_adapter_t* adapter
) {
    if (!adapter) {
        return 0.0F;
    }

    return adapter->metrics.mutual_information_exec_mw;
}

float executive_middleware_adapter_get_success_rate(
    const executive_middleware_adapter_t* adapter
) {
    if (!adapter) {
        return 0.0F;
    }

    return adapter->metrics.command_success_rate;
}

void executive_middleware_adapter_reset_stats(
    executive_middleware_adapter_t* adapter
) {
    if (!adapter) {
        return;
    }

    memset(&adapter->metrics, 0, sizeof(executive_middleware_metrics_t));
    adapter->next_command_id = 1;
    adapter->last_command_time_us = nimcp_time_get_us();

    LOG_INFO("Executive middleware adapter statistics reset");
}

//=============================================================================
// Configuration API
//=============================================================================

void executive_middleware_adapter_enable_adaptive_routing(
    executive_middleware_adapter_t* adapter,
    bool enable
) {
    if (!adapter) {
        return;
    }

    adapter->config.enable_adaptive_routing = enable;
    LOG_INFO("Adaptive routing %s", enable ? "enabled" : "disabled");
}

void executive_middleware_adapter_set_priority_threshold(
    executive_middleware_adapter_t* adapter,
    float threshold
) {
    if (!adapter) {
        return;
    }

    // Clamp to [0, 1]
    if (threshold < 0.0F) threshold = 0.0F;
    if (threshold > 1.0F) threshold = 1.0F;

    adapter->config.command_priority_threshold = threshold;
    LOG_INFO("Command priority threshold set to %.2f", threshold);
}

void executive_middleware_adapter_set_information_threshold(
    executive_middleware_adapter_t* adapter,
    float threshold_bits
) {
    if (!adapter || threshold_bits < 0.0F) {
        return;
    }

    adapter->config.information_threshold_bits = threshold_bits;
    LOG_INFO("Information threshold set to %.2f bits", threshold_bits);
}

/**
 * @file nimcp_middleware_command.h
 * @brief Middleware command types for cognitive-middleware communication
 *
 * WHAT: Command structures for executive → middleware control
 * WHY:  Enable top-down cognitive control of middleware processing
 * HOW:  Define command types, priorities, and payloads
 *
 * PHASE: 1.5.2 (Executive Integration)
 * SRP: Command type definitions only (no logic)
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#ifndef NIMCP_MIDDLEWARE_COMMAND_H
#define NIMCP_MIDDLEWARE_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Command Types
//=============================================================================

/**
 * @brief Middleware command types
 */
typedef enum {
    COMMAND_CONFIGURE_ATTENTION,      /**< Adjust attention gate parameters */
    COMMAND_SUBSCRIBE_PATTERN,        /**< Monitor specific pattern */
    COMMAND_UNSUBSCRIBE_PATTERN,      /**< Stop monitoring pattern */
    COMMAND_ADJUST_ROUTING,           /**< Modify thalamic routing weights */
    COMMAND_SET_NORMALIZATION,        /**< Change normalization strategy */
    COMMAND_REDUCE_ACTIVITY,          /**< Lower processing activity */
    COMMAND_INCREASE_ACTIVITY,        /**< Raise processing activity */
    COMMAND_RESET_BUFFERS,            /**< Clear temporal buffers */
    COMMAND_CUSTOM                    /**< User-defined command */
} middleware_command_type_t;

/**
 * @brief Brain regions that can be targeted by commands
 */
typedef enum {
    TARGET_ALL_REGIONS = 0,           /**< Broadcast to all regions */
    TARGET_PREFRONTAL = 1,            /**< Prefrontal cortex */
    TARGET_HIPPOCAMPUS = 2,           /**< Hippocampus */
    TARGET_AMYGDALA = 3,              /**< Amygdala */
    TARGET_VISUAL_CORTEX = 4,         /**< Visual cortex */
    TARGET_AUDITORY_CORTEX = 5,       /**< Auditory cortex */
    TARGET_MOTOR_CORTEX = 6,          /**< Motor cortex */
    TARGET_CUSTOM = 99                /**< Custom region */
} command_target_region_t;

//=============================================================================
// Command Structures
//=============================================================================

/**
 * @brief Attention configuration command payload
 */
typedef struct {
    command_target_region_t target_region;  /**< Which region to configure */
    float priority;                         /**< Attention priority [0-1] */
    float selectivity;                      /**< How selective [0-1] */
    uint32_t top_k;                         /**< Number of channels to attend */
} attention_config_payload_t;

/**
 * @brief Pattern subscription command payload
 */
typedef struct {
    uint32_t pattern_id;                    /**< Pattern to monitor */
    float confidence_threshold;             /**< Min confidence to report */
    bool enable_notifications;              /**< Send events on match */
} pattern_subscription_payload_t;

/**
 * @brief Routing adjustment command payload
 */
typedef struct {
    command_target_region_t source_region;  /**< Source of routing */
    command_target_region_t target_region;  /**< Target of routing */
    float weight;                           /**< Routing weight [0-1] */
} routing_adjustment_payload_t;

/**
 * @brief Activity adjustment command payload
 */
typedef struct {
    command_target_region_t target_region;  /**< Which region to adjust */
    float activity_scale;                   /**< Scale factor [0-2] */
} activity_adjustment_payload_t;

/**
 * @brief Generic middleware command
 */
typedef struct {
    middleware_command_type_t type;         /**< Command type */
    uint32_t command_id;                    /**< Unique command ID */
    uint64_t timestamp_us;                  /**< When command was issued */

    float information_bits;                 /**< Shannon information content */
    float priority;                         /**< Execution priority [0-1] */

    // Payload (union for type safety)
    union {
        attention_config_payload_t attention;
        pattern_subscription_payload_t pattern;
        routing_adjustment_payload_t routing;
        activity_adjustment_payload_t activity;
        void* custom_payload;
    } payload;

    // Status tracking
    bool executed;                          /**< Has been executed */
    uint64_t execution_time_us;             /**< When executed (0 if not) */
    bool success;                           /**< Execution succeeded */
} middleware_command_t;

//=============================================================================
// Command Execution Result
//=============================================================================

/**
 * @brief Result of command execution
 *
 * Note: Renamed from command_result_t to avoid conflict with
 *       core/directives/nimcp_command_compliance.h which defines
 *       command_result_t for compliance evaluation results.
 */
typedef struct {
    uint32_t command_id;                    /**< Which command */
    bool success;                           /**< Execution succeeded */
    uint64_t execution_latency_us;          /**< How long it took */
    float information_delivered;            /**< Bits successfully delivered */
    char error_message[128];                /**< Error if failed */
} command_execution_result_t;

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIDDLEWARE_COMMAND_H

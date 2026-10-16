//=============================================================================
// nimcp_portia_messages.h - Portia Bio-Async Message Types
//=============================================================================
/**
 * @file nimcp_portia_messages.h
 * @brief Bio-async message definitions for Portia system
 *
 * WHAT: Message types for Portia inter-module communication
 * WHY:  Enable decoupled, asynchronous Portia event handling
 * HOW:  Define message structures for tier changes, power alerts, etc.
 *
 * MESSAGE CHANNELS:
 * - DOPAMINE: Resource availability increases, tier upgrades
 * - SEROTONIN: Gradual state changes, planning results
 * - NOREPINEPHRINE: Alerts, power critical, thermal throttle
 * - ACETYLCHOLINE: Fast queries, status requests
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_MESSAGES_H
#define NIMCP_PORTIA_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include "async/nimcp_bio_messages.h"
#include "portia/nimcp_portia.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Message Type Enumeration
//=============================================================================

/**
 * @brief Portia-specific bio-async message types
 *
 * Range: 0x0B00 - 0x0BFF (Portia messages)
 */
typedef enum {
    /* Tier management messages */
    BIO_MSG_TYPE_PORTIA_TIER_CHANGE = 0x0B00,     /**< Tier changed */
    BIO_MSG_TYPE_PORTIA_TIER_QUERY,               /**< Query current tier */
    BIO_MSG_TYPE_PORTIA_TIER_RECOMMENDATION,      /**< Recommended tier */

    /* Power management messages */
    BIO_MSG_TYPE_PORTIA_POWER_ALERT = 0x0B10,     /**< Power state alert */
    BIO_MSG_TYPE_PORTIA_POWER_QUERY,              /**< Query power state */
    BIO_MSG_TYPE_PORTIA_BATTERY_UPDATE,           /**< Battery level update */

    /* Resource tracking messages */
    BIO_MSG_TYPE_PORTIA_RESOURCE_REQUEST = 0x0B20, /**< Request resource info */
    BIO_MSG_TYPE_PORTIA_RESOURCE_UPDATE,          /**< Resource update */
    BIO_MSG_TYPE_PORTIA_THERMAL_ALERT,            /**< Thermal alert */

    /* Degradation messages */
    BIO_MSG_TYPE_PORTIA_DEGRADATION_EVENT = 0x0B30, /**< Degradation event */
    BIO_MSG_TYPE_PORTIA_RECOVERY_EVENT,           /**< Recovery event */
    BIO_MSG_TYPE_PORTIA_FEATURE_DISABLED,         /**< Feature disabled */
    BIO_MSG_TYPE_PORTIA_FEATURE_ENABLED,          /**< Feature enabled */

    /* Accelerator messages */
    BIO_MSG_TYPE_PORTIA_ACCELERATOR_DETECTED = 0x0B40, /**< Accelerator detected */
    BIO_MSG_TYPE_PORTIA_ACCELERATOR_QUERY,        /**< Query accelerators */
    BIO_MSG_TYPE_PORTIA_OFFLOAD_REQUEST,          /**< Request offload to accelerator */
    BIO_MSG_TYPE_PORTIA_OFFLOAD_COMPLETE,         /**< Offload complete */

    /* Sensor fusion messages */
    BIO_MSG_TYPE_PORTIA_SENSOR_FUSION_UPDATE = 0x0B50, /**< Fused sensor data */
    BIO_MSG_TYPE_PORTIA_METRICS_REPORT,           /**< Metrics report */

    /* Planning messages */
    BIO_MSG_TYPE_PORTIA_PLANNING_REQUEST = 0x0B60, /**< Request resource plan */
    BIO_MSG_TYPE_PORTIA_PLANNING_RESULT,          /**< Planning result */
    BIO_MSG_TYPE_PORTIA_STRATEGY_CHANGE,          /**< Strategy changed */

    /* Workload messages */
    BIO_MSG_TYPE_PORTIA_TARGET_CLASSIFIED = 0x0B70, /**< Workload classified */
    BIO_MSG_TYPE_PORTIA_WORKLOAD_QUERY,           /**< Query current workload */

    /* System messages */
    BIO_MSG_TYPE_PORTIA_STATUS_QUERY = 0x0B80,    /**< Query system status */
    BIO_MSG_TYPE_PORTIA_STATUS_RESPONSE,          /**< Status response */
    BIO_MSG_TYPE_PORTIA_CONFIG_UPDATE,            /**< Configuration update */

    BIO_MSG_TYPE_PORTIA_COUNT
} portia_message_type_t;

//=============================================================================
// Tier Management Messages
//=============================================================================

/**
 * @brief Tier change notification
 *
 * CHANNEL: DOPAMINE (upgrade) or SEROTONIN (downgrade)
 */
typedef struct {
    bio_message_header_t header;
    platform_tier_t old_tier;      /**< Previous tier */
    platform_tier_t new_tier;      /**< New tier */
    float confidence;              /**< Confidence in decision (0-1) */
    uint32_t reason;               /**< Reason for change (bitmask) */
    uint64_t timestamp_us;         /**< When change occurred */
} bio_msg_portia_tier_change_t;

/* Tier change reasons (bitmask) */
#define PORTIA_TIER_REASON_RESOURCE_INCREASE  (1 << 0)
#define PORTIA_TIER_REASON_RESOURCE_DECREASE  (1 << 1)
#define PORTIA_TIER_REASON_POWER_LOW          (1 << 2)
#define PORTIA_TIER_REASON_THERMAL_HIGH       (1 << 3)
#define PORTIA_TIER_REASON_USER_REQUEST       (1 << 4)
#define PORTIA_TIER_REASON_WORKLOAD_CHANGE    (1 << 5)

/**
 * @brief Tier query request
 *
 * CHANNEL: ACETYLCHOLINE (fast query)
 */
typedef struct {
    bio_message_header_t header;
    bool include_recommendation; /**< Also get recommended tier */
} bio_msg_portia_tier_query_t;

/**
 * @brief Tier recommendation
 *
 * CHANNEL: SEROTONIN (deliberative)
 */
typedef struct {
    bio_message_header_t header;
    platform_tier_t current_tier;  /**< Current tier */
    platform_tier_t recommended_tier; /**< Recommended tier */
    float confidence;              /**< Confidence (0-1) */
    char reasoning[256];           /**< Explanation */
} bio_msg_portia_tier_recommendation_t;

//=============================================================================
// Power Management Messages
//=============================================================================

/**
 * @brief Power state alert
 *
 * CHANNEL: NOREPINEPHRINE (alerting)
 */
typedef struct {
    bio_message_header_t header;
    portia_power_state_t old_state; /**< Previous state */
    portia_power_state_t new_state; /**< New state */
    float battery_level;           /**< Battery level (0-1, -1 if N/A) */
    bool is_critical;              /**< Critical alert */
    uint32_t estimated_runtime_ms; /**< Estimated time remaining */
} bio_msg_portia_power_alert_t;

/**
 * @brief Battery level update
 *
 * CHANNEL: SEROTONIN (slow update)
 */
typedef struct {
    bio_message_header_t header;
    float battery_level;           /**< Current level (0-1) */
    float battery_voltage;         /**< Voltage (V) */
    float battery_current;         /**< Current (A, negative = discharging) */
    bool is_charging;              /**< Charging state */
    uint32_t estimated_runtime_ms; /**< Time remaining */
} bio_msg_portia_battery_update_t;

//=============================================================================
// Resource Tracking Messages
//=============================================================================

/**
 * @brief Resource information request
 *
 * CHANNEL: ACETYLCHOLINE (fast query)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t query_flags;          /**< What to query (bitmask) */
} bio_msg_portia_resource_request_t;

#define PORTIA_QUERY_CPU        (1 << 0)
#define PORTIA_QUERY_MEMORY     (1 << 1)
#define PORTIA_QUERY_THERMAL    (1 << 2)
#define PORTIA_QUERY_POWER      (1 << 3)
#define PORTIA_QUERY_ALL        0xFFFFFFFF

/**
 * @brief Resource update notification
 *
 * CHANNEL: SEROTONIN (periodic update)
 */
typedef struct {
    bio_message_header_t header;
    float cpu_usage;               /**< CPU utilization (0-1) */
    float memory_usage;            /**< Memory utilization (0-1) */
    float temperature_celsius;     /**< System temperature */
    uint32_t active_threads;       /**< Active thread count */
    uint64_t available_memory_bytes; /**< Available memory */
} bio_msg_portia_resource_update_t;

/**
 * @brief Thermal alert
 *
 * CHANNEL: NOREPINEPHRINE (alerting)
 */
typedef struct {
    bio_message_header_t header;
    portia_thermal_state_t old_state; /**< Previous state */
    portia_thermal_state_t new_state; /**< New state */
    float temperature_celsius;     /**< Current temperature */
    float threshold_celsius;       /**< Threshold crossed */
    bool is_throttling;            /**< System throttling active */
} bio_msg_portia_thermal_alert_t;

//=============================================================================
// Degradation Messages
//=============================================================================

/**
 * @brief Degradation event notification
 *
 * CHANNEL: NOREPINEPHRINE (alerting)
 */
typedef struct {
    bio_message_header_t header;
    portia_degradation_level_t old_level; /**< Previous level */
    portia_degradation_level_t new_level; /**< New level */
    uint32_t features_disabled;    /**< Number of features disabled */
    uint32_t reason;               /**< Reason (bitmask) */
    char description[256];         /**< Human-readable description */
} bio_msg_portia_degradation_event_t;

/* Degradation reasons (bitmask) */
#define PORTIA_DEGRADE_REASON_POWER     (1 << 0)
#define PORTIA_DEGRADE_REASON_THERMAL   (1 << 1)
#define PORTIA_DEGRADE_REASON_MEMORY    (1 << 2)
#define PORTIA_DEGRADE_REASON_CPU       (1 << 3)
#define PORTIA_DEGRADE_REASON_USER      (1 << 4)

/**
 * @brief Feature disabled notification
 *
 * CHANNEL: SEROTONIN (state change)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t feature_id;           /**< Feature identifier */
    char feature_name[64];         /**< Feature name */
    uint32_t reason;               /**< Reason for disabling */
} bio_msg_portia_feature_disabled_t;

/**
 * @brief Feature enabled notification
 *
 * CHANNEL: DOPAMINE (recovery)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t feature_id;           /**< Feature identifier */
    char feature_name[64];         /**< Feature name */
    uint32_t reason;               /**< Reason for enabling */
} bio_msg_portia_feature_enabled_t;

//=============================================================================
// Accelerator Messages
//=============================================================================

/**
 * @brief Accelerator detected notification
 *
 * CHANNEL: DOPAMINE (new resource)
 */
typedef struct {
    bio_message_header_t header;
    portia_accelerator_type_t type; /**< Accelerator type */
    uint32_t device_id;            /**< Device identifier */
    char device_name[64];          /**< Device name */
    uint64_t memory_bytes;         /**< Device memory */
    float compute_capability;      /**< Relative capability */
    bool is_available;             /**< Currently available */
} bio_msg_portia_accelerator_detected_t;

/**
 * @brief Offload request
 *
 * CHANNEL: ACETYLCHOLINE (fast decision)
 */
typedef struct {
    bio_message_header_t header;
    portia_accelerator_type_t preferred_type; /**< Preferred accelerator */
    uint32_t workload_size;        /**< Workload size estimate */
    uint32_t priority;             /**< Priority (0-10) */
    nimcp_bio_promise_t response_promise; /**< Response promise */
} bio_msg_portia_offload_request_t;

/**
 * @brief Offload complete notification
 *
 * CHANNEL: DOPAMINE (completion)
 */
typedef struct {
    bio_message_header_t header;
    portia_accelerator_type_t accelerator_used; /**< Accelerator used */
    bool success;                  /**< Offload successful */
    float execution_time_ms;       /**< Execution time */
    nimcp_error_t error;           /**< Error code if failed */
} bio_msg_portia_offload_complete_t;

//=============================================================================
// Sensor Fusion Messages
//=============================================================================

/**
 * @brief Fused sensor data update
 *
 * CHANNEL: SEROTONIN (integrated state)
 */
typedef struct {
    bio_message_header_t header;
    float overall_health;          /**< Overall system health (0-1) */
    float resource_pressure;       /**< Resource pressure (0-1) */
    float performance_score;       /**< Performance score (0-1) */
    float efficiency_score;        /**< Efficiency score (0-1) */
    uint32_t sensor_count;         /**< Number of sensors fused */
    float confidence;              /**< Fusion confidence (0-1) */
} bio_msg_portia_sensor_fusion_update_t;

//=============================================================================
// Planning Messages
//=============================================================================

/**
 * @brief Planning request
 *
 * CHANNEL: SEROTONIN (deliberative)
 */
typedef struct {
    bio_message_header_t header;
    portia_workload_type_t workload; /**< Expected workload */
    uint32_t duration_ms;          /**< Expected duration */
    uint32_t priority;             /**< Priority (0-10) */
    nimcp_bio_promise_t response_promise; /**< Response promise */
} bio_msg_portia_planning_request_t;

/**
 * @brief Planning result
 *
 * CHANNEL: SEROTONIN (deliberative)
 */
typedef struct {
    bio_message_header_t header;
    platform_tier_t recommended_tier; /**< Recommended tier */
    portia_degradation_level_t recommended_degradation; /**< Recommended degradation */
    bool use_accelerator;          /**< Use accelerator */
    portia_accelerator_type_t accelerator_type; /**< Accelerator type */
    float estimated_duration_ms;   /**< Estimated duration */
    float confidence;              /**< Confidence (0-1) */
    char strategy_description[256]; /**< Strategy explanation */
} bio_msg_portia_planning_result_t;

//=============================================================================
// Workload Messages
//=============================================================================

/**
 * @brief Workload classified notification
 *
 * CHANNEL: ACETYLCHOLINE (fast classification)
 */
typedef struct {
    bio_message_header_t header;
    portia_workload_type_t old_workload; /**< Previous workload */
    portia_workload_type_t new_workload; /**< New workload */
    float confidence;              /**< Classification confidence (0-1) */
    uint32_t pattern_id;           /**< Pattern identifier */
} bio_msg_portia_target_classified_t;

//=============================================================================
// System Messages
//=============================================================================

/**
 * @brief Status query request
 *
 * CHANNEL: ACETYLCHOLINE (fast query)
 */
typedef struct {
    bio_message_header_t header;
    bool include_statistics;       /**< Include detailed statistics */
    nimcp_bio_promise_t response_promise; /**< Response promise */
} bio_msg_portia_status_query_t;

/**
 * @brief Status response
 *
 * CHANNEL: ACETYLCHOLINE (fast response)
 */
typedef struct {
    bio_message_header_t header;
    portia_status_t status;        /**< Current status */
} bio_msg_portia_status_response_t;

//=============================================================================
// Message Utilities
//=============================================================================

/**
 * @brief Get recommended channel for Portia message type
 */
static inline nimcp_bio_channel_type_t portia_msg_recommended_channel(portia_message_type_t type) {
    /* Tier upgrades, accelerator detection, recovery → Dopamine */
    if (type == BIO_MSG_TYPE_PORTIA_TIER_CHANGE ||
        type == BIO_MSG_TYPE_PORTIA_ACCELERATOR_DETECTED ||
        type == BIO_MSG_TYPE_PORTIA_FEATURE_ENABLED ||
        type == BIO_MSG_TYPE_PORTIA_OFFLOAD_COMPLETE) {
        return BIO_CHANNEL_DOPAMINE;
    }

    /* Alerts, degradation, thermal, power critical → Norepinephrine */
    if (type == BIO_MSG_TYPE_PORTIA_POWER_ALERT ||
        type == BIO_MSG_TYPE_PORTIA_THERMAL_ALERT ||
        type == BIO_MSG_TYPE_PORTIA_DEGRADATION_EVENT) {
        return BIO_CHANNEL_NOREPINEPHRINE;
    }

    /* Fast queries, status, workload → Acetylcholine */
    if (type == BIO_MSG_TYPE_PORTIA_TIER_QUERY ||
        type == BIO_MSG_TYPE_PORTIA_RESOURCE_REQUEST ||
        type == BIO_MSG_TYPE_PORTIA_STATUS_QUERY ||
        type == BIO_MSG_TYPE_PORTIA_TARGET_CLASSIFIED ||
        type == BIO_MSG_TYPE_PORTIA_OFFLOAD_REQUEST) {
        return BIO_CHANNEL_ACETYLCHOLINE;
    }

    /* Slow updates, planning, sensor fusion → Serotonin */
    return BIO_CHANNEL_SEROTONIN;
}

/**
 * @brief Initialize Portia message header
 */
static inline void portia_msg_init_header(
    bio_message_header_t* header,
    portia_message_type_t type,
    bio_module_id_t source,
    bio_module_id_t target,
    size_t payload_size)
{
    header->type = (bio_message_type_t)type;
    header->sequence_id = 0;
    header->source_module = source;
    header->target_module = target;
    header->timestamp_us = 0;
    header->channel = portia_msg_recommended_channel(type);
    header->payload_size = (uint32_t)payload_size;
    header->flags = 0;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_MESSAGES_H

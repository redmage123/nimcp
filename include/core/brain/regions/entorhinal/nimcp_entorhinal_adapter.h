/**
 * @file nimcp_entorhinal_adapter.h
 * @brief Brain adapter for Entorhinal Cortex integration
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Unified adapter connecting entorhinal cortex to the brain system
 * WHY:  Enable seamless integration with hippocampus, neocortex, and all NIMCP modules
 * HOW:  Orchestrates grid cells, border cells, HD cells, and memory gateway
 *
 * ARCHITECTURE:
 * - Wraps all entorhinal sub-modules
 * - Provides high-level API for spatial processing and memory gateway
 * - Full bidirectional integration with all NIMCP systems
 * - Supports training through backpropagation adapters
 */

#ifndef NIMCP_ENTORHINAL_ADAPTER_H
#define NIMCP_ENTORHINAL_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"

/*=============================================================================
 * ADAPTER CONFIGURATION
 *===========================================================================*/

/**
 * @brief Adapter-specific configuration
 */
typedef struct {
    /* Base entorhinal config */
    entorhinal_config_t base_config;

    /* Adapter options */
    bool auto_connect_hippocampus;
    bool auto_connect_neocortex;
    bool auto_register_kg;

    /* Event system */
    bool enable_events;
    uint32_t event_buffer_size;

    /* Async processing */
    bool enable_async_processing;
    uint32_t async_queue_size;

    /* Auto-calibration */
    bool enable_auto_calibration;
    float calibration_interval_ms;
} entorhinal_adapter_config_t;

/*=============================================================================
 * ADAPTER STRUCTURE
 *===========================================================================*/

/**
 * @brief Entorhinal cortex adapter
 */
typedef struct entorhinal_adapter {
    /* Core entorhinal instance */
    nimcp_entorhinal_t* entorhinal;

    /* Configuration */
    entorhinal_adapter_config_t config;

    /* Parent brain reference */
    nimcp_brain_t* brain;

    /* Event callback */
    void (*event_callback)(uint32_t event_type, const void* data, void* user_data);
    void* event_user_data;

    /* Async processing state */
    bool async_enabled;
    uint32_t pending_async_ops;

    /* Auto-calibration state */
    float time_since_calibration_ms;
    bool calibration_needed;

    /* Statistics */
    uint64_t adapter_updates;
    uint64_t events_published;
    float mean_processing_time_ms;
} entorhinal_adapter_t;

/*=============================================================================
 * ADAPTER LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default adapter configuration
 */
entorhinal_adapter_config_t entorhinal_adapter_default_config(void);

/**
 * @brief Create entorhinal adapter
 */
entorhinal_adapter_t* entorhinal_adapter_create(
    const entorhinal_adapter_config_t* config);

/**
 * @brief Destroy entorhinal adapter
 */
void entorhinal_adapter_destroy(entorhinal_adapter_t* adapter);

/**
 * @brief Connect adapter to brain
 */
int entorhinal_adapter_connect_brain(entorhinal_adapter_t* adapter,
    nimcp_brain_t* brain);

/**
 * @brief Disconnect adapter from brain
 */
int entorhinal_adapter_disconnect_brain(entorhinal_adapter_t* adapter);

/**
 * @brief Reset adapter state
 */
bool entorhinal_adapter_reset(entorhinal_adapter_t* adapter);

/*=============================================================================
 * HIGH-LEVEL PROCESSING API
 *===========================================================================*/

/**
 * @brief Process spatial update (position + heading + velocity)
 */
int entorhinal_adapter_process_spatial(entorhinal_adapter_t* adapter,
    const float* position, uint32_t pos_dim,
    float heading, const float* velocity, float dt);

/**
 * @brief Process sensory input for boundary/object detection
 */
int entorhinal_adapter_process_sensory(entorhinal_adapter_t* adapter,
    const float* visual_input, uint32_t visual_dim,
    const float* boundary_input, uint32_t boundary_dim);

/**
 * @brief Process memory encoding request
 */
int entorhinal_adapter_encode_memory(entorhinal_adapter_t* adapter,
    const float* features, uint32_t feature_dim,
    const float* context, uint32_t context_dim,
    uint32_t* memory_id_out);

/**
 * @brief Process memory retrieval request
 */
int entorhinal_adapter_retrieve_memory(entorhinal_adapter_t* adapter,
    const float* cue, uint32_t cue_dim,
    float* retrieved, uint32_t max_dim,
    float* similarity_out);

/**
 * @brief Full update cycle
 */
int entorhinal_adapter_update(entorhinal_adapter_t* adapter, float dt);

/*=============================================================================
 * EVENT SYSTEM
 *===========================================================================*/

/**
 * @brief Event types
 */
typedef enum {
    ENTORHINAL_EVENT_POSITION_UPDATE = 0,
    ENTORHINAL_EVENT_HEADING_UPDATE,
    ENTORHINAL_EVENT_BOUNDARY_DETECTED,
    ENTORHINAL_EVENT_MEMORY_ENCODED,
    ENTORHINAL_EVENT_MEMORY_RETRIEVED,
    ENTORHINAL_EVENT_GRID_DRIFT_WARNING,
    ENTORHINAL_EVENT_CALIBRATION_NEEDED,
    ENTORHINAL_EVENT_SECURITY_ALERT,
    ENTORHINAL_EVENT_IMMUNE_ALERT,
    ENTORHINAL_EVENT_COUNT
} entorhinal_event_type_t;

/**
 * @brief Set event callback
 */
int entorhinal_adapter_set_event_callback(entorhinal_adapter_t* adapter,
    void (*callback)(uint32_t event_type, const void* data, void* user_data),
    void* user_data);

/**
 * @brief Publish event to cognitive hub
 */
int entorhinal_adapter_publish_event(entorhinal_adapter_t* adapter,
    entorhinal_event_type_t event_type, const void* event_data);

/*=============================================================================
 * ASYNC API
 *===========================================================================*/

/**
 * @brief Request async position estimate
 */
nimcp_bio_future_t entorhinal_adapter_request_position_async(
    entorhinal_adapter_t* adapter);

/**
 * @brief Request async memory encoding
 */
nimcp_bio_future_t entorhinal_adapter_request_encode_async(
    entorhinal_adapter_t* adapter,
    const float* features, uint32_t dim);

/**
 * @brief Request async memory retrieval
 */
nimcp_bio_future_t entorhinal_adapter_request_retrieve_async(
    entorhinal_adapter_t* adapter,
    const float* cue, uint32_t dim);

/**
 * @brief Process pending async operations
 */
uint32_t entorhinal_adapter_process_async(entorhinal_adapter_t* adapter,
    uint32_t max_ops);

/*=============================================================================
 * CALIBRATION API
 *===========================================================================*/

/**
 * @brief Trigger manual calibration
 */
int entorhinal_adapter_calibrate(entorhinal_adapter_t* adapter,
    const float* known_position, float known_heading);

/**
 * @brief Check if calibration is needed
 */
bool entorhinal_adapter_needs_calibration(const entorhinal_adapter_t* adapter);

/**
 * @brief Get calibration quality
 */
float entorhinal_adapter_get_calibration_quality(const entorhinal_adapter_t* adapter);

/*=============================================================================
 * DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get adapter statistics
 */
int entorhinal_adapter_get_stats(const entorhinal_adapter_t* adapter,
    entorhinal_stats_t* stats);

/**
 * @brief Get underlying entorhinal instance
 */
nimcp_entorhinal_t* entorhinal_adapter_get_entorhinal(entorhinal_adapter_t* adapter);

/**
 * @brief Log adapter diagnostics
 */
int entorhinal_adapter_log_diagnostics(const entorhinal_adapter_t* adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_ADAPTER_H */

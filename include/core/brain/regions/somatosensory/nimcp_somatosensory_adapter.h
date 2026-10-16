/**
 * @file nimcp_somatosensory_adapter.h
 * @brief Adapter for Somatosensory Cortex integration with NIMCP brain
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * This adapter provides the standardized interface for integrating the
 * somatosensory cortex module with the NIMCP brain initialization system
 * and bio-async messaging infrastructure.
 */

#ifndef NIMCP_SOMATOSENSORY_ADAPTER_H
#define NIMCP_SOMATOSENSORY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

/*=============================================================================
 * ADAPTER TYPE
 *===========================================================================*/

/**
 * @brief Opaque adapter handle
 */
typedef struct soma_adapter* soma_adapter_t;

/*=============================================================================
 * ADAPTER CONFIGURATION
 *===========================================================================*/

/**
 * @brief Adapter configuration for brain integration
 */
typedef struct {
    /* Module configuration */
    soma_config_t soma_config;

    /* Bio-async settings */
    bool enable_bio_async;
    uint32_t message_queue_size;
    uint32_t priority;

    /* Brain integration */
    bool auto_register_handlers;
    bool enable_health_monitoring;
    uint32_t health_check_interval_ms;

    /* Processing mode */
    bool real_time_processing;
    float max_latency_ms;

    /* KG wiring */
    bool enable_kg_registration;
    const char* kg_node_name;
} soma_adapter_config_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default adapter configuration
 */
soma_adapter_config_t soma_adapter_default_config(void);

/**
 * @brief Create somatosensory adapter
 */
soma_adapter_t soma_adapter_create(const soma_adapter_config_t* config);

/**
 * @brief Destroy adapter
 */
void soma_adapter_destroy(soma_adapter_t adapter);

/**
 * @brief Initialize adapter with brain context
 */
int soma_adapter_init(soma_adapter_t adapter, void* brain_ctx);

/**
 * @brief Start adapter processing
 */
int soma_adapter_start(soma_adapter_t adapter);

/**
 * @brief Stop adapter processing
 */
int soma_adapter_stop(soma_adapter_t adapter);

/**
 * @brief Reset adapter state
 */
int soma_adapter_reset(soma_adapter_t adapter);

/*=============================================================================
 * MESSAGE HANDLERS
 *===========================================================================*/

/**
 * @brief Register bio-async message handlers
 */
int soma_adapter_register_handlers(soma_adapter_t adapter, void* router);

/**
 * @brief Unregister bio-async message handlers
 */
int soma_adapter_unregister_handlers(soma_adapter_t adapter, void* router);

/**
 * @brief Process incoming bio-async message
 */
int soma_adapter_handle_message(soma_adapter_t adapter,
                                uint32_t msg_type,
                                const void* payload,
                                size_t payload_size);

/*=============================================================================
 * BRAIN INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to thalamus (VPL/VPM nuclei)
 */
int soma_adapter_connect_thalamus(soma_adapter_t adapter, void* thalamus);

/**
 * @brief Connect to motor cortex
 */
int soma_adapter_connect_motor_cortex(soma_adapter_t adapter, void* motor);

/**
 * @brief Connect to parietal cortex
 */
int soma_adapter_connect_parietal(soma_adapter_t adapter, void* parietal);

/**
 * @brief Connect to hypothalamus
 */
int soma_adapter_connect_hypothalamus(soma_adapter_t adapter, void* hypothalamus);

/**
 * @brief Connect to pain pathway (PAG)
 */
int soma_adapter_connect_pain_pathway(soma_adapter_t adapter, void* pag);

/**
 * @brief Connect to immune system
 */
int soma_adapter_connect_immune(soma_adapter_t adapter, void* immune);

/**
 * @brief Connect all standard bridges
 */
int soma_adapter_connect_all(soma_adapter_t adapter, void** connections);

/*=============================================================================
 * INPUT/OUTPUT
 *===========================================================================*/

/**
 * @brief Send touch event to somatosensory cortex
 */
int soma_adapter_send_touch(soma_adapter_t adapter,
                            body_segment_t segment,
                            const float* position,
                            float intensity,
                            touch_modality_t modality);

/**
 * @brief Send pain signal
 */
int soma_adapter_send_pain(soma_adapter_t adapter,
                           body_segment_t segment,
                           pain_type_t type,
                           float intensity);

/**
 * @brief Send proprioceptive update
 */
int soma_adapter_send_proprioception(soma_adapter_t adapter,
                                     body_segment_t segment,
                                     const float* position,
                                     const float* velocity);

/**
 * @brief Send temperature signal
 */
int soma_adapter_send_temperature(soma_adapter_t adapter,
                                  body_segment_t segment,
                                  float temperature);

/**
 * @brief Get somatosensory output
 */
int soma_adapter_get_output(soma_adapter_t adapter,
                            float* output,
                            uint32_t max_size,
                            uint32_t* actual_size);

/**
 * @brief Get body state summary
 */
int soma_adapter_get_body_state(soma_adapter_t adapter,
                                float* positions,
                                float* pain_levels,
                                uint32_t max_segments);

/*=============================================================================
 * UPDATE AND PROCESSING
 *===========================================================================*/

/**
 * @brief Update adapter (call each timestep)
 */
int soma_adapter_update(soma_adapter_t adapter, float dt);

/**
 * @brief Process pending inputs
 */
int soma_adapter_process_inputs(soma_adapter_t adapter);

/**
 * @brief Flush outputs to connected modules
 */
int soma_adapter_flush_outputs(soma_adapter_t adapter);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Check if adapter is ready
 */
bool soma_adapter_is_ready(soma_adapter_t adapter);

/**
 * @brief Check if adapter is running
 */
bool soma_adapter_is_running(soma_adapter_t adapter);

/**
 * @brief Get adapter health score
 */
float soma_adapter_get_health(soma_adapter_t adapter);

/**
 * @brief Get underlying somatosensory module
 */
nimcp_somatosensory_t* soma_adapter_get_module(soma_adapter_t adapter);

/**
 * @brief Get adapter statistics
 */
int soma_adapter_get_stats(soma_adapter_t adapter,
                           uint32_t* messages_received,
                           uint32_t* messages_sent,
                           float* avg_latency_ms);

/**
 * @brief Log adapter diagnostics
 */
int soma_adapter_log_diagnostics(soma_adapter_t adapter);

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

/**
 * @brief Register adapter in KG wiring system
 */
int soma_adapter_register_kg(soma_adapter_t adapter, void* kg_ctx);

/**
 * @brief Get KG node ID
 */
uint32_t soma_adapter_get_kg_node_id(soma_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMATOSENSORY_ADAPTER_H */

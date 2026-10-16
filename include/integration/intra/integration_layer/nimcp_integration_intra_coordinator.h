/**
 * @file nimcp_integration_intra_coordinator.h
 * @brief Integration Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between global integration modules
 * WHY:  Integration modules bind information across the brain
 * HOW:  Manages global binding, consciousness-related processes
 *
 * INTEGRATION LAYER MODULES:
 * ==========================
 * - Claustrum: Global binding, consciousness integration
 * - Periaqueductal Gray (PAG): Pain modulation, defensive behavior
 * - Red Nucleus: Motor coordination, cerebellar relay
 * - Reticular Formation: Arousal, sleep-wake, attention gating
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTEGRATION_INTRA_COORDINATOR_H
#define NIMCP_INTEGRATION_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define INTEGRATION_MODULE_CLAUSTRUM    0x0001
#define INTEGRATION_MODULE_PAG          0x0002
#define INTEGRATION_MODULE_RED_NUCLEUS  0x0003
#define INTEGRATION_MODULE_RETICULAR    0x0004
#define INTEGRATION_MODULE_COUNT        4

/* Message types */
#define INTEGRATION_MSG_BIND            (NIMCP_LAYER_MSG_MODULE_BASE + 0x0070)
#define INTEGRATION_MSG_UNBIND          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0071)
#define INTEGRATION_MSG_PAIN            (NIMCP_LAYER_MSG_MODULE_BASE + 0x0072)
#define INTEGRATION_MSG_DEFENSIVE       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0073)
#define INTEGRATION_MSG_MOTOR_COORD     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0074)
#define INTEGRATION_MSG_AROUSAL         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0075)
#define INTEGRATION_MSG_ATTENTION_GATE  (NIMCP_LAYER_MSG_MODULE_BASE + 0x0076)

typedef struct nimcp_integration_intra_struct* nimcp_integration_intra_t;

typedef struct {
    bool enable_claustrum;
    bool enable_pag;
    bool enable_red_nucleus;
    bool enable_reticular;
    float claustrum_pag_coupling;
    float claustrum_red_coupling;
    float claustrum_reticular_coupling;
    float pag_red_coupling;
    float pag_reticular_coupling;
    float red_reticular_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_global_binding;
    bool enable_logging;
    bool enable_metrics;
} nimcp_integration_intra_config_t;

typedef struct {
    bool claustrum_active;
    bool pag_active;
    bool red_nucleus_active;
    bool reticular_active;
    float binding_strength;
    float arousal_level;
    float pain_level;
    float motor_coordination;
    float global_coherence;
    float layer_coherence;
} nimcp_integration_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bind_events;
    uint64_t pain_events;
    uint64_t defensive_events;
    uint64_t arousal_changes;
    float avg_binding_strength;
    float avg_global_coherence;
    float avg_coherence;
} nimcp_integration_intra_stats_t;

NIMCP_EXPORT nimcp_integration_intra_config_t nimcp_integration_intra_default_config(void);
NIMCP_EXPORT nimcp_integration_intra_t nimcp_integration_intra_create(const nimcp_integration_intra_config_t* config);
NIMCP_EXPORT void nimcp_integration_intra_destroy(nimcp_integration_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_init(nimcp_integration_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_shutdown(nimcp_integration_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_connect_claustrum(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_connect_pag(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_connect_red_nucleus(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_connect_reticular(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_update(nimcp_integration_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_sync(nimcp_integration_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_send(nimcp_integration_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_broadcast(nimcp_integration_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_get_state(nimcp_integration_intra_t coord, nimcp_integration_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_get_stats(nimcp_integration_intra_t coord, nimcp_integration_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_integration_intra_get_coherence(nimcp_integration_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_intra_reset_stats(nimcp_integration_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTEGRATION_INTRA_COORDINATOR_H */

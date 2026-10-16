/**
 * @file nimcp_executive_intra_coordinator.h
 * @brief Executive Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between executive function modules
 * WHY:  Executive regions must work together for coherent decision-making
 * HOW:  Manages prefrontal interactions and goal-directed behavior
 *
 * EXECUTIVE LAYER MODULES:
 * ========================
 * - Orbitofrontal Cortex (OFC): Value-based decisions, reward processing
 * - Retrosplenial Cortex: Spatial memory, navigation, scene processing
 * - Prefrontal Cortex (PFC): Working memory, planning, cognitive control
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXECUTIVE_INTRA_COORDINATOR_H
#define NIMCP_EXECUTIVE_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define EXECUTIVE_MODULE_OFC            0x0001
#define EXECUTIVE_MODULE_RETROSPLENIAL  0x0002
#define EXECUTIVE_MODULE_PFC            0x0003
#define EXECUTIVE_MODULE_COUNT          3

/* Message types */
#define EXECUTIVE_MSG_VALUE_UPDATE      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0060)
#define EXECUTIVE_MSG_DECISION          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0061)
#define EXECUTIVE_MSG_GOAL_UPDATE       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0062)
#define EXECUTIVE_MSG_PLAN_UPDATE       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0063)
#define EXECUTIVE_MSG_INHIBIT           (NIMCP_LAYER_MSG_MODULE_BASE + 0x0064)
#define EXECUTIVE_MSG_SPATIAL_NAV       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0065)

typedef struct nimcp_executive_intra_struct* nimcp_executive_intra_t;

typedef struct {
    bool enable_ofc;
    bool enable_retrosplenial;
    bool enable_pfc;
    float ofc_retro_coupling;
    float ofc_pfc_coupling;
    float retro_pfc_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_goal_maintenance;
    bool enable_logging;
    bool enable_metrics;
} nimcp_executive_intra_config_t;

typedef struct {
    bool ofc_active;
    bool retrosplenial_active;
    bool pfc_active;
    float current_value;
    float goal_strength;
    float inhibition_level;
    float planning_progress;
    float layer_coherence;
} nimcp_executive_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t value_updates;
    uint64_t decisions_made;
    uint64_t goal_updates;
    uint64_t inhibitions;
    float avg_decision_latency;
    float avg_coherence;
} nimcp_executive_intra_stats_t;

NIMCP_EXPORT nimcp_executive_intra_config_t nimcp_executive_intra_default_config(void);
NIMCP_EXPORT nimcp_executive_intra_t nimcp_executive_intra_create(const nimcp_executive_intra_config_t* config);
NIMCP_EXPORT void nimcp_executive_intra_destroy(nimcp_executive_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_init(nimcp_executive_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_shutdown(nimcp_executive_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_connect_ofc(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_connect_retrosplenial(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_connect_pfc(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_update(nimcp_executive_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_sync(nimcp_executive_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_send(nimcp_executive_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_broadcast(nimcp_executive_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_get_state(nimcp_executive_intra_t coord, nimcp_executive_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_get_stats(nimcp_executive_intra_t coord, nimcp_executive_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_executive_intra_get_coherence(nimcp_executive_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_intra_reset_stats(nimcp_executive_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_INTRA_COORDINATOR_H */

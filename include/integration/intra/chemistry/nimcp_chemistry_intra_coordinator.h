/**
 * @file nimcp_chemistry_intra_coordinator.h
 * @brief Chemistry Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between modules within the Chemistry layer
 * WHY:  Chemistry layer modules must interact for coherent chemical signaling
 * HOW:  Manages intra-layer messaging, concentration tracking, and reactions
 *
 * CHEMISTRY LAYER MODULES:
 * ========================
 * - pH Regulation: Acid-base balance, pH-dependent processes
 * - NO Signaling: Nitric oxide signaling pathways
 * - Neurovascular: Blood-brain barrier, vascular coupling
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CHEMISTRY_INTRA_COORDINATOR_H
#define NIMCP_CHEMISTRY_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define CHEMISTRY_MODULE_PH             0x0001
#define CHEMISTRY_MODULE_NO_SIGNALING   0x0002
#define CHEMISTRY_MODULE_NEUROVASCULAR  0x0003
#define CHEMISTRY_MODULE_COUNT          3

/* Message types */
#define CHEMISTRY_MSG_PH_CHANGE         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0010)
#define CHEMISTRY_MSG_NO_RELEASE        (NIMCP_LAYER_MSG_MODULE_BASE + 0x0011)
#define CHEMISTRY_MSG_BLOOD_FLOW        (NIMCP_LAYER_MSG_MODULE_BASE + 0x0012)
#define CHEMISTRY_MSG_CONCENTRATION     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0013)

typedef struct nimcp_chemistry_intra_struct* nimcp_chemistry_intra_t;

typedef struct {
    bool enable_ph;
    bool enable_no_signaling;
    bool enable_neurovascular;
    float ph_no_coupling;
    float ph_vascular_coupling;
    float no_vascular_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    float baseline_ph;
    bool enable_logging;
    bool enable_metrics;
} nimcp_chemistry_intra_config_t;

typedef struct {
    bool ph_active;
    bool no_signaling_active;
    bool neurovascular_active;
    float current_ph;
    float no_concentration;
    float blood_flow_rate;
    float layer_coherence;
} nimcp_chemistry_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t ph_changes;
    uint64_t no_releases;
    uint64_t blood_flow_events;
    float avg_ph;
    float avg_no_concentration;
    float avg_coherence;
} nimcp_chemistry_intra_stats_t;

NIMCP_EXPORT nimcp_chemistry_intra_config_t nimcp_chemistry_intra_default_config(void);
NIMCP_EXPORT nimcp_chemistry_intra_t nimcp_chemistry_intra_create(const nimcp_chemistry_intra_config_t* config);
NIMCP_EXPORT void nimcp_chemistry_intra_destroy(nimcp_chemistry_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_init(nimcp_chemistry_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_shutdown(nimcp_chemistry_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_connect_ph(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_connect_no_signaling(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_connect_neurovascular(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_update(nimcp_chemistry_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_sync(nimcp_chemistry_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_send(nimcp_chemistry_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_broadcast(nimcp_chemistry_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_get_state(nimcp_chemistry_intra_t coord, nimcp_chemistry_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_get_stats(nimcp_chemistry_intra_t coord, nimcp_chemistry_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_chemistry_intra_get_coherence(nimcp_chemistry_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_intra_reset_stats(nimcp_chemistry_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHEMISTRY_INTRA_COORDINATOR_H */

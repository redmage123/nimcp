/**
 * @file nimcp_sensory_intra_coordinator.h
 * @brief Sensory Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between sensory processing modules
 * WHY:  Sensory modalities must integrate for coherent perception
 * HOW:  Manages cross-modal binding and multi-sensory integration
 *
 * SENSORY LAYER MODULES:
 * ======================
 * - Somatosensory: Touch, proprioception, temperature
 * - Olfactory: Smell processing
 * - Gustatory: Taste processing
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SENSORY_INTRA_COORDINATOR_H
#define NIMCP_SENSORY_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define SENSORY_MODULE_SOMATOSENSORY    0x0001
#define SENSORY_MODULE_OLFACTORY        0x0002
#define SENSORY_MODULE_GUSTATORY        0x0003
#define SENSORY_MODULE_COUNT            3

/* Message types */
#define SENSORY_MSG_TOUCH               (NIMCP_LAYER_MSG_MODULE_BASE + 0x0040)
#define SENSORY_MSG_PROPRIOCEPTION      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0041)
#define SENSORY_MSG_TEMPERATURE         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0042)
#define SENSORY_MSG_SMELL               (NIMCP_LAYER_MSG_MODULE_BASE + 0x0043)
#define SENSORY_MSG_TASTE               (NIMCP_LAYER_MSG_MODULE_BASE + 0x0044)
#define SENSORY_MSG_CROSSMODAL          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0045)

typedef struct nimcp_sensory_intra_struct* nimcp_sensory_intra_t;

typedef struct {
    bool enable_somatosensory;
    bool enable_olfactory;
    bool enable_gustatory;
    float somato_olfactory_coupling;
    float somato_gustatory_coupling;
    float olfactory_gustatory_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_crossmodal_binding;
    bool enable_logging;
    bool enable_metrics;
} nimcp_sensory_intra_config_t;

typedef struct {
    bool somatosensory_active;
    bool olfactory_active;
    bool gustatory_active;
    float touch_intensity;
    float smell_intensity;
    float taste_intensity;
    float crossmodal_binding_strength;
    float layer_coherence;
} nimcp_sensory_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t touch_events;
    uint64_t smell_events;
    uint64_t taste_events;
    uint64_t crossmodal_bindings;
    float avg_coherence;
} nimcp_sensory_intra_stats_t;

NIMCP_EXPORT nimcp_sensory_intra_config_t nimcp_sensory_intra_default_config(void);
NIMCP_EXPORT nimcp_sensory_intra_t nimcp_sensory_intra_create(const nimcp_sensory_intra_config_t* config);
NIMCP_EXPORT void nimcp_sensory_intra_destroy(nimcp_sensory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_init(nimcp_sensory_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_shutdown(nimcp_sensory_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_connect_somatosensory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_connect_olfactory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_connect_gustatory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_update(nimcp_sensory_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_sync(nimcp_sensory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_send(nimcp_sensory_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_broadcast(nimcp_sensory_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_get_state(nimcp_sensory_intra_t coord, nimcp_sensory_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_get_stats(nimcp_sensory_intra_t coord, nimcp_sensory_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_sensory_intra_get_coherence(nimcp_sensory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_intra_reset_stats(nimcp_sensory_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSORY_INTRA_COORDINATOR_H */

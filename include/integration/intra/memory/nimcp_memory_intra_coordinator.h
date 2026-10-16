/**
 * @file nimcp_memory_intra_coordinator.h
 * @brief Memory Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between memory circuit modules
 * WHY:  Memory systems must work together for coherent encoding/retrieval
 * HOW:  Manages hippocampal-cortical interactions and memory consolidation
 *
 * MEMORY LAYER MODULES:
 * =====================
 * - Entorhinal Cortex (EC): Gateway to hippocampus, grid cells
 * - Perirhinal Cortex (PRC): Object recognition memory
 * - Parahippocampal Cortex (PHC): Spatial/contextual memory
 * - Mammillary Bodies (MB): Memory consolidation, thalamic relay
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MEMORY_INTRA_COORDINATOR_H
#define NIMCP_MEMORY_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define MEMORY_MODULE_ENTORHINAL        0x0001
#define MEMORY_MODULE_PERIRHINAL        0x0002
#define MEMORY_MODULE_PARAHIPPOCAMPAL   0x0003
#define MEMORY_MODULE_MAMMILLARY        0x0004
#define MEMORY_MODULE_COUNT             4

/* Message types */
#define MEMORY_MSG_ENCODE               (NIMCP_LAYER_MSG_MODULE_BASE + 0x0050)
#define MEMORY_MSG_RETRIEVE             (NIMCP_LAYER_MSG_MODULE_BASE + 0x0051)
#define MEMORY_MSG_CONSOLIDATE          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0052)
#define MEMORY_MSG_GRID_CELL            (NIMCP_LAYER_MSG_MODULE_BASE + 0x0053)
#define MEMORY_MSG_OBJECT_RECOG         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0054)
#define MEMORY_MSG_SPATIAL_CONTEXT      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0055)
#define MEMORY_MSG_THETA_PHASE          (NIMCP_LAYER_MSG_MODULE_BASE + 0x0056)

typedef struct nimcp_memory_intra_struct* nimcp_memory_intra_t;

typedef struct {
    bool enable_entorhinal;
    bool enable_perirhinal;
    bool enable_parahippocampal;
    bool enable_mammillary;
    float ec_prc_coupling;
    float ec_phc_coupling;
    float ec_mb_coupling;
    float prc_phc_coupling;
    float prc_mb_coupling;
    float phc_mb_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_theta_gating;
    bool enable_logging;
    bool enable_metrics;
} nimcp_memory_intra_config_t;

typedef struct {
    bool entorhinal_active;
    bool perirhinal_active;
    bool parahippocampal_active;
    bool mammillary_active;
    float theta_phase;
    float encoding_strength;
    float retrieval_strength;
    float consolidation_progress;
    float layer_coherence;
} nimcp_memory_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t encode_events;
    uint64_t retrieve_events;
    uint64_t consolidation_events;
    uint64_t theta_cycles;
    float avg_encoding_strength;
    float avg_retrieval_accuracy;
    float avg_coherence;
} nimcp_memory_intra_stats_t;

NIMCP_EXPORT nimcp_memory_intra_config_t nimcp_memory_intra_default_config(void);
NIMCP_EXPORT nimcp_memory_intra_t nimcp_memory_intra_create(const nimcp_memory_intra_config_t* config);
NIMCP_EXPORT void nimcp_memory_intra_destroy(nimcp_memory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_init(nimcp_memory_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_shutdown(nimcp_memory_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_connect_entorhinal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_connect_perirhinal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_connect_parahippocampal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_connect_mammillary(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_update(nimcp_memory_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_sync(nimcp_memory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_send(nimcp_memory_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_broadcast(nimcp_memory_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_get_state(nimcp_memory_intra_t coord, nimcp_memory_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_get_stats(nimcp_memory_intra_t coord, nimcp_memory_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_memory_intra_get_coherence(nimcp_memory_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_intra_reset_stats(nimcp_memory_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_INTRA_COORDINATOR_H */

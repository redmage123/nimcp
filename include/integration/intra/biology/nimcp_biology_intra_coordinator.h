/**
 * @file nimcp_biology_intra_coordinator.h
 * @brief Biology Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between modules within the Biology layer
 * WHY:  Biology layer modules must interact for coherent biological processes
 * HOW:  Manages intra-layer messaging, gene expression cascades, and cell processes
 *
 * BIOLOGY LAYER MODULES:
 * ======================
 * - Epigenetics: DNA methylation, histone modification
 * - Neurogenesis: Neural stem cells, differentiation
 * - Gene Expression: Transcription, translation, protein synthesis
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BIOLOGY_INTRA_COORDINATOR_H
#define NIMCP_BIOLOGY_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define BIOLOGY_MODULE_EPIGENETICS      0x0001
#define BIOLOGY_MODULE_NEUROGENESIS     0x0002
#define BIOLOGY_MODULE_GENE_EXPRESSION  0x0003
#define BIOLOGY_MODULE_COUNT            3

/* Message types */
#define BIOLOGY_MSG_METHYLATION         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0020)
#define BIOLOGY_MSG_HISTONE_MOD         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0021)
#define BIOLOGY_MSG_CELL_DIVISION       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0022)
#define BIOLOGY_MSG_DIFFERENTIATION     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0023)
#define BIOLOGY_MSG_TRANSCRIPTION       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0024)
#define BIOLOGY_MSG_TRANSLATION         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0025)

typedef struct nimcp_biology_intra_struct* nimcp_biology_intra_t;

typedef struct {
    bool enable_epigenetics;
    bool enable_neurogenesis;
    bool enable_gene_expression;
    float epigenetics_genesis_coupling;
    float epigenetics_expression_coupling;
    float genesis_expression_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_logging;
    bool enable_metrics;
} nimcp_biology_intra_config_t;

typedef struct {
    bool epigenetics_active;
    bool neurogenesis_active;
    bool gene_expression_active;
    float methylation_level;
    float neurogenesis_rate;
    float expression_level;
    float layer_coherence;
} nimcp_biology_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t methylation_events;
    uint64_t cell_divisions;
    uint64_t transcription_events;
    float avg_methylation;
    float avg_expression;
    float avg_coherence;
} nimcp_biology_intra_stats_t;

NIMCP_EXPORT nimcp_biology_intra_config_t nimcp_biology_intra_default_config(void);
NIMCP_EXPORT nimcp_biology_intra_t nimcp_biology_intra_create(const nimcp_biology_intra_config_t* config);
NIMCP_EXPORT void nimcp_biology_intra_destroy(nimcp_biology_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_init(nimcp_biology_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_shutdown(nimcp_biology_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_connect_epigenetics(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_connect_neurogenesis(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_connect_gene_expression(nimcp_biology_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_update(nimcp_biology_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_sync(nimcp_biology_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_send(nimcp_biology_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_broadcast(nimcp_biology_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_get_state(nimcp_biology_intra_t coord, nimcp_biology_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_get_stats(nimcp_biology_intra_t coord, nimcp_biology_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_biology_intra_get_coherence(nimcp_biology_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_intra_reset_stats(nimcp_biology_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGY_INTRA_COORDINATOR_H */

/**
 * @file nimcp_biology_neuromod_bridge.h
 * @brief Biology-Neuromodulatory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges biological processes to neuromodulatory systems
 * WHY:  Neuromodulator release depends on cellular state
 * HOW:  Translates cellular health/activity to modulator levels
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Biology → Neuromodulatory):
 * - Gene expression → receptor density
 * - Neurogenesis → new circuit integration
 * - Cellular energy → firing capacity
 *
 * Top-Down (Neuromodulatory → Biology):
 * - DA/NE levels → BDNF expression
 * - 5-HT → neuroplasticity genes
 * - Stress hormones → epigenetic marks
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BIOLOGY_NEUROMOD_BRIDGE_H
#define NIMCP_BIOLOGY_NEUROMOD_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/biology/nimcp_biology_intra_coordinator.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define BIO_NEURO_MSG_RECEPTOR_DENSITY  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0020)
#define BIO_NEURO_MSG_CIRCUIT_INTEGRATE (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0021)
#define BIO_NEURO_MSG_ENERGY_CAPACITY   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0022)
#define BIO_NEURO_MSG_BDNF_EXPRESS      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0023)
#define BIO_NEURO_MSG_PLASTICITY_GENE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0024)
#define BIO_NEURO_MSG_EPIGENETIC_MARK   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0025)

typedef struct nimcp_biology_neuromod_bridge_struct* nimcp_biology_neuromod_bridge_t;

typedef struct {
    float receptor_density_coupling;
    float neurogenesis_integration_rate;
    float bdnf_expression_gain;
    float plasticity_gene_sensitivity;
    uint32_t update_interval_ms;
    bool enable_activity_dependent_plasticity;
    bool enable_logging;
    bool enable_metrics;
} nimcp_biology_neuromod_config_t;

typedef struct {
    float receptor_density_level;
    float circuit_integration_progress;
    float energy_capacity;
    float bdnf_expression_level;
    float plasticity_gene_activity;
    float epigenetic_modification_strength;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_biology_neuromod_state_t;

typedef struct {
    uint64_t receptor_density_changes;
    uint64_t circuit_integrations;
    uint64_t bdnf_expressions;
    uint64_t plasticity_gene_events;
    uint64_t epigenetic_marks;
    float avg_receptor_density;
    float avg_bdnf_level;
} nimcp_biology_neuromod_stats_t;

NIMCP_EXPORT nimcp_biology_neuromod_config_t nimcp_biology_neuromod_default_config(void);
NIMCP_EXPORT nimcp_biology_neuromod_bridge_t nimcp_biology_neuromod_create(const nimcp_biology_neuromod_config_t* config);
NIMCP_EXPORT void nimcp_biology_neuromod_destroy(nimcp_biology_neuromod_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_init(
    nimcp_biology_neuromod_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_biology_intra_t biology,
    nimcp_neuromod_intra_t neuromod
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_shutdown(nimcp_biology_neuromod_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_update(nimcp_biology_neuromod_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_transfer_bottom_up(nimcp_biology_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_transfer_top_down(nimcp_biology_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_get_state(nimcp_biology_neuromod_bridge_t bridge, nimcp_biology_neuromod_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_get_stats(nimcp_biology_neuromod_bridge_t bridge, nimcp_biology_neuromod_stats_t* stats_out);
NIMCP_EXPORT float nimcp_biology_neuromod_get_coherence(nimcp_biology_neuromod_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_biology_neuromod_reset_stats(nimcp_biology_neuromod_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGY_NEUROMOD_BRIDGE_H */

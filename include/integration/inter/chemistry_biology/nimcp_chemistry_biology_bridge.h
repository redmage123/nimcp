/**
 * @file nimcp_chemistry_biology_bridge.h
 * @brief Chemistry-Biology Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges chemical processes to biological functions
 * WHY:  Biological processes depend on chemical substrates
 * HOW:  Translates molecular signals to cellular responses
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Chemistry → Biology):
 * - Neurotransmitter concentration → receptor activation
 * - pH levels → protein function
 * - NO signaling → gene expression triggers
 *
 * Top-Down (Biology → Chemistry):
 * - Gene expression → protein synthesis requests
 * - Epigenetic signals → transcription factor binding
 * - Neurogenesis → growth factor release
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CHEMISTRY_BIOLOGY_BRIDGE_H
#define NIMCP_CHEMISTRY_BIOLOGY_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/chemistry/nimcp_chemistry_intra_coordinator.h"
#include "integration/intra/biology/nimcp_biology_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define CHEM_BIO_MSG_RECEPTOR_ACTIVATE  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0010)
#define CHEM_BIO_MSG_PH_EFFECT          (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0011)
#define CHEM_BIO_MSG_NO_SIGNAL          (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0012)
#define CHEM_BIO_MSG_PROTEIN_REQUEST    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0013)
#define CHEM_BIO_MSG_EPIGENETIC         (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0014)
#define CHEM_BIO_MSG_GROWTH_FACTOR      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0015)

typedef struct nimcp_chemistry_biology_bridge_struct* nimcp_chemistry_biology_bridge_t;

typedef struct {
    float receptor_coupling_strength;
    float ph_sensitivity;
    float no_signaling_gain;
    float protein_synthesis_rate;
    uint32_t update_interval_ms;
    bool enable_receptor_dynamics;
    bool enable_logging;
    bool enable_metrics;
} nimcp_chemistry_biology_config_t;

typedef struct {
    float receptor_activation_level;
    float ph_effect_magnitude;
    float no_signal_strength;
    float protein_synthesis_load;
    float epigenetic_activity;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_chemistry_biology_state_t;

typedef struct {
    uint64_t receptor_activations;
    uint64_t ph_effect_events;
    uint64_t no_signal_events;
    uint64_t protein_requests;
    uint64_t epigenetic_events;
    float avg_receptor_activation;
    float avg_synthesis_rate;
} nimcp_chemistry_biology_stats_t;

NIMCP_EXPORT nimcp_chemistry_biology_config_t nimcp_chemistry_biology_default_config(void);
NIMCP_EXPORT nimcp_chemistry_biology_bridge_t nimcp_chemistry_biology_create(const nimcp_chemistry_biology_config_t* config);
NIMCP_EXPORT void nimcp_chemistry_biology_destroy(nimcp_chemistry_biology_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_init(
    nimcp_chemistry_biology_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_chemistry_intra_t chemistry,
    nimcp_biology_intra_t biology
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_shutdown(nimcp_chemistry_biology_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_update(nimcp_chemistry_biology_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_transfer_bottom_up(nimcp_chemistry_biology_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_transfer_top_down(nimcp_chemistry_biology_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_get_state(nimcp_chemistry_biology_bridge_t bridge, nimcp_chemistry_biology_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_get_stats(nimcp_chemistry_biology_bridge_t bridge, nimcp_chemistry_biology_stats_t* stats_out);
NIMCP_EXPORT float nimcp_chemistry_biology_get_coherence(nimcp_chemistry_biology_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_chemistry_biology_reset_stats(nimcp_chemistry_biology_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHEMISTRY_BIOLOGY_BRIDGE_H */

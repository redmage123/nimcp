/**
 * @file nimcp_neuromod_intra_coordinator.h
 * @brief Neuromodulatory Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between neuromodulatory nuclei
 * WHY:  Neuromodulatory systems interact for coherent arousal/motivation control
 * HOW:  Manages cross-talk between LC, VTA, Raphe, Habenula
 *
 * NEUROMODULATORY LAYER MODULES:
 * ==============================
 * - Locus Coeruleus (LC): Norepinephrine, arousal, attention
 * - Ventral Tegmental Area (VTA): Dopamine, reward, motivation
 * - Raphe Nuclei: Serotonin, mood, impulse control
 * - Habenula: Disappointment, learning from negative outcomes
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_INTRA_COORDINATOR_H
#define NIMCP_NEUROMOD_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module IDs */
#define NEUROMOD_MODULE_LC              0x0001
#define NEUROMOD_MODULE_VTA             0x0002
#define NEUROMOD_MODULE_RAPHE           0x0003
#define NEUROMOD_MODULE_HABENULA        0x0004
#define NEUROMOD_MODULE_COUNT           4

/* Message types */
#define NEUROMOD_MSG_NE_RELEASE         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0030)
#define NEUROMOD_MSG_DA_RELEASE         (NIMCP_LAYER_MSG_MODULE_BASE + 0x0031)
#define NEUROMOD_MSG_5HT_RELEASE        (NIMCP_LAYER_MSG_MODULE_BASE + 0x0032)
#define NEUROMOD_MSG_AROUSAL_CHANGE     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0033)
#define NEUROMOD_MSG_REWARD_SIGNAL      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0034)
#define NEUROMOD_MSG_DISAPPOINTMENT     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0035)

typedef struct nimcp_neuromod_intra_struct* nimcp_neuromod_intra_t;

typedef struct {
    bool enable_lc;
    bool enable_vta;
    bool enable_raphe;
    bool enable_habenula;
    float lc_vta_coupling;
    float lc_raphe_coupling;
    float lc_habenula_coupling;
    float vta_raphe_coupling;
    float vta_habenula_coupling;
    float raphe_habenula_coupling;
    uint32_t sync_interval_ms;
    float coherence_threshold;
    bool enable_logging;
    bool enable_metrics;
} nimcp_neuromod_intra_config_t;

typedef struct {
    bool lc_active;
    bool vta_active;
    bool raphe_active;
    bool habenula_active;
    float norepinephrine_level;
    float dopamine_level;
    float serotonin_level;
    float arousal_level;
    float reward_prediction_error;
    float layer_coherence;
} nimcp_neuromod_intra_state_t;

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t ne_releases;
    uint64_t da_releases;
    uint64_t serotonin_releases;
    uint64_t reward_signals;
    float avg_arousal;
    float avg_dopamine;
    float avg_coherence;
} nimcp_neuromod_intra_stats_t;

NIMCP_EXPORT nimcp_neuromod_intra_config_t nimcp_neuromod_intra_default_config(void);
NIMCP_EXPORT nimcp_neuromod_intra_t nimcp_neuromod_intra_create(const nimcp_neuromod_intra_config_t* config);
NIMCP_EXPORT void nimcp_neuromod_intra_destroy(nimcp_neuromod_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_init(nimcp_neuromod_intra_t coord, nimcp_layer_registry_t registry);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_shutdown(nimcp_neuromod_intra_t coord);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_connect_lc(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_connect_vta(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_connect_raphe(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_connect_habenula(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_update(nimcp_neuromod_intra_t coord, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_sync(nimcp_neuromod_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_send(nimcp_neuromod_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_broadcast(nimcp_neuromod_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_get_state(nimcp_neuromod_intra_t coord, nimcp_neuromod_intra_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_get_stats(nimcp_neuromod_intra_t coord, nimcp_neuromod_intra_stats_t* stats_out);
NIMCP_EXPORT float nimcp_neuromod_intra_get_coherence(nimcp_neuromod_intra_t coord);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_intra_reset_stats(nimcp_neuromod_intra_t coord);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_INTRA_COORDINATOR_H */

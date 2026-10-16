//=============================================================================
// nimcp_subcortical_runtime_events.h - Per-Subcortical-Module Runtime Events
//=============================================================================
/**
 * @file nimcp_subcortical_runtime_events.h
 * @brief Wave W4 runtime emitters for 8 subcortical modules.
 *
 * WHY: The umbrella `nimcp_subcortical_kg_wiring.c` only adds **structural**
 *      nodes (striatum, GPe, GPi, STN, SNc, SNr, NAcc, SC, IC, thalamus) at
 *      brain init. It never emits runtime events. This file fills that gap:
 *      per-module action/decision events that are linked back to the
 *      structural parent.
 *
 * All 8 emit functions:
 *  - take `struct brain_struct*` (void-safe if NULL or kg disabled),
 *  - self-elevate to ADMIN via `brain->internal_kg_admin_token`
 *    (see docs/claude/kg-node-naming-registry.md §7),
 *  - generate a unique event-node name with a microsecond timestamp,
 *  - link the event back to the structural subcortical parent via
 *    `BRAIN_KG_EDGE_SENDS_TO` (description: `produced_by`),
 *  - restore access level to READ before returning.
 *
 * The 8 emitters (see audit §2.1 footer):
 *   basal_ganglia / striatum : subcortical_emit_action_selected
 *   globus_pallidus          : subcortical_emit_inhibition
 *   STN                      : subcortical_emit_stop_signal
 *   substantia_nigra         : subcortical_emit_dopamine_release
 *   NAcc                     : subcortical_emit_reward_prediction
 *   superior_colliculus      : subcortical_emit_orienting_response
 *   inferior_colliculus      : subcortical_emit_auditory_localization
 *   thalamus (subcortical)   : subcortical_emit_routing_decision
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_SUBCORTICAL_RUNTIME_EVENTS_H
#define NIMCP_SUBCORTICAL_RUNTIME_EVENTS_H

#include <stdint.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/**
 * @brief Action selected by basal ganglia / striatum.
 *
 * Event node: `striatum_event_action_<id>_<ts>`.
 * Parent link: `basal_ganglia` (structural node).
 *
 * @param brain       Brain handle.
 * @param action_id   Winning action id from BG competition.
 * @param confidence  Thalamic disinhibition margin [0..1].
 */
NIMCP_EXPORT void subcortical_emit_action_selected(
    struct brain_struct* brain,
    uint32_t action_id,
    float confidence
);

/** Globus pallidus inhibition event. Node: `gp_event_inhibit_<ts>`. */
NIMCP_EXPORT void subcortical_emit_inhibition(
    struct brain_struct* brain,
    float strength
);

/** Subthalamic nucleus stop signal. Node: `stn_event_stop_<ts>`. */
NIMCP_EXPORT void subcortical_emit_stop_signal(
    struct brain_struct* brain
);

/**
 * Substantia nigra DA release.
 *
 * Distinct from VTA (see W2 vta wiring). SN writes to `sn_event_da_<ts>`
 * and links to the `substantia_nigra_compacta` structural node if present.
 */
NIMCP_EXPORT void subcortical_emit_dopamine_release(
    struct brain_struct* brain,
    float amount
);

/** NAcc reward prediction error. Node: `nacc_event_rpe_<ts>`. */
NIMCP_EXPORT void subcortical_emit_reward_prediction(
    struct brain_struct* brain,
    float rpe
);

/** Superior colliculus orienting response. Node: `sc_event_orient_<ts>`. */
NIMCP_EXPORT void subcortical_emit_orienting_response(
    struct brain_struct* brain,
    float x,
    float y
);

/** Inferior colliculus auditory localization. Node: `ic_event_loc_<ts>`. */
NIMCP_EXPORT void subcortical_emit_auditory_localization(
    struct brain_struct* brain,
    float azimuth,
    float elevation
);

/**
 * @brief Thalamus routing decision event. Node: `thalamus_event_route_<ts>`.
 *
 * This is the **subcortical thalamus** (relay nucleus routing), distinct
 * from the brain-level thalamic_router (which queues thalamic messages).
 */
NIMCP_EXPORT void subcortical_emit_routing_decision(
    struct brain_struct* brain,
    uint32_t src_idx,
    uint32_t dst_idx
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUBCORTICAL_RUNTIME_EVENTS_H */

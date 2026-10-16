/**
 * @file nimcp_global_workspace_fep_bridge.h
 * @brief Free Energy Principle - Global Workspace Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and global workspace
 * WHY:  Global workspace broadcasts winning hypotheses (beliefs with highest evidence);
 *       broadcast content becomes shared prior across modules
 * HOW:  FEP beliefs compete for workspace access; workspace broadcasts update FEP
 *       priors across hierarchy
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * GLOBAL WORKSPACE AS BELIEF BROADCASTING:
 * ---------------------------------------
 * 1. Conscious Access as High-Evidence Beliefs:
 *    - Workspace content = beliefs with highest model evidence
 *    - Ignition = beliefs exceeding evidence threshold
 *    - Reference: Dehaene & Changeux (2011) "Experimental and theoretical
 *      approaches to conscious processing"
 *
 * 2. Broadcast Updates Priors:
 *    - Workspace content → shared priors
 *    - Global availability → hierarchical belief update
 *    - Reference: Hohwy (2012) "Attention and conscious perception in the
 *      hypothesis testing brain"
 *
 * 3. Competition as Model Selection:
 *    - Multiple hypotheses compete
 *    - Winner = highest free energy reduction
 *    - Workspace = selected model
 *    - Reference: Friston (2010) "The free-energy principle: a unified brain theory"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GLOBAL_WORKSPACE_FEP_BRIDGE_H
#define NIMCP_GLOBAL_WORKSPACE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants & Structures
 * ============================================================================ */

#define BELIEF_EVIDENCE_THRESHOLD         0.7f
#define BROADCAST_PRIOR_UPDATE_RATE       0.3f

typedef struct global_workspace_fep_bridge global_workspace_fep_bridge_t;

typedef struct {
    float belief_evidence_threshold;
    float broadcast_prior_weight;
    float model_evidence_sensitivity;
    float competition_strength_factor;
    bool enable_belief_broadcasting;
    bool enable_prior_updates;
    bool enable_evidence_competition;
} global_workspace_fep_config_t;

typedef struct {
    float broadcast_prior_boost;
    float model_evidence;
    float competition_strength;
} global_workspace_fep_effects_t;

typedef struct {
    float current_belief_evidence;
    uint32_t broadcasts_from_beliefs;
    uint32_t prior_updates_from_broadcast;
    bool belief_in_workspace;
} global_workspace_fep_state_t;

typedef struct {
    uint64_t total_belief_broadcasts;
    uint64_t total_prior_updates;
    uint64_t total_competitions_won;
    float avg_broadcast_evidence;
} global_workspace_fep_stats_t;

struct global_workspace_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    global_workspace_fep_config_t config;
    fep_system_t* fep_system;
    global_workspace_t* workspace;
    global_workspace_fep_effects_t effects;
    global_workspace_fep_state_t state;
    global_workspace_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int global_workspace_fep_bridge_default_config(global_workspace_fep_config_t* config);
global_workspace_fep_bridge_t* global_workspace_fep_bridge_create(const global_workspace_fep_config_t* config);
void global_workspace_fep_bridge_destroy(global_workspace_fep_bridge_t* bridge);

int global_workspace_fep_bridge_connect_fep(global_workspace_fep_bridge_t* bridge, fep_system_t* fep);
int global_workspace_fep_bridge_connect_workspace(global_workspace_fep_bridge_t* bridge, global_workspace_t* workspace);

int global_workspace_fep_compete_with_beliefs(global_workspace_fep_bridge_t* bridge);
int global_workspace_fep_broadcast_winning_belief(global_workspace_fep_bridge_t* bridge);
int global_workspace_fep_update_priors_from_broadcast(global_workspace_fep_bridge_t* bridge);

int global_workspace_fep_bridge_update(global_workspace_fep_bridge_t* bridge, uint64_t delta_ms);
int global_workspace_fep_bridge_get_state(const global_workspace_fep_bridge_t* bridge, global_workspace_fep_state_t* state);
int global_workspace_fep_bridge_get_stats(const global_workspace_fep_bridge_t* bridge, global_workspace_fep_stats_t* stats);

int global_workspace_fep_bridge_connect_bio_async(global_workspace_fep_bridge_t* bridge);
int global_workspace_fep_bridge_disconnect_bio_async(global_workspace_fep_bridge_t* bridge);
bool global_workspace_fep_bridge_is_bio_async_connected(const global_workspace_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLOBAL_WORKSPACE_FEP_BRIDGE_H */

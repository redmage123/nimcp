/**
 * @file nimcp_global_workspace_fep_bridge.c
 * @brief Free Energy Principle - Global Workspace Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and global workspace
 * WHY:  Conscious access in GWT = precision-weighted belief propagation in FEP
 * HOW:  FEP beliefs compete for workspace; broadcast content updates priors
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * THEORETICAL INTEGRATION (Dehaene + Friston):
 * --------------------------------------------
 * This bridge implements the convergence between:
 * 1. Global Workspace Theory (Dehaene & Changeux, 2011)
 *    - Conscious access = global broadcast
 *    - Competition for limited capacity
 *    - Winner reaches prefrontal-parietal network
 *
 * 2. Free Energy Principle (Friston, 2010)
 *    - Beliefs minimize prediction error
 *    - Precision-weighted evidence
 *    - Hierarchical belief propagation
 *
 * MECHANISTIC MAPPING:
 * -------------------
 * FEP → GWT:
 *   - High-precision beliefs compete for workspace access
 *   - Model evidence → competition strength
 *   - Belief crossing threshold → workspace ignition
 *   - Winner = hypothesis with highest evidence
 *
 * GWT → FEP:
 *   - Broadcast content → shared prior across hierarchy
 *   - Global availability → prior update signal
 *   - Ignition threshold modulated by FEP precision
 *
 * KEY INSIGHT:
 * -----------
 * Consciousness (workspace content) represents the brain's current "best guess"
 * (highest evidence belief). Broadcasting this hypothesis allows all modules to
 * update their priors, enabling coordinated inference across the cognitive hierarchy.
 *
 * REFERENCES:
 * - Hohwy (2012) "Attention and conscious perception in the hypothesis testing brain"
 * - Dehaene, Changeux (2011) "Experimental and theoretical approaches to conscious processing"
 * - Friston (2010) "The free-energy principle: a unified brain theory"
 */

#include "cognitive/global_workspace/nimcp_global_workspace_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(global_workspace_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_global_workspace_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_global_workspace_fep_bridge_mesh_registry = NULL;

nimcp_error_t global_workspace_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_global_workspace_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "global_workspace_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "global_workspace_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_global_workspace_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_global_workspace_fep_bridge_mesh_registry = registry;
    return err;
}

void global_workspace_fep_bridge_mesh_unregister(void) {
    if (g_global_workspace_fep_bridge_mesh_registry && g_global_workspace_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_global_workspace_fep_bridge_mesh_registry, g_global_workspace_fep_bridge_mesh_id);
        g_global_workspace_fep_bridge_mesh_id = 0;
        g_global_workspace_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from global_workspace_fep_bridge module (instance-level) */
static inline void global_workspace_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_global_workspace_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_global_workspace_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_global_workspace_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute model evidence from FEP free energy
 * WHY:  Evidence = exp(-F) measures how well model explains data
 * HOW:  Transform free energy to evidence scale
 *
 * THEORY: Free energy F upper-bounds surprise: F ≥ -ln p(o)
 *         Therefore: p(o) ≥ exp(-F)
 *         We use exp(-F) as evidence proxy
 */
static float compute_model_evidence(float free_energy) {
    /* Prevent overflow for very negative free energy */
    float clamped_fe = nimcp_clampf(free_energy, -50.0f, 50.0f);
    return expf(-clamped_fe);
}

/**
 * WHAT: Compute competition strength from belief precision and model evidence
 * WHY:  High-precision, high-evidence beliefs should win workspace access
 * HOW:  Weighted combination of precision and evidence
 *
 * BIOLOGICAL: Precision = neuromodulatory gain (dopamine/acetylcholine)
 *             Evidence = population firing rates (cortical columns)
 *             Strength = gain × activity = workspace ignition signal
 */
static float compute_competition_strength(
    float precision,
    float evidence,
    float precision_weight,
    float evidence_weight
) {
    float precision_norm = nimcp_clampf(precision, 0.0f, 1.0f);
    float evidence_norm = nimcp_clampf(evidence, 0.0f, 1.0f);
    return precision_weight * precision_norm + evidence_weight * evidence_norm;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Initialize default configuration
 * WHY:  Provide sensible defaults for bridge operation
 * HOW:  Set biologically-plausible parameter values
 */
int global_workspace_fep_bridge_default_config(global_workspace_fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_default_config", 0.0f);


    config->belief_evidence_threshold = BELIEF_EVIDENCE_THRESHOLD;
    config->broadcast_prior_weight = BROADCAST_PRIOR_UPDATE_RATE;
    config->model_evidence_sensitivity = 1.0f;
    config->competition_strength_factor = 0.5f;

    config->enable_belief_broadcasting = true;
    config->enable_prior_updates = true;
    config->enable_evidence_competition = true;

    return 0;
}

/**
 * WHAT: Create FEP-workspace bridge
 * WHY:  Enable bidirectional integration between systems
 * HOW:  Allocate structure, initialize state, create mutex
 */
global_workspace_fep_bridge_t* global_workspace_fep_bridge_create(
    const global_workspace_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_create", 0.0f);


    global_workspace_fep_bridge_t* bridge = (global_workspace_fep_bridge_t*)nimcp_calloc(
        1, sizeof(global_workspace_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate global workspace FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    global_workspace_fep_config_t default_cfg;
    if (!config) {
        global_workspace_fep_bridge_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Initialize state */
    memset(&bridge->state, 0, sizeof(global_workspace_fep_state_t));
    bridge->state.belief_in_workspace = false;

    /* Initialize effects */
    memset(&bridge->effects, 0, sizeof(global_workspace_fep_effects_t));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(global_workspace_fep_stats_t));

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "global_workspace_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for global workspace FEP bridge");
        global_workspace_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Global workspace FEP bridge created");
    return bridge;
}

/**
 * WHAT: Destroy FEP-workspace bridge
 * WHY:  Clean resource management
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void global_workspace_fep_bridge_destroy(global_workspace_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        global_workspace_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Global workspace FEP bridge destroyed");
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

/**
 * WHAT: Connect bridge to FEP system
 * WHY:  Access FEP beliefs for workspace competition
 * HOW:  Store pointer, enable FEP → workspace direction
 */
int global_workspace_fep_bridge_connect_fep(
    global_workspace_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Allow NULL fep to disconnect/reset FEP connection */

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_connect_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Global workspace FEP bridge connected to FEP system");
    return 0;
}

/**
 * WHAT: Connect bridge to global workspace
 * WHY:  Access workspace for broadcasting and reading
 * HOW:  Store pointer, enable bidirectional flow
 */
int global_workspace_fep_bridge_connect_workspace(
    global_workspace_fep_bridge_t* bridge,
    global_workspace_t* workspace
) {
    if (!bridge || !workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_bridge_connect_workspace: required parameter is NULL (bridge, workspace)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_connect_workspace", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->workspace = workspace;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Global workspace FEP bridge connected to workspace");
    return 0;
}

/* ============================================================================
 * FEP → Workspace Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Have FEP beliefs compete for workspace access
 * WHY:  High-evidence beliefs should reach consciousness (workspace)
 * HOW:  Compute evidence from free energy, compete with strength
 *
 * BIOLOGICAL BASIS:
 * - FEP beliefs = distributed cortical representations
 * - Model evidence = population firing strength
 * - Workspace competition = prefrontal-parietal ignition
 * - Winner = conscious percept/hypothesis
 *
 * ALGORITHM:
 * 1. Get current FEP free energy F
 * 2. Compute evidence: evidence = exp(-F)
 * 3. Get belief precision (gain control)
 * 4. Compute competition strength: strength = f(precision, evidence)
 * 5. Submit to workspace competition
 * 6. If win → beliefs become conscious (globally broadcast)
 */
int global_workspace_fep_compete_with_beliefs(global_workspace_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system || !bridge->workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_compete_with_beliefs: required parameter is NULL (bridge, bridge->fep_system, bridge->workspace)");
        return -1;
    }
    if (!bridge->config.enable_evidence_competition) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_global_workspace_fep", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Get current free energy and compute evidence */
    float free_energy = fep->free_energy.total;
    float evidence = compute_model_evidence(free_energy);
    evidence *= bridge->config.model_evidence_sensitivity;

    /* Get average belief precision across hierarchy */
    float avg_precision = 0.0f;
    uint32_t precision_count = 0;
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            global_workspace_fep_bridge_heartbeat("global_works_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->beliefs.dim > 256) {
                global_workspace_fep_bridge_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)level->beliefs.dim);
            }

            avg_precision += level->beliefs.precision[i];
            precision_count++;
        }
    }
    if (precision_count > 0) {
        avg_precision /= (float)precision_count;
    }

    /* Compute competition strength */
    float strength = compute_competition_strength(
        avg_precision,
        evidence,
        0.4f,  /* precision weight */
        0.6f   /* evidence weight */
    );
    strength *= bridge->config.competition_strength_factor;

    /* Store current evidence */
    bridge->state.current_belief_evidence = evidence;
    bridge->effects.model_evidence = evidence;
    bridge->effects.competition_strength = strength;

    /* Check if evidence exceeds threshold */
    if (evidence < bridge->config.belief_evidence_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;  /* Below threshold, don't compete */
    }

    /* Prepare belief content for broadcast (use top-level beliefs) */
    float belief_content[256];
    uint32_t content_dim = 0;

    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* top_level = &fep->levels[fep->num_levels - 1];
        content_dim = top_level->beliefs.dim < 256 ? top_level->beliefs.dim : 256;
        memcpy(belief_content, top_level->beliefs.mean, content_dim * sizeof(float));
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Compete for workspace access */
    bool won = global_workspace_compete(
        bridge->workspace,
        MODULE_PREDICTIVE,
        belief_content,
        content_dim,
        strength
    );

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->state.belief_in_workspace = won;
    if (won) {
        bridge->stats.total_competitions_won++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return won ? 1 : 0;
}

/**
 * WHAT: Broadcast winning FEP belief to workspace
 * WHY:  Make high-evidence hypothesis globally available (conscious)
 * HOW:  Extract belief, submit to workspace with evidence as strength
 *
 * THEORY: Winning belief = hypothesis with highest model evidence
 *         Broadcasting = making it globally available to all modules
 *         This is the FEP interpretation of "conscious access"
 */
int global_workspace_fep_broadcast_winning_belief(global_workspace_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system || !bridge->workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_broadcast_winning_belief: required parameter is NULL (bridge, bridge->fep_system, bridge->workspace)");
        return -1;
    }
    if (!bridge->config.enable_belief_broadcasting) return 0;

    /* Compete with beliefs (includes broadcast if win) */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_global_workspace_fep", 0.0f);


    int result = global_workspace_fep_compete_with_beliefs(bridge);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (result > 0) {
        bridge->state.broadcasts_from_beliefs++;
        bridge->stats.total_belief_broadcasts++;

        /* Update running average of broadcast evidence */
        float alpha = 0.1f;
        bridge->stats.avg_broadcast_evidence =
            alpha * bridge->state.current_belief_evidence +
            (1.0f - alpha) * bridge->stats.avg_broadcast_evidence;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ============================================================================
 * Workspace → FEP Direction Implementation
 * ============================================================================ */

/**
 * WHAT: Update FEP priors from workspace broadcast
 * WHY:  Broadcast content = shared belief that should inform predictions
 * HOW:  Read workspace, inject as prior into FEP hierarchy
 *
 * BIOLOGICAL BASIS:
 * - Workspace broadcast = prefrontal-parietal → distributed cortex
 * - Prior update = top-down prediction from "conscious hypothesis"
 * - All sensory/cognitive areas receive same prior (global coherence)
 *
 * THEORY (Hohwy 2012):
 * - Attention (workspace access) = precision-weighting
 * - Conscious content = high-precision prior for perceptual inference
 * - Broadcast enables hypothesis testing across entire hierarchy
 *
 * ALGORITHM:
 * 1. Read current workspace broadcast
 * 2. Check if broadcast is from FEP (avoid circular update)
 * 3. Extract broadcast content and strength
 * 4. Inject as prior into FEP top-level beliefs
 * 5. Scale by broadcast_prior_weight (prevent override)
 */
int global_workspace_fep_update_priors_from_broadcast(
    global_workspace_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_system || !bridge->workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_update_priors_from_broadcast: required parameter is NULL (bridge, bridge->fep_system, bridge->workspace)");
        return -1;
    }
    if (!bridge->config.enable_prior_updates) return 0;

    /* Check if workspace has broadcast */
    if (!global_workspace_has_broadcast(bridge->workspace)) {
        return 0;
    }

    /* Read broadcast */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_global_workspace_fep", 0.0f);


    float broadcast_content[256];
    uint32_t broadcast_dim = 0;
    cognitive_module_t source;

    bool has_broadcast = global_workspace_read_broadcast(
        bridge->workspace,
        broadcast_content,
        256,
        &broadcast_dim,
        &source
    );

    if (!has_broadcast) {
        return 0;
    }

    /* Avoid circular update (don't update from own broadcasts) */
    if (source == MODULE_PREDICTIVE) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    fep_system_t* fep = bridge->fep_system;

    /* Get broadcast strength (= confidence in this prior) */
    float broadcast_strength = global_workspace_get_broadcast_strength(bridge->workspace);
    float prior_weight = bridge->config.broadcast_prior_weight * broadcast_strength;

    /* Update top-level beliefs with broadcast content as prior */
    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* top_level = &fep->levels[fep->num_levels - 1];
        uint32_t update_dim = broadcast_dim < top_level->beliefs.dim ?
                              broadcast_dim : top_level->beliefs.dim;

        /* Blend broadcast into current beliefs (weighted prior update) */
        for (uint32_t i = 0; i < update_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && update_dim > 256) {
                global_workspace_fep_bridge_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)update_dim);
            }

            float current = top_level->beliefs.mean[i];
            float prior = broadcast_content[i];
            top_level->beliefs.mean[i] = (1.0f - prior_weight) * current +
                                         prior_weight * prior;
        }

        /* Boost precision for broadcast-aligned beliefs (attention effect) */
        float precision_boost = 1.0f + bridge->config.broadcast_prior_weight;
        for (uint32_t i = 0; i < update_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && update_dim > 256) {
                global_workspace_fep_bridge_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)update_dim);
            }

            top_level->beliefs.precision[i] *= precision_boost;
            /* Clamp to prevent runaway precision */
            top_level->beliefs.precision[i] = nimcp_clampf(
                top_level->beliefs.precision[i],
                0.01f,
                100.0f
            );
        }
    }

    /* Update state and statistics */
    bridge->state.prior_updates_from_broadcast++;
    bridge->stats.total_prior_updates++;
    bridge->effects.broadcast_prior_boost = prior_weight;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

/**
 * WHAT: Main update cycle for bridge
 * WHY:  Maintain bidirectional flow between FEP and workspace
 * HOW:  Execute both directions: compete → broadcast, read → update priors
 *
 * OPERATION SEQUENCE:
 * 1. FEP beliefs compete for workspace (FEP → workspace)
 * 2. If win, broadcast belief to all modules
 * 3. Read any workspace broadcasts from other modules
 * 4. Update FEP priors from broadcast (workspace → FEP)
 *
 * This creates a feedback loop:
 * - FEP hypotheses compete for global access (bottom-up)
 * - Workspace broadcasts guide FEP predictions (top-down)
 * - Result: coordinated inference across cognitive hierarchy
 */
int global_workspace_fep_bridge_update(
    global_workspace_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* FEP → Workspace: Compete with current beliefs */
    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_update", 0.0f);


    global_workspace_fep_compete_with_beliefs(bridge);

    /* Workspace → FEP: Update priors from broadcast */
    global_workspace_fep_update_priors_from_broadcast(bridge);

    return 0;
}

/* ============================================================================
 * State/Stats Implementation
 * ============================================================================ */

/**
 * WHAT: Get current bridge state
 * WHY:  Monitor bridge operation
 * HOW:  Copy state structure
 */
int global_workspace_fep_bridge_get_state(
    const global_workspace_fep_bridge_t* bridge,
    global_workspace_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_get_state", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get accumulated statistics
 * WHY:  Track performance and usage
 * HOW:  Copy stats structure
 */
int global_workspace_fep_bridge_get_stats(
    const global_workspace_fep_bridge_t* bridge,
    global_workspace_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "global_workspace_fep_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_get_stats", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect bridge to bio-async messaging
 * WHY:  Enable inter-module communication
 * HOW:  Register with bio_router, get module context
 */
int global_workspace_fep_bridge_connect_bio_async(
    global_workspace_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_GLOBAL_WORKSPACE_BRIDGE,
        .module_name = "global_workspace_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Global workspace FEP bridge connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister from bio_router
 */
int global_workspace_fep_bridge_disconnect_bio_async(
    global_workspace_fep_bridge_t* bridge
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Global workspace FEP bridge disconnected from bio-async");

    return 0;
}

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection state
 * HOW:  Return enabled flag
 */
bool global_workspace_fep_bridge_is_bio_async_connected(
    const global_workspace_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_is_bio_async_connect", 0.0f);


    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int global_workspace_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_fep_bridge_heartbeat("global_works_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Global_Workspace_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                global_workspace_fep_bridge_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Global_Workspace_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Global_Workspace_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void global_workspace_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_global_workspace_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int global_workspace_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    global_workspace_fep_bridge_heartbeat_instance(g_global_workspace_fep_bridge_health_agent, "global_workspace_fep_bridge_training_begin", 0.0f);
    return 0;
}

int global_workspace_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_fep_bridge_training_end: NULL argument");
        return -1;
    }
    global_workspace_fep_bridge_heartbeat_instance(g_global_workspace_fep_bridge_health_agent, "global_workspace_fep_bridge_training_end", 1.0f);
    return 0;
}

int global_workspace_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "global_workspace_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    global_workspace_fep_bridge_heartbeat_instance(g_global_workspace_fep_bridge_health_agent, "global_workspace_fep_bridge_training_step", progress);
    return 0;
}

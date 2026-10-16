//=============================================================================
// nimcp_brain_init_world_model.c - World Model Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_world_model.c
 * @brief Implementation of World Model subsystem initialization
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Brain factory initialization function for world models
 * WHY:  Integrate generative world models into brain lifecycle
 * HOW:  Creates and configures omni and multimodal world models
 *
 * THEORETICAL BASIS:
 * - DreamerV3: RSSM world model with symlog rewards (Hafner et al.)
 * - JEPA: Predict in latent space, not pixel space (LeCun)
 * - Active Inference: Generative model for EFE minimization (Friston)
 *
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_world_model.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "cognitive/omni/nimcp_omni_active_inference.h"
#include "utils/logging/nimcp_logging.h"

/* World Model Integration Bridge Headers */
#include "cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thousand_brains_bridge.h"
#include "core/cortical_columns/nimcp_thousand_brains_integration.h"
#include "core/cortical_columns/nimcp_column_reference_frame.h"
#include "core/cortical_columns/nimcp_column_voting.h"
#include "core/cortical_columns/nimcp_dendritic_sequence.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Forward declarations to avoid conflicting type definitions in imagination header */
struct imagination_engine;
int imagination_engine_set_world_model(struct imagination_engine* engine,
                                       struct omni_world_model* wm);

/* Accessor stubs — simulation engines are standalone singletons, not on brain_struct.
 * These return NULL; the parietal lobe and world simulator manage engines directly. */
struct intuitive_physics_engine* nimcp_brain_get_intuitive_physics(struct brain_struct* b) { return b ? b->intuitive_physics : NULL; }
struct entity_tracker*           nimcp_brain_get_entity_tracker(struct brain_struct* b) { return b ? b->entity_tracker : NULL; }
struct scene_graph*              nimcp_brain_get_scene_graph(struct brain_struct* b) { return b ? b->scene_graph : NULL; }
struct physics_prior*            nimcp_brain_get_physics_prior(struct brain_struct* b) { return b ? b->physics_prior : NULL; }

#define LOG_MODULE "BRAIN_INIT_WORLD_MODEL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_world_model, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize Omni World Model
 */
static bool init_omni_world_model(struct brain_struct* brain) {
    if (!brain->config.enable_omni_world_model) {
        LOG_DEBUG(LOG_MODULE, "Omni world model disabled in config");
        return true;
    }

    LOG_DEBUG(LOG_MODULE, "Creating Omni World Model...");

    /* Build configuration from brain config */
    omni_wm_config_t config;
    if (omni_wm_get_default_config(&config) != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to get default omni WM config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_omni_world_model: validation failed");
        return false;
    }

    /* Override with brain config values */
    if (brain->config.omni_wm_state_dim > 0) {
        config.state_dim = brain->config.omni_wm_state_dim;
    }
    if (brain->config.omni_wm_action_dim > 0) {
        config.action_dim = brain->config.omni_wm_action_dim;
    }
    if (brain->config.omni_wm_obs_dim > 0) {
        config.obs_dim = brain->config.omni_wm_obs_dim;
    }
    if (brain->config.omni_wm_latent_dim > 0) {
        config.latent_dim = brain->config.omni_wm_latent_dim;
    }
    if (brain->config.omni_wm_rssm_h_dim > 0) {
        config.rssm_h_dim = brain->config.omni_wm_rssm_h_dim;
    }
    if (brain->config.omni_wm_rssm_z_dim > 0) {
        config.rssm_z_dim = brain->config.omni_wm_rssm_z_dim;
    }
    if (brain->config.omni_wm_learning_rate > 0.0f) {
        config.learning_rate = brain->config.omni_wm_learning_rate;
    }
    if (brain->config.omni_wm_dream_horizon > 0) {
        config.dream_horizon = brain->config.omni_wm_dream_horizon;
    }
    config.enable_dreaming = brain->config.omni_wm_enable_dreaming;
    config.use_rssm = true;  /* Always use RSSM for DreamerV3-style dynamics */
    config.use_symlog_rewards = true;  /* DreamerV3 reward normalization */

    /* Create omni world model */
    brain->omni_world_model = omni_wm_create(&config);
    if (!brain->omni_world_model) {
        LOG_ERROR(LOG_MODULE, "Failed to create omni world model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_omni_world_model: brain->omni_world_model is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Omni World Model created:");
    LOG_DEBUG(LOG_MODULE, "  - State dim: %u", config.state_dim);
    LOG_DEBUG(LOG_MODULE, "  - Action dim: %u", config.action_dim);
    LOG_DEBUG(LOG_MODULE, "  - RSSM h_dim: %u, z_dim: %u", config.rssm_h_dim, config.rssm_z_dim);
    LOG_DEBUG(LOG_MODULE, "  - Dreaming: %s", config.enable_dreaming ? "enabled" : "disabled");

    return true;
}

/**
 * @brief Initialize Multimodal World Model
 */
static bool init_multimodal_world_model(struct brain_struct* brain) {
    if (!brain->config.enable_multimodal_world_model) {
        LOG_DEBUG(LOG_MODULE, "Multimodal world model disabled in config");
        return true;
    }

    LOG_DEBUG(LOG_MODULE, "Creating Multimodal World Model...");

    /* Get default configuration */
    wm_config_t config = wm_default_config();

    /* Override with brain config values */
    if (brain->config.mm_wm_latent_dim > 0) {
        config.latent_dim = brain->config.mm_wm_latent_dim;
    }
    if (brain->config.mm_wm_max_entities > 0) {
        config.max_entities = brain->config.mm_wm_max_entities;
    }
    if (brain->config.mm_wm_max_prediction_steps > 0) {
        config.max_prediction_steps = brain->config.mm_wm_max_prediction_steps;
    }
    if (brain->config.mm_wm_learning_rate > 0.0f) {
        config.learning_rate = brain->config.mm_wm_learning_rate;
    }
    config.enable_bio_async = brain->config.mm_wm_enable_bio_async;

    /* Create multimodal world model */
    brain->multimodal_world_model = wm_create(&config);
    if (!brain->multimodal_world_model) {
        LOG_ERROR(LOG_MODULE, "Failed to create multimodal world model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_world_model: brain->multimodal_world_model is NULL");
        return false;
    }

    /* Initialize the world model */
    if (wm_init(brain->multimodal_world_model) != WM_OK) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize multimodal world model");
        wm_destroy(brain->multimodal_world_model);
        brain->multimodal_world_model = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "init_multimodal_world_model: validation failed");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Multimodal World Model created:");
    LOG_DEBUG(LOG_MODULE, "  - Latent dim: %u", config.latent_dim);
    LOG_DEBUG(LOG_MODULE, "  - Max entities: %u", config.max_entities);
    LOG_DEBUG(LOG_MODULE, "  - Max prediction steps: %u", config.max_prediction_steps);
    LOG_DEBUG(LOG_MODULE, "  - Bio-async: %s", config.enable_bio_async ? "enabled" : "disabled");

    return true;
}

//=============================================================================
// Subsystem Initialization
//=============================================================================

bool nimcp_brain_factory_init_world_model_subsystem(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_world_model_subsystem: brain is NULL");
        return false;
    }

    /* Do NOT NULL existing pointers — checkpoint may have loaded them.
     * Only set defaults for fields that aren't already populated. */
    brain->world_model_lazy_init = false;
    brain->last_world_model_update_us = 0;
    brain->world_model_update_interval_us = 10000;  /* Default: 10ms */

    /* === Intuitive Physics Subsystem ===
     * Simulation engines are created as standalone singletons, NOT stored on
     * brain_struct (which would break checkpoint compatibility). They are
     * accessed via the world simulator and parietal lobe integration layer.
     * The engines are created here during init but NOT attached to the brain. */
    if (brain->config.enable_intuitive_physics) {
        LOG_INFO(LOG_MODULE, "Initializing Intuitive Physics subsystem...");

        extern struct intuitive_physics_engine* intuitive_physics_create(const void* config);
        extern struct entity_tracker* entity_tracker_create(const void* config);
        extern struct scene_graph* scene_graph_create(const void* config);
        extern struct physics_prior* physics_prior_create(const void* config);
        extern void physics_prior_connect(struct physics_prior* prior,
                                           struct intuitive_physics_engine* physics,
                                           struct entity_tracker* tracker,
                                           struct scene_graph* scene);
        extern uint32_t intuitive_physics_add_ground(struct intuitive_physics_engine* engine);

        brain->intuitive_physics = intuitive_physics_create(NULL);
        if (brain->intuitive_physics) {
            intuitive_physics_add_ground(brain->intuitive_physics);
            LOG_INFO(LOG_MODULE, "  - Intuitive physics engine: created (with ground plane)");
        }

        brain->entity_tracker = entity_tracker_create(NULL);
        if (brain->entity_tracker)
            LOG_INFO(LOG_MODULE, "  - Entity tracker: created");

        brain->scene_graph = scene_graph_create(NULL);
        if (brain->scene_graph)
            LOG_INFO(LOG_MODULE, "  - Scene graph: created");

        if (brain->config.enable_physics_prior) {
            brain->physics_prior = physics_prior_create(NULL);
            if (brain->physics_prior) {
                physics_prior_connect(NULL /* {field} removed from brain_struct */,
                                       NULL /* {field} removed from brain_struct */,
                                       NULL /* {field} removed from brain_struct */,
                                       brain->scene_graph);
                LOG_INFO(LOG_MODULE, "  - Physics prior: created + connected");
            }
        }

        /* Chemistry simulator */
        extern struct chemistry_sim* chemistry_sim_create(const void* config);
        extern void chemistry_sim_load_common_elements(struct chemistry_sim* sim);
        extern void chemistry_sim_load_common_substances(struct chemistry_sim* sim);
        extern void chemistry_sim_load_common_reactions(struct chemistry_sim* sim);

        brain->chemistry_sim = chemistry_sim_create(NULL);
        if (brain->chemistry_sim) {
            chemistry_sim_load_common_elements(brain->chemistry_sim);
            chemistry_sim_load_common_substances(brain->chemistry_sim);
            chemistry_sim_load_common_reactions(brain->chemistry_sim);
        }

        /* Biology simulator */
        extern struct biology_sim* biology_sim_create(const void* config);
        extern void biology_sim_load_grassland(struct biology_sim* sim);
        extern void biology_sim_load_human_body(struct biology_sim* sim);

        brain->biology_sim = biology_sim_create(NULL);
        if (brain->biology_sim) {
            biology_sim_load_grassland(brain->biology_sim);
            biology_sim_load_human_body(brain->biology_sim);
        }

        /* Unified world prior */
        extern struct world_prior* world_prior_create(const void* config);
        extern void world_prior_connect(struct world_prior* wp,
                                         struct physics_prior* physics,
                                         struct chemistry_sim* chemistry,
                                         struct biology_sim* biology);

        brain->world_prior = world_prior_create(NULL);
        if (brain->world_prior) {
            world_prior_connect(NULL /* {field} removed from brain_struct */,
                                 NULL /* {field} removed from brain_struct */,
                                 NULL /* {field} removed from brain_struct */,
                                 brain->biology_sim);
        }

        brain->intuitive_physics_enabled = (void*)(uintptr_t)(brain->intuitive_physics != NULL);
        LOG_INFO(LOG_MODULE, "World Model Prior: physics=%s chemistry=%s biology=%s unified=%s",
                 NULL /* {field} removed from brain_struct */ ? "yes" : "no",
                 NULL /* {field} removed from brain_struct */ ? "yes" : "no",
                 NULL /* {field} removed from brain_struct */ ? "yes" : "no",
                 NULL /* {field} removed from brain_struct */ ? "yes" : "no");
    }

    /* Check if world model is enabled in config */
    if (!brain->config.enable_world_model) {
        LOG_DEBUG(LOG_MODULE, "World model disabled in config - skipping initialization");
        return true;  /* Not an error - just disabled */
    }

    /* Check for lazy initialization */
    if (brain->config.lazy_world_model_init) {
        LOG_DEBUG(LOG_MODULE, "World model set to lazy initialization - deferring");
        brain->world_model_lazy_init = true;
        return true;
    }

    LOG_INFO(LOG_MODULE, "Initializing World Model subsystem...");

    /* Initialize Omni World Model */
    if (!init_omni_world_model(brain)) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize Omni World Model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_brain_factory_init_world_model_subsystem: init_omni_world_model is NULL");
        return false;
    }

    /* Initialize Multimodal World Model */
    if (!init_multimodal_world_model(brain)) {
        LOG_ERROR(LOG_MODULE, "Failed to initialize Multimodal World Model");
        /* Cleanup omni world model on failure */
        if (brain->omni_world_model) {
            omni_wm_destroy(brain->omni_world_model);
            brain->omni_world_model = NULL;
        }
        return false;
    }

    /* Only mark as enabled if a world model actually exists */
    if (brain->omni_world_model || brain->multimodal_world_model) {
        brain->world_model_enabled = true;
    }

    /* === Thousand Brains Integration === */
    if (brain->config.enable_thousand_brains_integration) {
        LOG_INFO(LOG_MODULE, "Initializing Thousand Brains integration...");

        /* Create TB core components — skip if already allocated (checkpoint load) */
        if (!brain->tb_ref_frames) {
            column_ref_frame_config_t rf_config;
            column_ref_frame_config_default(&rf_config);
            brain->tb_ref_frames = column_ref_frame_create(&rf_config);
        }

        if (!brain->tb_voting) {
            column_voting_config_t vt_config;
            column_voting_config_default(&vt_config);
            brain->tb_voting = column_voting_create(&vt_config);
        }

        if (!brain->tb_sequences) {
            dendritic_seq_config_t ds_config;
            dendritic_seq_config_default(&ds_config);
            brain->tb_sequences = dendritic_seq_create(&ds_config);
        }

        /* Create integration hub — skip if already exists */
        if (!brain->tb_integration_hub) {
            tb_integration_config_t tb_config;
            tb_integration_config_default(&tb_config);
            brain->tb_integration_hub = tb_integration_create(&tb_config);
        }

        if (brain->tb_integration_hub) {
            /* Skip TB bridge connections on loaded brains — the connect
             * functions assume fresh state and can corrupt loaded brain data.
             * The TB components themselves are safe. */
            if (brain->stats.total_learning_steps == 0) {
                /* Fresh brain — safe to wire everything */
                if (brain->tb_ref_frames && brain->tb_voting && brain->tb_sequences) {
                    tb_integration_connect_tb(brain->tb_integration_hub,
                                               brain->tb_ref_frames,
                                               brain->tb_voting,
                                               brain->tb_sequences);
                }
                int wired = tb_integration_wire_from_brain(brain->tb_integration_hub, brain);
                LOG_INFO(LOG_MODULE, "  - Thousand Brains: %d systems wired", wired);

                if (brain->wm_thousand_brains_bridge &&
                    brain->tb_ref_frames && brain->tb_voting && brain->tb_sequences) {
                    wm_tb_bridge_connect_ref_frames(brain->wm_thousand_brains_bridge,
                                                     brain->tb_ref_frames);
                    wm_tb_bridge_connect_voting(brain->wm_thousand_brains_bridge,
                                                 brain->tb_voting);
                    wm_tb_bridge_connect_sequences(brain->wm_thousand_brains_bridge,
                                                    brain->tb_sequences);
                }
                if (brain->tb_voting && brain->global_workspace) {
                    column_voting_connect_workspace(brain->tb_voting,
                                                     brain->global_workspace);
                }
            } else {
                /* Loaded brain: the TB components were freshly created but NOT wired.
                 * The wm_thousand_brains_bridge from checkpoint has stale internal
                 * pointers (ref_frames, voting, sequences) that point to freed memory.
                 * NULL it out so brain_learn_vector's wm_tb_bridge_step skips it. */
                brain->wm_thousand_brains_bridge = NULL;
                brain->config.enable_wm_thousand_brains_bridge = false;
                brain->config.enable_thousand_brains_integration = false;
                LOG_INFO(LOG_MODULE, "  - Thousand Brains: disabled on loaded brain (stale bridge pointers)");
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Failed to create TB integration hub");
        }
    }

    LOG_INFO(LOG_MODULE, "World Model subsystem initialized successfully");
    LOG_DEBUG(LOG_MODULE, "  - Omni World Model: %s",
              brain->omni_world_model ? "enabled" : "disabled");
    LOG_DEBUG(LOG_MODULE, "  - Multimodal World Model: %s",
              brain->multimodal_world_model ? "enabled" : "disabled");
    LOG_DEBUG(LOG_MODULE, "  - Thousand Brains: %s",
              brain->tb_integration_hub ? "enabled" : "disabled");
    LOG_DEBUG(LOG_MODULE, "  - Intuitive Physics: %s",
              NULL /* {field} removed from brain_struct */ ? "enabled" : "disabled");

    return true;
}

void nimcp_brain_factory_destroy_world_model_subsystem(struct brain_struct* brain) {
    if (!brain) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying World Model subsystem...");

    /* Destroy omni world model */
    if (brain->omni_world_model) {
        omni_wm_destroy(brain->omni_world_model);
        brain->omni_world_model = NULL;
    }

    /* Destroy multimodal world model */
    if (brain->multimodal_world_model) {
        wm_destroy(brain->multimodal_world_model);
        brain->multimodal_world_model = NULL;
    }

    /* Destroy Thousand Brains integration */
    if (brain->tb_integration_hub) {
        tb_integration_destroy(brain->tb_integration_hub);
        brain->tb_integration_hub = NULL;
    }
    if (brain->tb_sequences) {
        dendritic_seq_destroy(brain->tb_sequences);
        brain->tb_sequences = NULL;
    }
    if (brain->tb_voting) {
        column_voting_destroy(brain->tb_voting);
        brain->tb_voting = NULL;
    }
    if (brain->tb_ref_frames) {
        column_ref_frame_destroy(brain->tb_ref_frames);
        brain->tb_ref_frames = NULL;
    }

    /* Destroy World Prior subsystem (unified + chemistry + biology + physics) */
    if (brain->world_prior) {
        extern void world_prior_destroy(struct world_prior* wp);
        world_prior_destroy(brain->world_prior);
        brain->world_prior = NULL;
    }
    if (brain->biology_sim) {
        extern void biology_sim_destroy(struct biology_sim* sim);
        biology_sim_destroy(brain->biology_sim);
        brain->biology_sim = NULL;
    }
    if (brain->chemistry_sim) {
        extern void chemistry_sim_destroy(struct chemistry_sim* sim);
        chemistry_sim_destroy(brain->chemistry_sim);
        brain->chemistry_sim = NULL;
    }
    if (brain->physics_prior) {
        extern void physics_prior_destroy(struct physics_prior* prior);
        physics_prior_destroy(brain->physics_prior);
        brain->physics_prior = NULL;
    }
    if (brain->scene_graph) {
        extern void scene_graph_destroy(struct scene_graph* graph);
        scene_graph_destroy(brain->scene_graph);
        brain->scene_graph = NULL;
    }
    if (brain->entity_tracker) {
        extern void entity_tracker_destroy(struct entity_tracker* tracker);
        entity_tracker_destroy(brain->entity_tracker);
        brain->entity_tracker = NULL;
    }
    if (brain->intuitive_physics) {
        extern void intuitive_physics_destroy(struct intuitive_physics_engine* engine);
        intuitive_physics_destroy(brain->intuitive_physics);
        brain->intuitive_physics = NULL;
    }
    brain->intuitive_physics_enabled = false;

    brain->world_model_enabled = false;
    brain->world_model_lazy_init = false;

    LOG_DEBUG(LOG_MODULE, "World Model subsystem destroyed");
}

bool nimcp_brain_factory_wire_world_model_active_inference(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_wire_world_model_active_inference: brain is NULL");
        return false;
    }

    if (!brain->config.world_model_connect_active_inference) {
        LOG_DEBUG(LOG_MODULE, "World model -> active inference wiring disabled");
        return true;
    }

    if (!brain->world_model_enabled) {
        LOG_DEBUG(LOG_MODULE, "World model not enabled - skipping active inference wiring");
        return true;
    }

    /* Check if omni world model is available */
    if (!brain->omni_world_model) {
        LOG_DEBUG(LOG_MODULE, "Omni world model not initialized - skipping active inference wiring");
        return true;
    }

    /* TODO: Check if active inference system is available and wire them together
     * This requires accessing the active inference module from the brain
     * For now, we log that the connection would be made
     *
     * When active inference is initialized, it should call:
     *   omni_wm_connect_active_inference(brain->omni_world_model, brain->active_inference);
     */
    LOG_INFO(LOG_MODULE, "World model ready for active inference connection");
    LOG_DEBUG(LOG_MODULE, "  - Active inference will use world model for policy evaluation");
    LOG_DEBUG(LOG_MODULE, "  - EFE computation via simulated rollouts");

    return true;
}

bool nimcp_brain_factory_wire_world_model_imagination(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_wire_world_model_imagination: brain is NULL");
        return false;
    }

    if (!brain->config.world_model_connect_imagination) {
        LOG_DEBUG(LOG_MODULE, "World model -> imagination wiring disabled");
        return true;
    }

    if (!brain->world_model_enabled) {
        LOG_DEBUG(LOG_MODULE, "World model not enabled - skipping imagination wiring");
        return true;
    }

    /* TODO: Check if imagination engine is available and wire them together
     * This requires accessing the imagination engine from the brain
     * For now, we log that the connection would be made
     *
     * When imagination engine is initialized, it should call:
     *   imagination_engine_set_world_model(brain->imagination_engine, brain->omni_world_model);
     */
    LOG_INFO(LOG_MODULE, "World model ready for imagination engine connection");
    LOG_DEBUG(LOG_MODULE, "  - Imagination will use world model for scene dynamics");
    LOG_DEBUG(LOG_MODULE, "  - Counterfactual reasoning via world model simulation");

    return true;
}

//=============================================================================
// World Model Bridge Wiring
//=============================================================================

bool nimcp_brain_factory_wire_world_model_bridges(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_wire_world_model_bridges: brain is NULL");
        return false;
    }

    if (!brain->world_model_enabled) {
        LOG_DEBUG(LOG_MODULE, "World model not enabled - skipping bridge wiring");
        return true;
    }

    if (!brain->omni_world_model) {
        LOG_DEBUG(LOG_MODULE, "Omni world model not available - skipping bridge wiring");
        return true;
    }

    LOG_INFO(LOG_MODULE, "Wiring World Model integration bridges...");

    uint32_t bridges_created = 0;
    uint32_t bridges_failed = 0;

    /* 1. Security-Immune Bridge (0x0E63) */
    if (brain->config.enable_wm_security_immune_bridge) {
        omni_wm_security_immune_config_t sec_config;
        omni_wm_security_immune_bridge_default_config(&sec_config);
        brain->wm_security_immune_bridge = omni_wm_security_immune_bridge_create(&sec_config);
        if (brain->wm_security_immune_bridge) {
            if (omni_wm_security_immune_bridge_connect_world_model(brain->wm_security_immune_bridge,
                                                          brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Security-Immune Bridge (0x0E63) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Security-Immune Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Security-Immune Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 2. Logging Bridge (0x0E64) */
    if (brain->config.enable_wm_logging_bridge) {
        omni_wm_logging_bridge_config_t log_config;
        omni_wm_logging_bridge_default_config(&log_config);
        brain->wm_logging_bridge = omni_wm_logging_bridge_create(&log_config);
        if (brain->wm_logging_bridge) {
            if (omni_wm_logging_bridge_connect_world_model(brain->wm_logging_bridge,
                                                   brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Logging Bridge (0x0E64) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Logging Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Logging Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 3. Cognitive Bridge (0x0E65) */
    if (brain->config.enable_wm_cognitive_bridge) {
        omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
        brain->wm_cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
        if (brain->wm_cognitive_bridge) {
            if (omni_wm_cognitive_bridge_connect_world_model(brain->wm_cognitive_bridge,
                                                    brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Cognitive Bridge (0x0E65) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Cognitive Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Cognitive Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 4. Parietal Bridge (0x0E66) */
    if (brain->config.enable_wm_parietal_bridge) {
        omni_wm_parietal_bridge_config_t par_config;
        omni_wm_parietal_bridge_default_config(&par_config);
        brain->wm_parietal_bridge = omni_wm_parietal_bridge_create(&par_config);
        if (brain->wm_parietal_bridge) {
            if (omni_wm_parietal_bridge_connect_world_model(brain->wm_parietal_bridge,
                                                   brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Parietal Bridge (0x0E66) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Parietal Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Parietal Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 5. Hypothalamus Bridge (0x0E67) */
    if (brain->config.enable_wm_hypothalamus_bridge) {
        omni_wm_hypothalamus_bridge_config_t hypo_config;
        omni_wm_hypothalamus_bridge_default_config(&hypo_config);
        brain->wm_hypothalamus_bridge = omni_wm_hypothalamus_bridge_create(&hypo_config);
        if (brain->wm_hypothalamus_bridge) {
            if (omni_wm_hypothalamus_bridge_connect_world_model(brain->wm_hypothalamus_bridge,
                                                       brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Hypothalamus Bridge (0x0E67) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Hypothalamus Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Hypothalamus Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 6. Thalamic Bridge (0x0E68) */
    if (brain->config.enable_wm_thalamic_bridge) {
        omni_wm_thalamic_bridge_config_t thal_config;
        omni_wm_thalamic_bridge_default_config(&thal_config);
        brain->wm_thalamic_bridge = omni_wm_thalamic_bridge_create(&thal_config);
        if (brain->wm_thalamic_bridge) {
            if (omni_wm_thalamic_bridge_connect_world_model(brain->wm_thalamic_bridge,
                                                   brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Thalamic Bridge (0x0E68) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Thalamic Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Thalamic Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 7. Substrate Bridge (0x0E69) */
    if (brain->config.enable_wm_substrate_bridge) {
        omni_wm_substrate_bridge_config_t sub_config;
        omni_wm_substrate_bridge_default_config(&sub_config);
        brain->wm_substrate_bridge = omni_wm_substrate_bridge_create(&sub_config);
        if (brain->wm_substrate_bridge) {
            if (omni_wm_substrate_bridge_connect_world_model(brain->wm_substrate_bridge,
                                                    brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Substrate Bridge (0x0E69) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Substrate Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Substrate Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 8. Memory Bridge (0x0E6A) */
    if (brain->config.enable_wm_memory_bridge) {
        omni_wm_memory_bridge_config_t mem_config;
        omni_wm_memory_bridge_default_config(&mem_config);
        brain->wm_memory_bridge = omni_wm_memory_bridge_create(&mem_config);
        if (brain->wm_memory_bridge) {
            if (omni_wm_memory_bridge_connect_world_model(brain->wm_memory_bridge,
                                                  brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Memory Bridge (0x0E6A) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Memory Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Memory Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 9. KG Bridge (0x0E6B) */
    if (brain->config.enable_wm_kg_bridge) {
        omni_wm_kg_bridge_config_t kg_config;
        omni_wm_kg_bridge_default_config(&kg_config);
        brain->wm_kg_bridge = omni_wm_kg_bridge_create(&kg_config);
        if (brain->wm_kg_bridge) {
            if (omni_wm_kg_bridge_connect_world_model(brain->wm_kg_bridge,
                                             brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - KG Bridge (0x0E6B) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - KG Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - KG Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 10. Theory of Mind Bridge (0x0E6C) */
    if (brain->config.enable_wm_tom_bridge) {
        omni_wm_tom_bridge_config_t tom_config;
        omni_wm_tom_bridge_default_config(&tom_config);
        brain->wm_tom_bridge = omni_wm_tom_bridge_create(&tom_config);
        if (brain->wm_tom_bridge) {
            if (omni_wm_tom_bridge_connect_world_model(brain->wm_tom_bridge,
                                              brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - ToM Bridge (0x0E6C) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - ToM Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - ToM Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 11. Plasticity Bridge (0x0E6D) */
    if (brain->config.enable_wm_plasticity_bridge) {
        omni_wm_plasticity_bridge_config_t plast_config;
        omni_wm_plasticity_bridge_default_config(&plast_config);
        brain->wm_plasticity_bridge = omni_wm_plasticity_bridge_create(&plast_config);
        if (brain->wm_plasticity_bridge) {
            if (omni_wm_plasticity_bridge_connect_world_model(brain->wm_plasticity_bridge,
                                                     brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Plasticity Bridge (0x0E6D) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Plasticity Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Plasticity Bridge creation failed");
            bridges_failed++;
        }
    }

    /* 12. Thousand Brains Bridge (0x0E6E) - Hawkins cortical column integration */
    if (brain->config.enable_wm_thousand_brains_bridge) {
        wm_tb_bridge_config_t tb_config;
        wm_tb_bridge_config_default(&tb_config);
        brain->wm_thousand_brains_bridge = wm_tb_bridge_create(&tb_config);
        if (brain->wm_thousand_brains_bridge) {
            if (wm_tb_bridge_connect_world_model(brain->wm_thousand_brains_bridge,
                                                  brain->omni_world_model) == NIMCP_SUCCESS) {
                bridges_created++;
                LOG_DEBUG(LOG_MODULE, "  - Thousand Brains Bridge (0x0E6E) created");
            } else {
                LOG_WARN(LOG_MODULE, "  - Thousand Brains Bridge failed to connect");
                bridges_failed++;
            }
        } else {
            LOG_WARN(LOG_MODULE, "  - Thousand Brains Bridge creation failed");
            bridges_failed++;
        }
    }

    LOG_INFO(LOG_MODULE, "World Model bridge wiring complete: %u created, %u failed",
             bridges_created, bridges_failed);

    /* Return success even if some bridges failed - non-fatal */
    return true;
}

void nimcp_brain_factory_destroy_world_model_bridges(struct brain_struct* brain) {
    if (!brain) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying World Model integration bridges...");

    /* Destroy in reverse order of creation */

    /* 12. Thousand Brains Bridge */
    if (brain->wm_thousand_brains_bridge) {
        wm_tb_bridge_destroy(brain->wm_thousand_brains_bridge);
        brain->wm_thousand_brains_bridge = NULL;
    }

    /* 11. Plasticity Bridge */
    if (brain->wm_plasticity_bridge) {
        omni_wm_plasticity_bridge_destroy(brain->wm_plasticity_bridge);
        brain->wm_plasticity_bridge = NULL;
    }

    /* 10. ToM Bridge */
    if (brain->wm_tom_bridge) {
        omni_wm_tom_bridge_destroy(brain->wm_tom_bridge);
        brain->wm_tom_bridge = NULL;
    }

    /* 9. KG Bridge */
    if (brain->wm_kg_bridge) {
        omni_wm_kg_bridge_destroy(brain->wm_kg_bridge);
        brain->wm_kg_bridge = NULL;
    }

    /* 8. Memory Bridge */
    if (brain->wm_memory_bridge) {
        omni_wm_memory_bridge_destroy(brain->wm_memory_bridge);
        brain->wm_memory_bridge = NULL;
    }

    /* 7. Substrate Bridge */
    if (brain->wm_substrate_bridge) {
        omni_wm_substrate_bridge_destroy(brain->wm_substrate_bridge);
        brain->wm_substrate_bridge = NULL;
    }

    /* 6. Thalamic Bridge */
    if (brain->wm_thalamic_bridge) {
        omni_wm_thalamic_bridge_destroy(brain->wm_thalamic_bridge);
        brain->wm_thalamic_bridge = NULL;
    }

    /* 5. Hypothalamus Bridge */
    if (brain->wm_hypothalamus_bridge) {
        omni_wm_hypothalamus_bridge_destroy(brain->wm_hypothalamus_bridge);
        brain->wm_hypothalamus_bridge = NULL;
    }

    /* 4. Parietal Bridge */
    if (brain->wm_parietal_bridge) {
        omni_wm_parietal_bridge_destroy(brain->wm_parietal_bridge);
        brain->wm_parietal_bridge = NULL;
    }

    /* 3. Cognitive Bridge */
    if (brain->wm_cognitive_bridge) {
        omni_wm_cognitive_bridge_destroy(brain->wm_cognitive_bridge);
        brain->wm_cognitive_bridge = NULL;
    }

    /* 2. Logging Bridge */
    if (brain->wm_logging_bridge) {
        omni_wm_logging_bridge_destroy(brain->wm_logging_bridge);
        brain->wm_logging_bridge = NULL;
    }

    /* 1. Security-Immune Bridge */
    if (brain->wm_security_immune_bridge) {
        omni_wm_security_immune_bridge_destroy(brain->wm_security_immune_bridge);
        brain->wm_security_immune_bridge = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "World Model integration bridges destroyed");
}

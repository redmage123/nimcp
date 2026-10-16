//=============================================================================
// nimcp_brain_init_basal_ganglia.c - Basal Ganglia Subsystem Initialization
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_basal_ganglia.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.h"  /* W4 */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_basal_ganglia, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Mesh Integration (Phase: Mesh Network Integration)
//=============================================================================

/* Mesh integration - forward declarations to avoid header conflicts */
struct mesh_bootstrap;
typedef struct mesh_bootstrap mesh_bootstrap_t;
struct mesh_basal_ganglia_integration;
typedef struct mesh_basal_ganglia_integration mesh_basal_ganglia_integration_t;

/* Mesh basal ganglia integration function declarations */
extern mesh_basal_ganglia_integration_t* mesh_basal_ganglia_create(
    mesh_bootstrap_t* bootstrap,
    void* basal_ganglia,
    const void* config
);
extern void mesh_basal_ganglia_destroy(mesh_basal_ganglia_integration_t* integration);
extern int mesh_basal_ganglia_register_participant(mesh_basal_ganglia_integration_t* integration);
extern int mesh_basal_ganglia_set_health_agent(mesh_basal_ganglia_integration_t* integration,
                                                void* agent);

/** Mutex protecting global mesh state (set/get/init/destroy races) */
static pthread_mutex_t g_bg_mesh_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Global mesh bootstrap handle (set externally) */
static mesh_bootstrap_t* g_bg_mesh_bootstrap = NULL;
static mesh_basal_ganglia_integration_t* g_bg_mesh_integration = NULL;

/**
 * @brief Set the mesh bootstrap handle for basal ganglia mesh registration
 *
 * WHAT: Configures basal ganglia init to register with mesh network
 * WHY:  Enable coordinated action selection via mesh consensus
 * HOW:  Stores bootstrap handle, used during basal ganglia init
 *
 * @param bootstrap Mesh bootstrap handle (NULL to disable)
 */
void nimcp_brain_bg_set_mesh_bootstrap(mesh_bootstrap_t* bootstrap) {
    pthread_mutex_lock(&g_bg_mesh_mutex);
    g_bg_mesh_bootstrap = bootstrap;
    pthread_mutex_unlock(&g_bg_mesh_mutex);
    if (bootstrap) {
        NIMCP_LOGGING_INFO("Mesh bootstrap set for basal ganglia integration");
    }
}

/**
 * @brief Get the basal ganglia mesh integration handle
 *
 * @return Current mesh integration or NULL if not registered
 */
mesh_basal_ganglia_integration_t* nimcp_brain_bg_get_mesh_integration(void) {
    pthread_mutex_lock(&g_bg_mesh_mutex);
    mesh_basal_ganglia_integration_t* result = g_bg_mesh_integration;
    pthread_mutex_unlock(&g_bg_mesh_mutex);
    return result;
}

//=============================================================================
// Configuration
//=============================================================================

void nimcp_brain_bg_default_config(brain_t brain, bg_enhanced_config_t* config) {
    if (!config) return;

    /* Start with module defaults */
    bg_enhanced_default_config(config);

    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_default_config: brain is NULL"); return; }

    struct brain_struct* b = (struct brain_struct*)brain;

    /* Enable features based on brain's enabled subsystems */

    /* Beta oscillations - always enable for biological realism */
    config->features.enable_beta_oscillations = true;

    /* Multi-neuromodulators - integrate with brain's neuromodulator system */
    config->features.enable_multi_neuromod = true;

    /* Hierarchical RL - enable if executive is present */
    config->features.enable_hierarchical_rl = (b->executive != NULL);

    /* Model-based planning - enable if predictive coding is present */
    config->features.enable_model_based = (b->predictive_coding != NULL);

    /* Nucleus accumbens - enable if emotional system is present */
    config->features.enable_nucleus_accumbens = (b->emotional_system != NULL);

    /* Superior colliculus - enable if dragonfly is present (visual tracking) */
    config->features.enable_superior_colliculus = b->dragonfly_enabled;

    /* Striatal interneurons - always enable for proper timing */
    config->features.enable_interneurons = true;

    /* Cerebellar coordination - enable if we have motor output */
    config->features.enable_cerebellar_coord = b->dragonfly_enabled;

    /* Outcome devaluation - enable if working memory is present */
    config->features.enable_outcome_deval = (b->working_memory != NULL);

    /* Temporal credit assignment - always enable for learning */
    config->features.enable_temporal_credit = true;

    /* Training plasticity - always enable for reinforcement learning */
    config->features.enable_training_plasticity = true;

    /* Core BG config based on brain size */
    uint32_t num_actions = b->config.num_outputs;
    if (num_actions == 0) num_actions = 16;  /* Default */
    if (num_actions > BG_MAX_ACTIONS) num_actions = BG_MAX_ACTIONS;

    config->core_config.num_actions = num_actions;

    NIMCP_LOGGING_DEBUG("BG config: %u actions, beta=%d, neuromod=%d, hrl=%d, mb=%d, nac=%d, sc=%d, int=%d, cb=%d, od=%d, tc=%d, tr=%d",
        num_actions,
        config->features.enable_beta_oscillations,
        config->features.enable_multi_neuromod,
        config->features.enable_hierarchical_rl,
        config->features.enable_model_based,
        config->features.enable_nucleus_accumbens,
        config->features.enable_superior_colliculus,
        config->features.enable_interneurons,
        config->features.enable_cerebellar_coord,
        config->features.enable_outcome_deval,
        config->features.enable_temporal_credit,
        config->features.enable_training_plasticity);
}

//=============================================================================
// Lifecycle
//=============================================================================

bool nimcp_brain_factory_init_basal_ganglia_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("Cannot init BG: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_basal_ganglia_subsystem: brain is NULL");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    /* Skip if already initialized */
    if (b->basal_ganglia != NULL) {
        NIMCP_LOGGING_WARN("Basal ganglia already initialized");
        return true;
    }

    NIMCP_LOGGING_INFO("Initializing enhanced basal ganglia subsystem...");

    /* Get brain-appropriate configuration */
    bg_enhanced_config_t config;
    nimcp_brain_bg_default_config(brain, &config);

    /* Create enhanced basal ganglia */
    b->basal_ganglia = bg_enhanced_create(&config);
    if (!b->basal_ganglia) {
        NIMCP_LOGGING_ERROR("Failed to create enhanced basal ganglia");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_basal_ganglia_subsystem: b->basal_ganglia is NULL");
        return false;
    }

    b->basal_ganglia_enabled = true;
    b->last_basal_ganglia_update_us = 0;

    /* Wire BBB system into basal ganglia for input validation */
    if (b->bbb_system) {
        basal_ganglia_set_bbb(b->bbb_system);
        NIMCP_LOGGING_DEBUG("BG: BBB system wired for input validation");
    }

    /* Wire KG persistence context into basal ganglia for state recording */
    if (b->kg_persistence) {
        basal_ganglia_set_kg_context(b->kg_persistence);
        NIMCP_LOGGING_DEBUG("BG: KG persistence context wired for state recording");
    }

    NIMCP_LOGGING_INFO("Enhanced basal ganglia initialized successfully");

    /* Register with mesh network if bootstrap is available */
    pthread_mutex_lock(&g_bg_mesh_mutex);
    if (g_bg_mesh_bootstrap) {
        brain_init_basal_ganglia_heartbeat("mesh_registration", 0.0f);

        mesh_basal_ganglia_integration_t* mesh_integration = mesh_basal_ganglia_create(
            g_bg_mesh_bootstrap,
            b->basal_ganglia,
            NULL  /* Use default config */
        );

        if (mesh_integration) {
            /* Set health agent if available */
            if (g_brain_init_basal_ganglia_health_agent) {
                mesh_basal_ganglia_set_health_agent(mesh_integration,
                                                    g_brain_init_basal_ganglia_health_agent);
            }

            /* Register as mesh participant */
            int mesh_result = mesh_basal_ganglia_register_participant(mesh_integration);
            if (mesh_result == NIMCP_SUCCESS) {
                g_bg_mesh_integration = mesh_integration;
                NIMCP_LOGGING_INFO("Basal ganglia registered with mesh network");
            } else {
                NIMCP_LOGGING_WARN("Failed to register basal ganglia with mesh (error %d)",
                                   mesh_result);
                mesh_basal_ganglia_destroy(mesh_integration);
            }
        } else {
            NIMCP_LOGGING_WARN("Failed to create basal ganglia mesh integration");
        }

        brain_init_basal_ganglia_heartbeat("mesh_registration", 1.0f);
    }
    pthread_mutex_unlock(&g_bg_mesh_mutex);

    /* Log enabled features */
    NIMCP_LOGGING_DEBUG("BG features enabled:");
    if (config.features.enable_beta_oscillations) NIMCP_LOGGING_DEBUG("  - Beta oscillations");
    if (config.features.enable_multi_neuromod) NIMCP_LOGGING_DEBUG("  - Multi-neuromodulators");
    if (config.features.enable_hierarchical_rl) NIMCP_LOGGING_DEBUG("  - Hierarchical RL");
    if (config.features.enable_model_based) NIMCP_LOGGING_DEBUG("  - Model-based planning");
    if (config.features.enable_nucleus_accumbens) NIMCP_LOGGING_DEBUG("  - Nucleus accumbens");
    if (config.features.enable_superior_colliculus) NIMCP_LOGGING_DEBUG("  - Superior colliculus");
    if (config.features.enable_interneurons) NIMCP_LOGGING_DEBUG("  - Striatal interneurons");
    if (config.features.enable_cerebellar_coord) NIMCP_LOGGING_DEBUG("  - Cerebellar coordination");
    if (config.features.enable_outcome_deval) NIMCP_LOGGING_DEBUG("  - Outcome devaluation");
    if (config.features.enable_temporal_credit) NIMCP_LOGGING_DEBUG("  - Temporal credit assignment");
    if (config.features.enable_training_plasticity) NIMCP_LOGGING_DEBUG("  - Training plasticity");

    return true;
}

void nimcp_brain_bg_destroy(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_destroy: brain is NULL"); return; }

    struct brain_struct* b = (struct brain_struct*)brain;

    /* Destroy mesh integration first */
    pthread_mutex_lock(&g_bg_mesh_mutex);
    if (g_bg_mesh_integration) {
        mesh_basal_ganglia_destroy(g_bg_mesh_integration);
        g_bg_mesh_integration = NULL;
        NIMCP_LOGGING_DEBUG("Basal ganglia mesh integration destroyed");
    }
    pthread_mutex_unlock(&g_bg_mesh_mutex);

    if (b->basal_ganglia) {
        bg_enhanced_destroy(b->basal_ganglia);
        b->basal_ganglia = NULL;
        b->basal_ganglia_enabled = false;
        NIMCP_LOGGING_DEBUG("Basal ganglia destroyed");
    }
}

//=============================================================================
// Processing
//=============================================================================

int nimcp_brain_bg_step(brain_t brain, float dt_ms) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_step: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return 0;  /* Not enabled, skip silently */
    }

    /* Update BG */
    int result = bg_enhanced_step(b->basal_ganglia, dt_ms);

    if (result == 0) {
        b->last_basal_ganglia_update_us = b->current_time_us;
    }

    return result;
}

int nimcp_brain_bg_select_action(brain_t brain,
                                  const float* cortical_input,
                                  uint32_t* selected_action) {
    if (!brain || !cortical_input || !selected_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_select_action: required parameter is NULL (brain, cortical_input, selected_action)");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        /* BG not enabled - fall back to max activation over known outputs */
        uint32_t num_actions = b->config.num_outputs;
        if (num_actions == 0) {
            /* No outputs configured - cannot safely index cortical_input */
            *selected_action = 0;
            return 0;
        }
        /* Safety clamp: never read beyond 64 elements even if config is misconfigured */
        if (num_actions > 64) num_actions = 64;

        float max_val = cortical_input[0];
        *selected_action = 0;
        for (uint32_t i = 1; i < num_actions; i++) {
            if (cortical_input[i] > max_val) {
                max_val = cortical_input[i];
                *selected_action = i;
            }
        }
        /* W4: emit KG event even in fallback path so symbolic queries still see it. */
        subcortical_emit_action_selected(brain, *selected_action, max_val);
        return 0;
    }

    int rc = bg_enhanced_select_action(b->basal_ganglia, cortical_input, selected_action);
    if (rc == 0) {
        /* W4: per-action-selection KG event (hot-path hook). */
        subcortical_emit_action_selected(brain, *selected_action, 1.0f);
    }
    return rc;
}

int nimcp_brain_bg_process_reward(brain_t brain,
                                   float reward,
                                   float predicted_reward) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_process_reward: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return 0;  /* Not enabled, skip silently */
    }

    /* W4: emit NAcc RPE event + SN dopamine release event. */
    float rpe = reward - predicted_reward;
    subcortical_emit_reward_prediction(brain, rpe);
    if (rpe > 0.0f) {
        /* Positive RPE drives phasic DA burst (Schultz 1997). */
        subcortical_emit_dopamine_release(brain, rpe);
    }

    return bg_enhanced_process_reward(b->basal_ganglia, reward, predicted_reward);
}

//=============================================================================
// Integration Callbacks
//=============================================================================

void nimcp_brain_bg_on_emotional_signal(brain_t brain,
                                         float valence,
                                         float arousal) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_on_emotional_signal: brain is NULL"); return; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) return;

    /* Route emotional signal to nucleus accumbens */
    nucleus_accumbens_t* nac = bg_enhanced_get_nac(b->basal_ganglia);
    if (nac) {
        nac_receive_amygdala_input(nac, valence, arousal);
    }

    /* Also affect neuromodulators based on valence */
    bg_neuromod_system_t* neuromod = bg_enhanced_get_neuromod(b->basal_ganglia);
    if (neuromod) {
        if (valence > 0) {
            /* Positive valence -> dopamine burst */
            bg_neuromod_trigger_release(neuromod, BG_NEUROMOD_DOPAMINE, valence * 0.5f);
        } else if (valence < 0) {
            /* Negative valence -> serotonin/NE release */
            bg_neuromod_process_aversion(neuromod, -valence);
        }
    }
}

void nimcp_brain_bg_on_goal_change(brain_t brain,
                                    uint32_t goal_id,
                                    bool is_active) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_on_goal_change: brain is NULL"); return; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) return;

    /* Update HRL system with goal change */
    bg_hrl_system_t* hrl = bg_enhanced_get_hrl(b->basal_ganglia);
    if (hrl) {
        if (is_active) {
            bg_hrl_activate_goal(hrl, goal_id);
        } else {
            /* Goal deactivated */
            bg_hrl_deactivate_goal(hrl, goal_id);
        }
    }
}

void nimcp_brain_bg_on_arousal_change(brain_t brain,
                                       float arousal_level) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_on_arousal_change: brain is NULL"); return; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) return;

    /* Arousal affects neuromodulator levels */
    bg_neuromod_system_t* neuromod = bg_enhanced_get_neuromod(b->basal_ganglia);
    if (neuromod) {
        /* High arousal -> norepinephrine release */
        bg_neuromod_set_level(neuromod, BG_NEUROMOD_NOREPINEPHRINE, arousal_level);

        /* Low arousal -> adenosine accumulation (fatigue) */
        if (arousal_level < 0.3f) {
            bg_neuromod_trigger_release(neuromod, BG_NEUROMOD_ADENOSINE,
                                         (0.3f - arousal_level) * 2.0f);
        }
    }
}

//=============================================================================
// Query Functions
//=============================================================================

int nimcp_brain_bg_get_stats(brain_t brain, bg_enhanced_stats_t* stats) {
    if (!brain || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_stats: required parameter is NULL (brain, stats)");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        memset(stats, 0, sizeof(*stats));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_stats: required parameter is NULL (b->basal_ganglia_enabled, b->basal_ganglia)");
        return -1;
    }

    return bg_enhanced_get_stats(b->basal_ganglia, stats);
}

bool nimcp_brain_bg_is_enabled(brain_t brain) {
    if (!brain) {
        return false;
    }
    struct brain_struct* b = (struct brain_struct*)brain;
    return b->basal_ganglia_enabled && b->basal_ganglia != NULL;
}

bgod_behavior_type_t nimcp_brain_bg_get_behavior_type(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_behavior_type: brain is NULL"); return BGOD_BEHAVIOR_UNKNOWN; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return BGOD_BEHAVIOR_UNKNOWN;
    }

    bg_outcome_deval_t* od = bg_enhanced_get_outcome_deval(b->basal_ganglia);
    if (od) {
        return bgod_get_overall_behavior(od);
    }

    return BGOD_BEHAVIOR_UNKNOWN;
}

float nimcp_brain_bg_get_motivation(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_motivation: brain is NULL"); return 0.0f; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return 0.5f;  /* Default neutral motivation */
    }

    return bg_enhanced_get_motivation(b->basal_ganglia);
}

//=============================================================================
// Training Integration API
//=============================================================================

bgtr_bridge_t* nimcp_brain_bg_get_training_bridge(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_training_bridge: required parameter is NULL (b->basal_ganglia_enabled, b->basal_ganglia)");
        return NULL;
    }

    return bg_enhanced_get_training_bridge(b->basal_ganglia);
}

int nimcp_brain_bg_get_training_stats(brain_t brain, bgtr_bridge_stats_t* stats) {
    if (!brain || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_training_stats: required parameter is NULL (brain, stats)");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        memset(stats, 0, sizeof(*stats));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_training_stats: required parameter is NULL (b->basal_ganglia_enabled, b->basal_ganglia)");
        return -1;
    }

    bgtr_bridge_t* training = bg_enhanced_get_training_bridge(b->basal_ganglia);
    if (!training) {
        memset(stats, 0, sizeof(*stats));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_training_stats: training is NULL");
        return -1;
    }

    return bgtr_bridge_get_stats(training, stats);
}

int nimcp_brain_bg_connect_training_context(brain_t brain,
                                             nimcp_training_context_t* training) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_connect_training_context: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        LOG_DEBUG("nimcp_brain_bg_connect_training_context: BG not enabled or NULL");
        return -1;
    }

    bgtr_bridge_t* bridge = bg_enhanced_get_training_bridge(b->basal_ganglia);
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    return bgtr_bridge_connect_training(bridge, training);
}

float nimcp_brain_bg_get_last_rpe(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_last_rpe: brain is NULL"); return 0.0f; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return 0.0f;
    }

    bgtr_bridge_t* bridge = bg_enhanced_get_training_bridge(b->basal_ganglia);
    if (!bridge) {
        return 0.0f;
    }

    return bridge->last_rpe;
}

uint32_t nimcp_brain_bg_get_active_traces(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_bg_get_active_traces: brain is NULL"); return 0; }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->basal_ganglia_enabled || !b->basal_ganglia) {
        return 0;
    }

    bgtr_bridge_t* bridge = bg_enhanced_get_training_bridge(b->basal_ganglia);
    if (!bridge) {
        return 0;
    }

    return bgtr_bridge_get_trace_count(bridge);
}

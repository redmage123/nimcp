//=============================================================================
// nimcp_brain_init_parietal.c - Parietal Lobe Initialization & Integration
//=============================================================================
/**
 * @file nimcp_brain_init_parietal.c
 * @brief Parietal lobe subsystem initialization with full brain integration
 *
 * BIOLOGICAL BASIS:
 * The parietal lobe (posterior parietal cortex) is essential for:
 * - Number sense (intraparietal sulcus - IPS)
 * - Spatial reasoning (superior parietal lobule - SPL)
 * - Mathematical intuition (angular gyrus)
 * - Scientific reasoning (integration with prefrontal cortex)
 *
 * INTEGRATION POINTS:
 * - Working Memory: Mathematical problem-solving requires WM resources
 * - Training Layer: Parietal guides mathematical learning optimization
 * - Immune System: Inflammation affects numerical precision
 * - FEP Orchestrator: Mathematical reasoning modulates free energy
 * - Sleep/Medulla: Fatigue affects spatial and numerical accuracy
 * - Global Workspace: Parietal results broadcast for conscious access
 * - Theory of Mind: Spatial reasoning for agent tracking
 * - Logic Gates: Mathematical operations use neural logic
 *
 * @version 2.0.0
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "core/brain/regions/parietal/nimcp_parietal_quantum_bridge.h"
#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_parietal, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Forward Declarations for Optional Integration Types
//=============================================================================

/* These may not be available if modules are disabled */
struct fep_brain_struct;
struct thalamic_router_struct;
struct substrate_interface_struct;
struct perception_system_struct;

//=============================================================================
// Integration Helper Functions
//=============================================================================

/**
 * @brief Connect parietal to working memory subsystem
 *
 * Working memory integration enables:
 * - Mathematical problem-solving with WM item manipulation
 * - Spatial working memory for mental rotation tasks
 * - Number sense for WM capacity estimation
 */
static void connect_parietal_to_working_memory(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->working_memory) {
        return;
    }

    /* Cast to expected type - working_memory_t is typedef struct working_memory */
    working_memory_t* wm = brain->working_memory;
    int result = parietal_attach_working_memory(parietal, wm);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to working memory\n");
    }
}

/**
 * @brief Connect parietal to training subsystem
 *
 * Training integration enables:
 * - Mathematical optimization of learning rates
 * - Pattern-based curriculum learning
 * - Scientific hypothesis testing for training experiments
 */
static void connect_parietal_to_training(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->training_ctx || !brain->enable_training_integration) {
        return;
    }

    /* Training context provides training_engine interface */
    training_engine_t* training = (training_engine_t*)brain->training_ctx;
    int result = parietal_attach_training(parietal, training);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to training layer\n");
    }
}

/**
 * @brief Connect parietal to immune system
 *
 * Immune integration enables:
 * - Inflammation affects Weber fraction (numerical precision)
 * - Immune stress reduces spatial accuracy
 * - Cytokine signaling modulates mathematical confidence
 */
static void connect_parietal_to_immune(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->immune_system || !brain->immune_enabled) {
        return;
    }

    /* Brain immune system provides code_immune_system interface */
    code_immune_system_t* immune = (code_immune_system_t*)brain->immune_system;
    int result = parietal_attach_immune(parietal, immune);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to immune system (inflammation modulation)\n");
    }
}

/**
 * @brief Connect parietal to FEP orchestrator
 *
 * FEP integration enables:
 * - Mathematical free energy computation
 * - Prediction error quantification using number sense
 * - Hypothesis testing for active inference
 */
static void connect_parietal_to_fep(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->fep_orchestrator || !brain->fep_orchestrator_enabled) {
        return;
    }

    /* FEP orchestrator provides fep_brain interface */
    fep_brain_t* fep = (fep_brain_t*)brain->fep_orchestrator;
    int result = parietal_attach_fep(parietal, fep);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to FEP orchestrator\n");
    }
}

/**
 * @brief Connect parietal to sleep/medulla system
 *
 * Sleep integration enables:
 * - Fatigue affects spatial reasoning speed
 * - Circadian modulation of numerical precision
 * - Sleep deprivation increases Weber fraction
 */
static void connect_parietal_to_sleep(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->medulla || !brain->medulla_enabled) {
        return;
    }

    /* Medulla provides sleep_system interface via its sleep subsystem */
    /* For now, we use the brain's sleep_system directly */
    sleep_system_t sleep = brain->sleep_system;
    if (sleep) {
        int result = parietal_attach_sleep(parietal, &sleep);
        if (result == 0) {
            fprintf(stderr, "[PARIETAL] Connected to sleep system (fatigue modulation)\n");
        }
    }
}

/**
 * @brief Connect parietal to logic gate network
 *
 * Logic gate integration enables:
 * - Neural logic operations for mathematical reasoning
 * - Boolean algebra using spiking logic gates
 * - Arithmetic circuits with GPU acceleration
 */
static void connect_parietal_to_logic(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->logic) {
        return;
    }

    /* Neural logic network provides logic_gate_network interface */
    logic_gate_network_t* logic = (logic_gate_network_t*)brain->logic;
    int result = parietal_attach_logic_gates(parietal, logic);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to logic gate network\n");
    }
}

/**
 * @brief Connect parietal to perception system (if available)
 *
 * Perception integration enables:
 * - Visual number processing (numerosity from visual input)
 * - Spatial perception for coordinate transforms
 * - Multi-modal magnitude estimation
 */
static void connect_parietal_to_perception(parietal_lobe_t* parietal, brain_t brain) {
    /* Check if visual cortex is available as perception proxy */
    if (!brain->visual_cortex) {
        return;
    }

    /* Visual cortex can serve as perception system interface */
    perception_system_t* perception = (perception_system_t*)brain->visual_cortex;
    int result = parietal_attach_perception(parietal, perception);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Connected to perception system\n");
    }
}

/**
 * @brief Subscribe parietal to global workspace broadcasts
 *
 * Global workspace integration enables:
 * - Parietal results broadcast for conscious access
 * - Mathematical reasoning contributes to workspace competition
 * - Spatial attention integrated with workspace
 */
static void connect_parietal_to_global_workspace(parietal_lobe_t* parietal, brain_t brain) {
    (void)parietal;  /* Parietal doesn't have direct GW attach, but we can subscribe */

    if (!brain->global_workspace) {
        return;
    }

    /* Subscribe parietal module to global workspace broadcasts */
    /* MODULE_PARIETAL should be defined - if not, use a reasonable value */
    #ifndef MODULE_PARIETAL
    #define MODULE_PARIETAL 41  /* Matches PARIETAL_BRAIN_REGION_TYPE */
    #endif

    /* TODO: This function is a no-op. It should:
     *   1. Call global_workspace_subscribe(brain->global_workspace, MODULE_PARIETAL)
     *   2. Register a GW broadcast callback so parietal receives workspace broadcasts
     *   3. Implement parietal_on_gw_broadcast() to process incoming broadcast content
     * Currently only prints a log message without performing any actual connection. */

    /* Note: global_workspace_subscribe takes module ID, not parietal pointer */
    /* The parietal will need to implement GW callbacks separately */
    fprintf(stderr, "[PARIETAL] Global workspace integration prepared (module %d)\n", MODULE_PARIETAL);
}

/**
 * @brief Connect parietal to brain region architecture
 *
 * Brain region integration enables:
 * - Parietal creates its own brain region with cortical layers
 * - Connections to other regions (prefrontal, temporal, motor)
 * - Hierarchical processing within parietal subregions
 */
static void connect_parietal_to_brain_regions(parietal_lobe_t* parietal, brain_t brain) {
    if (!brain->brain_regions) {
        return;
    }

    /* Attach parietal to brain module with standard neuron count */
    brain_module_t* brain_module = brain->brain_regions;
    uint32_t parietal_neurons = 10000;  /* Reasonable default for parietal */

    int result = parietal_attach_to_brain(parietal, brain_module, parietal_neurons);
    if (result == 0) {
        fprintf(stderr, "[PARIETAL] Attached to brain region architecture (%u neurons)\n", parietal_neurons);
    }
}

//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize parietal lobe subsystem for brain with full integration
 *
 * Creates and configures the parietal lobe module based on brain configuration,
 * then connects it to all available brain subsystems.
 *
 * @param brain Brain instance to initialize parietal for
 * @return true on success, false on failure
 *
 * PROCESS:
 * 1. Check if parietal is enabled in brain config
 * 2. Get default parietal configuration
 * 3. Map brain config fields to parietal submodule configs
 * 4. Create parietal lobe with custom configuration
 * 5. Store in brain->parietal
 * 6. Connect to all available subsystems
 */
bool nimcp_brain_factory_init_parietal_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_parietal_subsystem: brain is NULL");

            return false;
    }

    /* Check if parietal is disabled in config */
    if (!brain->config.enable_parietal) {
        brain->parietal = NULL;
        brain->parietal_enabled = false;
        brain->last_parietal_update_us = 0;
        return true;  /* Success - parietal is disabled by config */
    }

    fprintf(stderr, "[PARIETAL] Initializing parietal lobe subsystem...\n");

    /* Get default parietal configuration */
    parietal_config_t config = parietal_default_config();

    /* Map brain config fields to parietal submodule configs */

    /* Weber fraction affects magnitude discrimination precision */
    if (brain->config.parietal_weber_fraction > 0.0f) {
        config.number_sense.weber_fraction = brain->config.parietal_weber_fraction;
    }

    /* Subitizing limit affects instant small-number recognition */
    if (brain->config.parietal_subitizing_limit > 0) {
        config.number_sense.subitizing_limit = brain->config.parietal_subitizing_limit;
    }

    /* Mental rotation rate (degrees per millisecond) */
    if (brain->config.parietal_rotation_rate_deg_ms > 0.0f) {
        config.spatial.rotation_rate_deg_ms = brain->config.parietal_rotation_rate_deg_ms;
    }

    /* Enable integration features based on brain configuration */
    config.enable_immune_bridge = brain->immune_enabled;
    config.enable_fep_bridge = brain->fep_orchestrator_enabled;
    config.enable_working_memory = (brain->working_memory != NULL);
    config.enable_training = brain->enable_training_integration;
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_sleep_modulation = brain->medulla_enabled;
    config.enable_logic_gates = (brain->logic != NULL);

    /* Scale FEP hierarchy dimensions to avoid buffer overflow.
     * brain_learn_vector passes features of size brain->config.num_inputs
     * to fep_parietal_train_model which uses level_dims[0]. If level_dims[0]
     * is smaller than num_inputs, the train function overflows its buffer.
     * Cap at 32 to keep memory reasonable — the train function will
     * truncate features to this size. */
    if (brain->config.num_inputs > 0) {
        uint32_t dim = brain->config.num_inputs;
        if (dim > 32) dim = 32;
        for (uint32_t i = 0; i < config.fep_parietal_config.num_levels; i++) {
            if (config.fep_parietal_config.level_dims[i] == 0 ||
                config.fep_parietal_config.level_dims[i] > dim) {
                config.fep_parietal_config.level_dims[i] = dim;
            }
            /* Each level can be same or smaller */
            dim = (dim > 8) ? dim / 2 : dim;
        }
    }

    /* Create parietal lobe with dimension-matched configuration */
    parietal_lobe_t* parietal = parietal_create_custom(&config);
    if (!parietal) {
        fprintf(stderr, "[PARIETAL] ERROR: Failed to create parietal lobe\n");
        brain->parietal = NULL;
        brain->parietal_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_parietal_subsystem: parietal is NULL");
        return false;
    }

    /* Store parietal in brain */
    brain->parietal = parietal;
    brain->parietal_enabled = true;
    brain->last_parietal_update_us = 0;

    /* The parietal_create_custom function calloc's all internal state,
     * so pending_requests array and num_pending are already zero.
     * No need to clear — the fresh parietal starts clean. */

    fprintf(stderr, "[PARIETAL] Parietal lobe created, connecting to subsystems...\n");

    /* Connect subsystems — all connections are safe because they only
     * set callback/function pointers, not allocate buffers. */
    connect_parietal_to_working_memory(parietal, brain);
    connect_parietal_to_fep(parietal, brain);
    connect_parietal_to_logic(parietal, brain);
    connect_parietal_to_global_workspace(parietal, brain);
    connect_parietal_to_training(parietal, brain);
    connect_parietal_to_immune(parietal, brain);
    connect_parietal_to_sleep(parietal, brain);
    connect_parietal_to_perception(parietal, brain);
    connect_parietal_to_brain_regions(parietal, brain);

    if (brain->config.enable_intuitive_physics) {
        extern int parietal_attach_simulation_engines(parietal_lobe_t* pl,
                                                       struct brain_struct* b);
        if (parietal_attach_simulation_engines(parietal, brain) == 0) {
            fprintf(stderr, "[PARIETAL] Connected to world model simulation engines\n");
        }
    }

    fprintf(stderr, "[PARIETAL] Parietal lobe initialization complete\n");
    fprintf(stderr, "[PARIETAL]   Weber fraction: %.3f\n", config.number_sense.weber_fraction);
    fprintf(stderr, "[PARIETAL]   Subitizing limit: %u\n", config.number_sense.subitizing_limit);
    fprintf(stderr, "[PARIETAL]   Rotation rate: %.3f deg/ms\n", config.spatial.rotation_rate_deg_ms);

    return true;
}

//=============================================================================
// Accessor Function
//=============================================================================

/**
 * @brief Get parietal lobe from brain
 *
 * @param brain Brain instance
 * @return Parietal lobe handle or NULL if not enabled
 */
parietal_lobe_t* brain_get_parietal(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_get_parietal: brain is NULL");
        return NULL;
    }
    /* Feature disabled is a normal state — return NULL without throwing */
    if (!brain->parietal_enabled) {
        return NULL;
    }
    return brain->parietal;
}

//=============================================================================
// Runtime Integration Functions
//=============================================================================

/**
 * @brief Update parietal from immune system state
 *
 * Call this periodically to sync inflammation levels with parietal precision.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_from_immune(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_update_parietal_from_immune: brain is NULL");
        return -1;
    }
    /* Feature disabled is a normal state — return success without throwing */
    if (!brain->parietal_enabled || !brain->parietal) {
        return 0;
    }

    return parietal_update_from_immune(brain->parietal);
}

/**
 * @brief Update parietal from sleep system state
 *
 * Call this periodically to sync fatigue levels with parietal accuracy.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_from_sleep(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_update_parietal_from_sleep: brain is NULL");
        return -1;
    }
    /* Feature disabled is a normal state — return success without throwing */
    if (!brain->parietal_enabled || !brain->parietal) {
        return 0;
    }

    return parietal_update_from_sleep(brain->parietal);
}

/**
 * @brief Step parietal lobe forward in time
 *
 * Call this during brain stepping to process pending parietal requests.
 *
 * @param brain Brain instance
 * @param delta_t Time step in microseconds
 * @return 0 on success, -1 on error
 */
int brain_step_parietal(brain_t brain, uint64_t delta_t) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_step_parietal: brain is NULL");
        return -1;
    }
    /* Feature disabled is a normal state — return success without throwing */
    if (!brain->parietal_enabled || !brain->parietal) {
        return 0;
    }

    int result = parietal_step(brain->parietal, delta_t);
    if (result == 0) {
        brain->last_parietal_update_us += delta_t;
    }
    return result;
}

//=============================================================================
// Intuition System Integration (Phase 6 Reasoning Engines)
//=============================================================================

/**
 * @brief Connect intuition system to working memory
 */
static void connect_intuition_to_working_memory(intuition_system_t* intuition, brain_t brain) {
    if (!brain->working_memory) {
        return;
    }

    working_memory_t* wm = brain->working_memory;
    int result = intuition_attach_working_memory(intuition, wm);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to working memory\n");
    }
}

/**
 * @brief Connect intuition system to training engine
 */
static void connect_intuition_to_training(intuition_system_t* intuition, brain_t brain) {
    if (!brain->training_ctx || !brain->enable_training_integration) {
        return;
    }

    training_engine_t* training = (training_engine_t*)brain->training_ctx;
    int result = intuition_attach_training(intuition, training);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to training layer\n");
    }
}

/**
 * @brief Connect intuition system to attention
 */
static void connect_intuition_to_attention(intuition_system_t* intuition, brain_t brain) {
    /* Attention system in brain is multihead_attention_t, we need attention_system_t */
    /* For now, skip this connection - would need attention adapter */
    (void)intuition;
    (void)brain;
    /* TODO: This function is permanently stubbed. To implement:
     *   1. Create an attention_system_t adapter that wraps multihead_attention_t
     *   2. Call intuition_attach_attention(intuition, adapter) to connect
     *   3. This would allow intuition to guide attentional focus allocation
     * Blocked on defining the adapter interface between the two attention types. */
}

/**
 * @brief Connect intuition system to executive functions
 */
static void connect_intuition_to_executive(intuition_system_t* intuition, brain_t brain) {
    if (!brain->executive) {
        return;
    }

    executive_function_t* exec = (executive_function_t*)brain->executive;
    int result = intuition_attach_executive(intuition, exec);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to executive functions\n");
    }
}

/**
 * @brief Connect intuition system to emotion system
 */
static void connect_intuition_to_emotion(intuition_system_t* intuition, brain_t brain) {
    if (!brain->emotional_system) {
        return;
    }

    emotion_system_t* emotion = (emotion_system_t*)brain->emotional_system;
    int result = intuition_attach_emotion(intuition, emotion);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to emotion system (gut feelings)\n");
    }
}

/**
 * @brief Connect intuition system to logic gates
 */
static void connect_intuition_to_logic(intuition_system_t* intuition, brain_t brain) {
    if (!brain->logic) {
        return;
    }

    logic_gate_network_t* logic = (logic_gate_network_t*)brain->logic;
    int result = intuition_attach_logic_gates(intuition, logic);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to logic gates (formal validation)\n");
    }
}

/**
 * @brief Connect intuition system to semantic memory
 */
static void connect_intuition_to_semantic_memory(intuition_system_t* intuition, brain_t brain) {
    if (!brain->semantic_memory) {
        return;
    }

    semantic_memory_t* semantic = (semantic_memory_t*)brain->semantic_memory;
    int result = intuition_attach_semantic_memory(intuition, semantic);
    if (result == 0) {
        fprintf(stderr, "[INTUITION] Connected to semantic memory\n");
    }
}

/**
 * @brief Initialize intuition system for brain
 *
 * Creates and connects the Phase 6 intuition integration system to all
 * available brain subsystems.
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_intuition_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_intuition_subsystem: brain is NULL");

            return false;
    }

    /* Check if intuition is disabled or parietal is disabled */
    if (!brain->config.enable_intuition_system && !brain->config.enable_parietal) {
        brain->intuition_system = NULL;
        brain->intuition_system_enabled = false;
        brain->last_intuition_update_us = 0;
        return true;  /* Success - intuition disabled by config */
    }

    /* If parietal is enabled, default to enabling intuition as well */
    if (!brain->config.enable_intuition_system && brain->config.enable_parietal) {
        /* Intuition is implicitly enabled with parietal */
    } else if (!brain->config.enable_intuition_system) {
        brain->intuition_system = NULL;
        brain->intuition_system_enabled = false;
        brain->last_intuition_update_us = 0;
        return true;
    }

    fprintf(stderr, "[INTUITION] Initializing Phase 6 intuition system...\n");

    /* Get default configuration */
    intuition_system_config_t config = intuition_system_default_config();

    /* Enable engines based on brain config */
    config.enable_training_integration = brain->enable_training_integration;
    config.enable_memory_integration = (brain->working_memory != NULL);
    config.enable_attention_integration = true;
    config.enable_emotion_integration = (brain->emotional_system != NULL);
    config.enable_logic_validation = (brain->logic != NULL);

    /* Create intuition system */
    intuition_system_t* intuition = intuition_system_create_custom(&config);
    if (!intuition) {
        fprintf(stderr, "[INTUITION] ERROR: Failed to create intuition system\n");
        brain->intuition_system = NULL;
        brain->intuition_system_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_intuition_subsystem: intuition is NULL");
        return false;
    }

    /* Store in brain */
    brain->intuition_system = intuition;
    brain->intuition_system_enabled = true;
    brain->last_intuition_update_us = 0;

    fprintf(stderr, "[INTUITION] Intuition system created, connecting to subsystems...\n");

    /* ====================================================================== */
    /* CONNECT TO ALL AVAILABLE SUBSYSTEMS                                    */
    /* ====================================================================== */

    /* 1. Working Memory - Active hunch manipulation */
    connect_intuition_to_working_memory(intuition, brain);

    /* 2. Training - Learn from successful/failed intuitions */
    connect_intuition_to_training(intuition, brain);

    /* 3. Attention - Focus allocation */
    connect_intuition_to_attention(intuition, brain);

    /* 4. Executive Functions - Strategy guidance */
    connect_intuition_to_executive(intuition, brain);

    /* 5. Emotion System - Gut feelings */
    connect_intuition_to_emotion(intuition, brain);

    /* 6. Logic Gates - Formal validation */
    connect_intuition_to_logic(intuition, brain);

    /* 7. Semantic Memory - Conceptual knowledge */
    connect_intuition_to_semantic_memory(intuition, brain);

    fprintf(stderr, "[INTUITION] Phase 6 intuition system initialization complete\n");
    fprintf(stderr, "[INTUITION]   Engines enabled: intuitive, analogical, insight,\n");
    fprintf(stderr, "[INTUITION]                    hypothesis, blending, counterfactual, meta\n");

    return true;
}

/**
 * @brief Get intuition system from brain
 *
 * @param brain Brain instance
 * @return Intuition system handle or NULL if not enabled
 */
intuition_system_t* brain_get_intuition_system(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_get_intuition_system: brain is NULL");
        return NULL;
    }
    /* Feature disabled is a normal state — return NULL without throwing */
    if (!brain->intuition_system_enabled) {
        return NULL;
    }
    return brain->intuition_system;
}

/**
 * @brief Update intuition system biological state
 *
 * Syncs inflammation and fatigue levels with intuition system.
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_intuition_biological_state(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_update_intuition_biological_state: brain is NULL");
        return -1;
    }
    /* Feature disabled is a normal state — return success without throwing */
    if (!brain->intuition_system_enabled || !brain->intuition_system) {
        return 0;
    }

    /* Get inflammation from immune system if available */
    float inflammation = 0.0f;
    if (brain->immune_enabled && brain->immune_system) {
        /* Would query immune system for current inflammation level */
        /* For now, use a default value */
        inflammation = 0.1f;
    }

    /* Get fatigue from sleep system */
    float fatigue = 0.0f;
    if (brain->medulla_enabled) {
        /* Would query medulla for arousal → inverse to fatigue */
        fatigue = 0.1f;
    }

    intuition_system_set_inflammation(brain->intuition_system, inflammation);
    intuition_system_set_fatigue(brain->intuition_system, fatigue);

    return 0;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy parietal cortex subsystem
 *
 * WHAT: Clean up all parietal resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_parietal_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_parietal_subsystem: brain is NULL"); return; }

    /* Destroy intuition system */
    if (brain->intuition_system) {
        intuition_system_destroy(brain->intuition_system);
        brain->intuition_system = NULL;
    }

    /* Destroy quantum bridge first (depends on parietal) */
    if (brain->parietal_cortex_quantum_bridge) {
        parietal_region_quantum_bridge_destroy(brain->parietal_cortex_quantum_bridge);
        brain->parietal_cortex_quantum_bridge = NULL;
    }

    /* Thalamic and substrate bridges are stubbed out (always NULL) - no destroy needed */
    brain->parietal_cortex_thalamic_bridge = NULL;
    brain->parietal_cortex_substrate_bridge = NULL;

    /* Destroy parietal adapter */
    if (brain->parietal) {
        parietal_destroy(brain->parietal);
        brain->parietal = NULL;
    }

    brain->parietal_enabled = false;
    brain->last_parietal_update_us = 0;
}

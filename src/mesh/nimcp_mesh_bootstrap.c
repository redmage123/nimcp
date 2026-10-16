/**
 * @file nimcp_mesh_bootstrap.c
 * @brief Complete NIMCP System Mesh Network Bootstrap Implementation
 *
 * WHAT: Bootstraps the entire NIMCP system into a unified mesh network
 * WHY:  Single initialization point for all NIMCP modules
 * HOW:  Category-based registration with auto-discovery
 */

#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * Bootstrap Structure
 * ============================================================================ */

struct mesh_bootstrap {
    uint32_t magic;
    mesh_bootstrap_config_t config;

    /* Core mesh integration */
    mesh_integration_t* integration;

    /* Registered component handles */
    mesh_participant_id_t brain_id;
    mesh_participant_id_t thalamus_id;
    mesh_participant_id_t bbb_id;
    mesh_participant_id_t immune_id;
    mesh_participant_id_t amygdala_id;
    mesh_participant_id_t hippocampus_id;
    mesh_participant_id_t fep_id;
    mesh_participant_id_t bio_router_id;
    mesh_participant_id_t gpu_recovery_id;
    mesh_participant_id_t plasticity_id;
    mesh_participant_id_t gossip_id;
    mesh_participant_id_t swarm_id;

    /* Category tracking arrays */
    mesh_participant_id_t* cognitive_ids;
    size_t cognitive_count;

    mesh_participant_id_t* sensory_ids;
    size_t sensory_count;

    mesh_participant_id_t* motor_ids;
    size_t motor_count;

    mesh_participant_id_t* memory_ids;
    size_t memory_count;

    mesh_participant_id_t* security_ids;
    size_t security_count;

    mesh_participant_id_t* gpu_ids;
    size_t gpu_count;

    mesh_participant_id_t* plasticity_ids;
    size_t plasticity_count;

    mesh_participant_id_t* glial_ids;
    size_t glial_count;

    mesh_participant_id_t* swarm_ids;
    size_t swarm_count;

    mesh_participant_id_t* async_ids;
    size_t async_count;

    mesh_participant_id_t* lnn_ids;
    size_t lnn_count;

    mesh_participant_id_t* snn_ids;
    size_t snn_count;

    /* Statistics */
    mesh_bootstrap_stats_t stats;

    /* Pattern-based routing (brain-like self-selection) */
    mesh_pattern_router_t* pattern_router;

    /* Phase 14: Bridges for full bidirectional integration */
    mesh_module_registry_t* module_registry;    /**< Type-safe module lookup */
    mesh_bio_bridge_t* bio_bridge;              /**< Bio-async ↔ mesh translation */
    mesh_exception_bridge_t* exception_bridge;  /**< Exception → immune routing */
    mesh_health_bridge_t* health_bridge;        /**< Heartbeat aggregation */

    /* State */
    bool initialized;
    bool receptive_fields_initialized;
    uint64_t init_start_ns;

    nimcp_mutex_t* mutex;
};

#define MESH_BOOTSTRAP_MAGIC 0x424F4F54  /* "BOOT" */
#define MAX_CATEGORY_MODULES 256

/* ============================================================================
 * Private: Module Registration Helpers
 * ============================================================================ */

/**
 * @brief Generic module registration with category assignment
 */
static mesh_participant_id_t register_generic_module(
    mesh_bootstrap_t* bootstrap,
    void* module,
    const char* name,
    mesh_adapter_category_t category,
    endorser_role_t role,
    const char** policies,
    size_t policy_count
) {
    if (!bootstrap || !module || !name) return 0;

    mesh_adapter_base_t* base = nimcp_calloc(1, sizeof(mesh_adapter_base_t));
    if (!base) return 0;

    mesh_adapter_config_t config;
    mesh_adapter_config_init(&config, name, category);
    config.endorser_role = role;
    config.policies = policies;
    config.policy_count = policy_count;

    if (mesh_adapter_base_init(base, module, &config, NULL) != NIMCP_SUCCESS) {
        nimcp_free(base);
        return 0;
    }

    if (mesh_integration_register_adapter(bootstrap->integration, base) != NIMCP_SUCCESS) {
        mesh_adapter_base_cleanup(base);
        nimcp_free(base);
        return 0;
    }

    if (bootstrap->config.verbose_logging) {
        LOG_DEBUG("Registered module '%s' (ID=0x%llx) category=%d",
                  name, (unsigned long long)base->participant_id, category);
    }

    return base->participant_id;
}

/**
 * @brief Add participant ID to category tracking array
 */
static void track_category_id(
    mesh_participant_id_t** ids,
    size_t* count,
    mesh_participant_id_t id
) {
    if (!ids || !count || id == 0) return;

    if (*ids == NULL) {
        *ids = nimcp_calloc(MAX_CATEGORY_MODULES, sizeof(mesh_participant_id_t));
        if (!*ids) return;
    }

    if (*count < MAX_CATEGORY_MODULES) {
        (*ids)[(*count)++] = id;
    }
}

/* ============================================================================
 * Private: Cognitive Module Registration
 * ============================================================================ */

/* Cognitive module names for registration */
static const char* cognitive_module_names[] = {
    "fep_orchestrator",
    "attention_manager",
    "working_memory",
    "reasoning_engine",
    "planning_module",
    "inner_dialogue",
    "metacognition",
    "executive_function",
    "decision_making",
    "problem_solving",
    "abstract_reasoning",
    "analogical_reasoning",
    "causal_reasoning",
    "temporal_reasoning",
    "spatial_reasoning",
    "social_cognition",
    "theory_of_mind",
    "emotional_reasoning",
    "moral_reasoning",
    "creative_thinking",
    "divergent_thinking",
    "convergent_thinking",
    "critical_thinking",
    "systems_thinking",
    "mental_simulation",
    "counterfactual_reasoning",
    "prospective_memory",
    "cognitive_control",
    "inhibitory_control",
    "task_switching",
    "cognitive_flexibility",
    "goal_maintenance",
    "error_monitoring",
    "conflict_detection",
    "response_selection",
    "action_monitoring",
    NULL
};

static size_t register_cognitive_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    /* FEP is REQUIRED for cognitive policy */
    static const char* fep_policies[] = { MESH_POLICY_COGNITIVE };
    mesh_participant_id_t fep_id = register_generic_module(
        bootstrap, (void*)(uintptr_t)0x1001, "fep_orchestrator",
        MESH_ADAPTER_CATEGORY_COGNITIVE, ENDORSER_ROLE_REQUIRED,
        fep_policies, 1
    );
    if (fep_id) {
        track_category_id(&bootstrap->cognitive_ids, &bootstrap->cognitive_count, fep_id);
        count++;
    }

    /* Register remaining cognitive modules as OPTIONAL */
    for (int i = 1; cognitive_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x1000 + i + 1),
            cognitive_module_names[i],
            MESH_ADAPTER_CATEGORY_COGNITIVE, ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->cognitive_ids, &bootstrap->cognitive_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu cognitive modules", count);
    return count;
}

/* ============================================================================
 * Private: Sensory Module Registration
 * ============================================================================ */

static const char* sensory_module_names[] = {
    "visual_cortex",
    "auditory_cortex",
    "somatosensory_cortex",
    "olfactory_cortex",
    "gustatory_cortex",
    "vestibular_system",
    "proprioception",
    "interoception",
    "nociception",
    "thermoreception",
    "mechanoreception",
    "visual_attention",
    "auditory_attention",
    "cross_modal_integration",
    "multisensory_binding",
    "feature_detection",
    "edge_detection",
    "motion_detection",
    "color_processing",
    "depth_perception",
    "object_recognition",
    "face_recognition",
    "scene_recognition",
    "pattern_recognition",
    "speech_recognition",
    "sound_localization",
    "pitch_perception",
    "rhythm_perception",
    "texture_perception",
    "haptic_perception",
    NULL
};

static size_t register_sensory_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    /* Visual, auditory, somatosensory are OPTIONAL for sensory_fusion policy */
    static const char* sensory_policies[] = { MESH_POLICY_SENSORY_FUSION };

    for (int i = 0; sensory_module_names[i]; i++) {
        /* First 3 (visual, auditory, somatosensory) are in sensory policy */
        const char** policies = (i < 3) ? sensory_policies : NULL;
        size_t policy_count = (i < 3) ? 1 : 0;

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x2000 + i + 1),
            sensory_module_names[i],
            MESH_ADAPTER_CATEGORY_PERCEPTION,
            (i < 3) ? ENDORSER_ROLE_PREFERRED : ENDORSER_ROLE_OPTIONAL,
            policies, policy_count
        );
        if (id) {
            track_category_id(&bootstrap->sensory_ids, &bootstrap->sensory_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu sensory modules", count);
    return count;
}

/* ============================================================================
 * Private: Motor Module Registration
 * ============================================================================ */

static const char* motor_module_names[] = {
    "motor_cortex",
    "premotor_cortex",
    "supplementary_motor",
    "cerebellum",
    "basal_ganglia",
    "motor_planning",
    "motor_execution",
    "motor_learning",
    "motor_adaptation",
    "motor_sequence",
    "reach_planning",
    "grasp_planning",
    "locomotion",
    "posture_control",
    "balance_control",
    "eye_movement",
    "saccade_control",
    "smooth_pursuit",
    "vestibulo_ocular",
    "head_movement",
    "speech_motor",
    "facial_motor",
    "fine_motor",
    "gross_motor",
    "bimanual_coordination",
    NULL
};

static size_t register_motor_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    static const char* motor_policies[] = { MESH_POLICY_MOTOR_COMMAND };

    for (int i = 0; motor_module_names[i]; i++) {
        /* motor_cortex and cerebellum are REQUIRED for motor policy */
        bool is_required = (i == 0 || i == 3);  /* motor_cortex, cerebellum */
        bool is_motor_policy = (i < 5);  /* First 5 are in motor policy */

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x3000 + i + 1),
            motor_module_names[i],
            MESH_ADAPTER_CATEGORY_MOTOR,
            is_required ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL,
            is_motor_policy ? motor_policies : NULL,
            is_motor_policy ? 1 : 0
        );
        if (id) {
            track_category_id(&bootstrap->motor_ids, &bootstrap->motor_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu motor modules", count);
    return count;
}

/* ============================================================================
 * Private: Memory Module Registration
 * ============================================================================ */

static const char* memory_module_names[] = {
    "hippocampus",
    "working_memory_buffer",
    "episodic_memory",
    "semantic_memory",
    "procedural_memory",
    "autobiographical_memory",
    "prospective_memory",
    "memory_encoding",
    "memory_consolidation",
    "memory_retrieval",
    "memory_reconsolidation",
    "pattern_separation",
    "pattern_completion",
    "memory_indexing",
    "memory_binding",
    "context_memory",
    "source_memory",
    "familiarity_detection",
    "recollection",
    "forgetting_module",
    "interference_control",
    "memory_search",
    "cue_processing",
    "memory_monitoring",
    NULL
};

static size_t register_memory_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    static const char* memory_policies[] = { MESH_POLICY_MEMORY_STORE };

    for (int i = 0; memory_module_names[i]; i++) {
        /* hippocampus is REQUIRED for memory policy */
        bool is_required = (i == 0);  /* hippocampus */
        bool is_memory_policy = (i < 10);  /* First 10 are in memory policy */

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x4000 + i + 1),
            memory_module_names[i],
            MESH_ADAPTER_CATEGORY_MEMORY,
            is_required ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL,
            is_memory_policy ? memory_policies : NULL,
            is_memory_policy ? 1 : 0
        );
        if (id) {
            track_category_id(&bootstrap->memory_ids, &bootstrap->memory_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu memory modules", count);
    return count;
}

/* ============================================================================
 * Private: Security Module Registration
 * ============================================================================ */

static const char* security_module_names[] = {
    "blood_brain_barrier",
    "brain_immune_system",
    "microglia_surveillance",
    "threat_detection",
    "antigen_processing",
    "antibody_generation",
    "inflammation_control",
    "quarantine_manager",
    "immune_memory",
    "cytokine_signaling",
    "complement_system",
    "phagocytosis",
    "apoptosis_control",
    "oxidative_stress",
    "neuroprotection",
    "repair_mechanisms",
    "barrier_integrity",
    "transport_control",
    "efflux_pumps",
    "tight_junctions",
    NULL
};

static size_t register_security_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    static const char* security_policies[] = { MESH_POLICY_SECURITY };

    for (int i = 0; security_module_names[i]; i++) {
        /* BBB and immune are REQUIRED for security policy */
        bool is_required = (i < 2);  /* bbb, immune */

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x5000 + i + 1),
            security_module_names[i],
            MESH_ADAPTER_CATEGORY_SECURITY,
            is_required ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL,
            security_policies, 1
        );
        if (id) {
            track_category_id(&bootstrap->security_ids, &bootstrap->security_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu security modules", count);
    return count;
}

/* ============================================================================
 * Private: GPU Module Registration
 * ============================================================================ */

static const char* gpu_module_names[] = {
    "gpu_recovery",
    "gpu_batch_processor",
    "gpu_memory_manager",
    "multi_gpu_coordinator",
    "cuda_kernel_manager",
    "tensor_operations",
    "matrix_operations",
    "convolution_engine",
    "attention_kernels",
    "activation_kernels",
    "gradient_kernels",
    "loss_kernels",
    "optimizer_kernels",
    "financial_gpu",
    "monte_carlo_gpu",
    "risk_gpu",
    "derivatives_gpu",
    "optimization_gpu",
    "statistics_gpu",
    "fuzzy_gpu",
    NULL
};

static size_t register_gpu_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    static const char* gpu_policies[] = { MESH_POLICY_GPU_BATCH };

    for (int i = 0; gpu_module_names[i]; i++) {
        /* gpu_recovery and batch_processor are REQUIRED */
        bool is_required = (i < 2);

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x6000 + i + 1),
            gpu_module_names[i],
            MESH_ADAPTER_CATEGORY_GPU,
            is_required ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL,
            gpu_policies, 1
        );
        if (id) {
            track_category_id(&bootstrap->gpu_ids, &bootstrap->gpu_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu GPU modules", count);
    return count;
}

/* ============================================================================
 * Private: Plasticity Module Registration
 * ============================================================================ */

static const char* plasticity_module_names[] = {
    "plasticity_coordinator",
    "stdp_module",
    "ltp_module",
    "ltd_module",
    "homeostatic_plasticity",
    "metaplasticity",
    "structural_plasticity",
    "synaptic_scaling",
    "synaptic_tagging",
    "protein_synthesis",
    "spine_dynamics",
    "dendritic_plasticity",
    "axonal_plasticity",
    "neuromodulation",
    "dopamine_modulation",
    "serotonin_modulation",
    "norepinephrine_modulation",
    "acetylcholine_modulation",
    "reward_learning",
    "error_driven_learning",
    "hebbian_learning",
    "anti_hebbian",
    "competitive_learning",
    "contrastive_learning",
    NULL
};

static size_t register_plasticity_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    static const char* plasticity_policies[] = { MESH_POLICY_PLASTICITY };

    for (int i = 0; plasticity_module_names[i]; i++) {
        /* plasticity_coordinator is REQUIRED */
        bool is_required = (i == 0);

        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x7000 + i + 1),
            plasticity_module_names[i],
            MESH_ADAPTER_CATEGORY_PLASTICITY,
            is_required ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL,
            plasticity_policies, 1
        );
        if (id) {
            track_category_id(&bootstrap->plasticity_ids, &bootstrap->plasticity_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu plasticity modules", count);
    return count;
}

/* ============================================================================
 * Private: Glial Module Registration
 * ============================================================================ */

static const char* glial_module_names[] = {
    "astrocyte_network",
    "microglia_system",
    "oligodendrocyte_network",
    "schwann_cells",
    "ependymal_cells",
    "radial_glia",
    "metabolic_support",
    "lactate_shuttle",
    "glutamate_uptake",
    "potassium_buffering",
    "water_homeostasis",
    "waste_clearance",
    "glymphatic_system",
    "blood_flow_regulation",
    "neurovascular_coupling",
    NULL
};

static size_t register_glial_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    for (int i = 0; glial_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x8000 + i + 1),
            glial_module_names[i],
            MESH_ADAPTER_CATEGORY_GLIAL,
            ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->glial_ids, &bootstrap->glial_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu glial modules", count);
    return count;
}

/* ============================================================================
 * Private: Swarm Module Registration
 * ============================================================================ */

static const char* swarm_module_names[] = {
    "gossip_beliefs",
    "swarm_consensus",
    "collective_workspace",
    "fep_convergence",
    "distributed_coordination",
    "belief_propagation",
    "consensus_voting",
    "leader_election",
    "membership_protocol",
    "failure_detection",
    "view_synchronization",
    "state_machine_replication",
    "log_replication",
    "snapshot_coordination",
    "recovery_protocol",
    NULL
};

static size_t register_swarm_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    for (int i = 0; swarm_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0x9000 + i + 1),
            swarm_module_names[i],
            MESH_ADAPTER_CATEGORY_SWARM,
            ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->swarm_ids, &bootstrap->swarm_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu swarm modules", count);
    return count;
}

/* ============================================================================
 * Private: Async Module Registration
 * ============================================================================ */

static const char* async_module_names[] = {
    "bio_router",
    "bio_scheduler",
    "bio_promise",
    "bio_future",
    "dopamine_channel",
    "serotonin_channel",
    "norepinephrine_channel",
    "acetylcholine_channel",
    "gaba_channel",
    "glutamate_channel",
    "phase_synchronization",
    "oscillator_network",
    "theta_rhythm",
    "gamma_rhythm",
    "alpha_rhythm",
    "beta_rhythm",
    "delta_rhythm",
    "cross_frequency_coupling",
    NULL
};

static size_t register_async_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    for (int i = 0; async_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0xA000 + i + 1),
            async_module_names[i],
            MESH_ADAPTER_CATEGORY_SYSTEM,
            ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->async_ids, &bootstrap->async_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu async modules", count);
    return count;
}

/* ============================================================================
 * Private: LNN Module Registration
 * ============================================================================ */

static const char* lnn_module_names[] = {
    "lnn_cluster",
    "lnn_layer",
    "lnn_neuron",
    "lnn_synapse",
    "lnn_ode_solver",
    "lnn_continuous_time",
    "lnn_time_constant",
    "lnn_input_modulation",
    "lnn_output_modulation",
    "lnn_recurrent",
    "lnn_feedforward",
    "lnn_lateral",
    "lnn_bridge",
    "lnn_ensemble",
    "lnn_mixer",
    "lnn_attention",
    "lnn_memory",
    NULL
};

static size_t register_lnn_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    for (int i = 0; lnn_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0xB000 + i + 1),
            lnn_module_names[i],
            MESH_ADAPTER_CATEGORY_COGNITIVE,  /* LNN is cognitive */
            ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->lnn_ids, &bootstrap->lnn_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu LNN modules", count);
    return count;
}

/* ============================================================================
 * Private: SNN Module Registration
 * ============================================================================ */

static const char* snn_module_names[] = {
    "snn_network",
    "snn_layer",
    "snn_neuron",
    "snn_synapse",
    "spike_encoder",
    "spike_decoder",
    "spike_timing",
    "spike_propagation",
    "stdp_learning",
    "rate_coding",
    "temporal_coding",
    "population_coding",
    "sparse_coding",
    "predictive_coding",
    "reservoir_computing",
    "liquid_state_machine",
    "echo_state_network",
    NULL
};

static size_t register_snn_category(mesh_bootstrap_t* bootstrap) {
    size_t count = 0;

    for (int i = 0; snn_module_names[i]; i++) {
        mesh_participant_id_t id = register_generic_module(
            bootstrap, (void*)(uintptr_t)(0xC000 + i + 1),
            snn_module_names[i],
            MESH_ADAPTER_CATEGORY_COGNITIVE,  /* SNN is cognitive */
            ENDORSER_ROLE_OPTIONAL,
            NULL, 0
        );
        if (id) {
            track_category_id(&bootstrap->snn_ids, &bootstrap->snn_count, id);
            count++;
        }
    }

    LOG_INFO("Registered %zu SNN modules", count);
    return count;
}

/* ============================================================================
 * Private: Core Component Registration
 * ============================================================================ */

static void register_core_components(mesh_bootstrap_t* bootstrap) {
    mesh_integration_t* integration = bootstrap->integration;

    /* Register thalamus as central GATEWAY */
    bootstrap->thalamus_id = mesh_integration_register_thalamus(
        integration, (void*)(uintptr_t)0x100
    );

    /* Register amygdala with VETO role */
    bootstrap->amygdala_id = mesh_integration_register_amygdala(
        integration, (void*)(uintptr_t)0x101
    );

    /* Register hippocampus as REQUIRED for memory */
    bootstrap->hippocampus_id = mesh_integration_register_hippocampus(
        integration, (void*)(uintptr_t)0x102
    );

    /* Register motor cortex */
    bootstrap->motor_ids = nimcp_calloc(MAX_CATEGORY_MODULES, sizeof(mesh_participant_id_t));
    mesh_participant_id_t motor_id = mesh_integration_register_motor_cortex(
        integration, (void*)(uintptr_t)0x103
    );
    if (motor_id && bootstrap->motor_ids) {
        bootstrap->motor_ids[bootstrap->motor_count++] = motor_id;
    }

    /* Register cerebellum */
    mesh_participant_id_t cerebellum_id = mesh_integration_register_cerebellum(
        integration, (void*)(uintptr_t)0x104
    );
    if (cerebellum_id && bootstrap->motor_ids) {
        bootstrap->motor_ids[bootstrap->motor_count++] = cerebellum_id;
    }

    /* Register basal ganglia */
    mesh_participant_id_t bg_id = mesh_integration_register_basal_ganglia(
        integration, (void*)(uintptr_t)0x105
    );
    if (bg_id && bootstrap->motor_ids) {
        bootstrap->motor_ids[bootstrap->motor_count++] = bg_id;
    }

    /* Register PFC (left and right) */
    mesh_participant_id_t pfc_left = mesh_integration_register_prefrontal_cortex(
        integration, (void*)(uintptr_t)0x106, true
    );
    mesh_participant_id_t pfc_right = mesh_integration_register_prefrontal_cortex(
        integration, (void*)(uintptr_t)0x107, false
    );

    if (pfc_left) {
        track_category_id(&bootstrap->cognitive_ids, &bootstrap->cognitive_count, pfc_left);
    }
    if (pfc_right) {
        track_category_id(&bootstrap->cognitive_ids, &bootstrap->cognitive_count, pfc_right);
    }

    LOG_INFO("Registered core brain components");
}

/* ============================================================================
 * Public API: Configuration
 * ============================================================================ */

nimcp_error_t mesh_bootstrap_default_config(mesh_bootstrap_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    /* Default integration config */
    mesh_integration_default_config(&config->integration);

    /* Enable all standard subsystems */
    config->subsystems = MESH_SUBSYSTEMS_ALL;

    /* Auto-discovery enabled */
    config->auto_discover_modules = true;
    config->auto_register_all = true;

    /* Health monitoring */
    config->enable_health_monitoring = true;
    config->health_check_interval_ms = 1000.0f;

    /* Logging */
    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Public API: Lifecycle
 * ============================================================================ */

mesh_bootstrap_t* mesh_bootstrap_create(const mesh_bootstrap_config_t* config) {
    mesh_bootstrap_config_t default_config;
    if (!config) {
        mesh_bootstrap_default_config(&default_config);
        config = &default_config;
    }

    mesh_bootstrap_t* bootstrap = nimcp_calloc(1, sizeof(*bootstrap));
    if (!bootstrap) {
        LOG_ERROR("Failed to allocate mesh bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bootstrap_create: bootstrap is NULL");
        return NULL;
    }

    bootstrap->magic = MESH_BOOTSTRAP_MAGIC;
    bootstrap->config = *config;
    bootstrap->init_start_ns = nimcp_time_now_ns();

    /* Create mutex */
    mutex_attr_t attr = {0};
    bootstrap->mutex = nimcp_mutex_create(&attr);
    if (!bootstrap->mutex) {
        LOG_ERROR("Failed to create bootstrap mutex");
        nimcp_free(bootstrap);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bootstrap_create: bootstrap->mutex is NULL");
        return NULL;
    }

    /* Create core mesh integration */
    bootstrap->integration = mesh_integration_create(&config->integration);
    if (!bootstrap->integration) {
        LOG_ERROR("Failed to create mesh integration");
        mesh_bootstrap_destroy(bootstrap);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_bootstrap_create: bootstrap->integration is NULL");
        return NULL;
    }

    /* Create pattern router for brain-like self-selection */
    mesh_pattern_router_config_t router_config = {
        .default_threshold = MESH_DEFAULT_ACTIVATION_THRESHOLD,
        .competition_strength = 0.5f,
        .enable_learning = true,
        .learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .max_endorsers = 16
    };
    bootstrap->pattern_router = mesh_pattern_router_create(&router_config);
    if (!bootstrap->pattern_router) {
        LOG_WARN("Failed to create pattern router - pattern-based routing disabled");
        /* Non-fatal: system still works with enum-based routing */
    } else {
        LOG_DEBUG("Pattern router created for brain-like self-selection");
    }

    /* Phase 14: Initialize module registry for type-safe lookup */
    mesh_module_registry_config_t registry_config;
    mesh_module_registry_default_config(&registry_config);
    registry_config.require_magic_validation = true;
    registry_config.verbose_logging = config->verbose_logging;
    bootstrap->module_registry = mesh_module_registry_create(&registry_config);
    if (!bootstrap->module_registry) {
        LOG_WARN("Failed to create module registry - type-safe lookup disabled");
    } else {
        LOG_DEBUG("Module registry created for type-safe module lookup");

        /* Auto-register bootstrap internal modules into the module registry.
         * These use _bootstrap_ prefix to avoid conflicts with user-registered
         * modules, and dummy instances with magic=0 to skip magic validation. */
        struct { const char* name; mesh_adapter_category_t cat; } core_modules[] = {
            { "_bootstrap_cortex",    MESH_ADAPTER_CATEGORY_MOTOR },
            { "_bootstrap_cerebel",   MESH_ADAPTER_CATEGORY_MOTOR },
            { "_bootstrap_basal",     MESH_ADAPTER_CATEGORY_MOTOR },
            { "_bootstrap_hipp",      MESH_ADAPTER_CATEGORY_MEMORY },
            { "_bootstrap_prosp",     MESH_ADAPTER_CATEGORY_MEMORY },
        };
        size_t num_core = sizeof(core_modules) / sizeof(core_modules[0]);
        for (size_t cm = 0; cm < num_core; cm++) {
            mesh_module_descriptor_t desc;
            memset(&desc, 0, sizeof(desc));
            desc.module_name = core_modules[cm].name;
            desc.category = core_modules[cm].cat;
            desc.module_instance = (void*)(uintptr_t)(0xB000 + cm);
            desc.module_magic = 0;  /* Skip magic validation */
            mesh_module_registry_register(bootstrap->module_registry, &desc);
        }
    }

    /* Phase 14: Initialize bio bridge for bio-async integration */
    mesh_bio_bridge_config_t bio_config;
    mesh_bio_bridge_default_config(&bio_config);
    bio_config.enable_pattern_routing = (bootstrap->pattern_router != NULL);
    bio_config.verbose_logging = config->verbose_logging;
    bootstrap->bio_bridge = mesh_bio_bridge_create(bootstrap, &bio_config);
    if (!bootstrap->bio_bridge) {
        LOG_WARN("Failed to create bio bridge - bio-async integration disabled");
    } else {
        LOG_DEBUG("Bio bridge created for bio-async ↔ mesh translation");
    }

    /* Phase 14: Initialize health bridge for distributed monitoring */
    mesh_health_bridge_config_t health_config;
    mesh_health_bridge_default_config(&health_config);
    health_config.verbose_logging = config->verbose_logging;
    bootstrap->health_bridge = mesh_health_bridge_create(bootstrap, &health_config);
    if (!bootstrap->health_bridge) {
        LOG_WARN("Failed to create health bridge - distributed health disabled");
    } else {
        LOG_DEBUG("Health bridge created for heartbeat aggregation");
    }

    /* Phase 14: Exception bridge requires immune system - defer if not available */
    /* Will be initialized when immune system is registered */
    bootstrap->exception_bridge = NULL;

    /* Phase 14: Initialize receptive fields library */
    if (mesh_receptive_fields_init() == NIMCP_SUCCESS) {
        bootstrap->receptive_fields_initialized = true;
        LOG_DEBUG("Receptive fields library initialized");
    } else {
        bootstrap->receptive_fields_initialized = false;
        LOG_WARN("Failed to initialize receptive fields library");
    }

    LOG_INFO("Created mesh bootstrap - registering subsystems...");

    /* Register core brain components first */
    register_core_components(bootstrap);

    /* Register subsystem categories based on flags */
    mesh_subsystem_flags_t flags = config->subsystems;

    if (flags.enable_cognitive) {
        bootstrap->stats.cognitive_modules = register_cognitive_category(bootstrap);
    }

    if (flags.enable_sensory) {
        bootstrap->stats.sensory_modules = register_sensory_category(bootstrap);
    }

    if (flags.enable_motor) {
        bootstrap->stats.motor_modules += register_motor_category(bootstrap);
    }

    if (flags.enable_memory) {
        bootstrap->stats.memory_modules = register_memory_category(bootstrap);
    }

    if (flags.enable_security) {
        bootstrap->stats.security_modules = register_security_category(bootstrap);
    }

    if (flags.enable_gpu) {
        bootstrap->stats.gpu_modules = register_gpu_category(bootstrap);
    }

    if (flags.enable_plasticity) {
        bootstrap->stats.plasticity_modules = register_plasticity_category(bootstrap);
    }

    if (flags.enable_glial) {
        bootstrap->stats.glial_modules = register_glial_category(bootstrap);
    }

    if (flags.enable_swarm) {
        bootstrap->stats.swarm_modules = register_swarm_category(bootstrap);
    }

    if (flags.enable_async) {
        bootstrap->stats.async_modules = register_async_category(bootstrap);
    }

    if (flags.enable_lnn) {
        bootstrap->stats.lnn_modules = register_lnn_category(bootstrap);
    }

    if (flags.enable_snn) {
        bootstrap->stats.snn_modules = register_snn_category(bootstrap);
    }

    /* Calculate totals */
    bootstrap->stats.total_modules_registered =
        bootstrap->stats.cognitive_modules +
        bootstrap->stats.sensory_modules +
        bootstrap->stats.motor_modules +
        bootstrap->stats.memory_modules +
        bootstrap->stats.security_modules +
        bootstrap->stats.gpu_modules +
        bootstrap->stats.plasticity_modules +
        bootstrap->stats.glial_modules +
        bootstrap->stats.swarm_modules +
        bootstrap->stats.async_modules +
        bootstrap->stats.lnn_modules +
        bootstrap->stats.snn_modules;

    bootstrap->stats.total_channels_active = MESH_NUM_STANDARD_CHANNELS;
    bootstrap->stats.initialization_time_ns = nimcp_time_now_ns() - bootstrap->init_start_ns;
    bootstrap->stats.fully_initialized = true;
    bootstrap->initialized = true;

    LOG_INFO("Mesh bootstrap complete: %zu modules in %llu ms",
             bootstrap->stats.total_modules_registered,
             (unsigned long long)(bootstrap->stats.initialization_time_ns / 1000000));

    return bootstrap;
}

void mesh_bootstrap_destroy(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return;

    nimcp_mutex_lock(bootstrap->mutex);

    /* Free category tracking arrays */
    nimcp_free(bootstrap->cognitive_ids);
    nimcp_free(bootstrap->sensory_ids);
    nimcp_free(bootstrap->motor_ids);
    nimcp_free(bootstrap->memory_ids);
    nimcp_free(bootstrap->security_ids);
    nimcp_free(bootstrap->gpu_ids);
    nimcp_free(bootstrap->plasticity_ids);
    nimcp_free(bootstrap->glial_ids);
    nimcp_free(bootstrap->swarm_ids);
    nimcp_free(bootstrap->async_ids);
    nimcp_free(bootstrap->lnn_ids);
    nimcp_free(bootstrap->snn_ids);

    /* Destroy pattern router */
    if (bootstrap->pattern_router) {
        mesh_pattern_router_destroy(bootstrap->pattern_router);
        bootstrap->pattern_router = NULL;
    }

    /* Phase 14: Destroy bridges */
    if (bootstrap->exception_bridge) {
        mesh_exception_bridge_destroy(bootstrap->exception_bridge);
        bootstrap->exception_bridge = NULL;
    }

    if (bootstrap->health_bridge) {
        mesh_health_bridge_destroy(bootstrap->health_bridge);
        bootstrap->health_bridge = NULL;
    }

    if (bootstrap->bio_bridge) {
        mesh_bio_bridge_destroy(bootstrap->bio_bridge);
        bootstrap->bio_bridge = NULL;
    }

    if (bootstrap->module_registry) {
        mesh_module_registry_destroy(bootstrap->module_registry);
        bootstrap->module_registry = NULL;
    }

    /* Phase 14: Cleanup receptive fields library */
    if (bootstrap->receptive_fields_initialized) {
        mesh_receptive_fields_cleanup();
    }

    /* Destroy integration */
    if (bootstrap->integration) {
        mesh_integration_destroy(bootstrap->integration);
    }

    nimcp_mutex_unlock(bootstrap->mutex);
    nimcp_mutex_destroy(bootstrap->mutex);

    bootstrap->magic = 0;
    nimcp_free(bootstrap);

    LOG_INFO("Mesh bootstrap destroyed");
}

/* ============================================================================
 * Public API: Component Access
 * ============================================================================ */

mesh_integration_t* mesh_bootstrap_get_integration(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_integration: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->integration;
}

mesh_participant_registry_t* mesh_bootstrap_get_registry(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_registry: bootstrap is NULL");
        return NULL;
    }
    return mesh_integration_get_registry(bootstrap->integration);
}

mesh_channel_t* mesh_bootstrap_get_channel(
    mesh_bootstrap_t* bootstrap,
    mesh_channel_id_t channel_id
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_channel: bootstrap is NULL");
        return NULL;
    }
    return mesh_integration_get_channel(bootstrap->integration, channel_id);
}

/* ============================================================================
 * Public API: Category Registration
 * ============================================================================ */

size_t mesh_bootstrap_register_cognitive_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_cognitive_category(bootstrap);
    bootstrap->stats.cognitive_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_sensory_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_sensory_category(bootstrap);
    bootstrap->stats.sensory_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_motor_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_motor_category(bootstrap);
    bootstrap->stats.motor_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_memory_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_memory_category(bootstrap);
    bootstrap->stats.memory_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_security_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_security_category(bootstrap);
    bootstrap->stats.security_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_gpu_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_gpu_category(bootstrap);
    bootstrap->stats.gpu_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_plasticity_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_plasticity_category(bootstrap);
    bootstrap->stats.plasticity_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_glial_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_glial_category(bootstrap);
    bootstrap->stats.glial_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_swarm_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_swarm_category(bootstrap);
    bootstrap->stats.swarm_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_async_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_async_category(bootstrap);
    bootstrap->stats.async_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_lnn_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_lnn_category(bootstrap);
    bootstrap->stats.lnn_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

size_t mesh_bootstrap_register_snn_modules(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    size_t count = register_snn_category(bootstrap);
    bootstrap->stats.snn_modules += count;
    bootstrap->stats.total_modules_registered += count;
    return count;
}

/* ============================================================================
 * Public API: Update and Processing
 * ============================================================================ */

nimcp_error_t mesh_bootstrap_update(
    mesh_bootstrap_t* bootstrap,
    uint64_t delta_ms
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    return mesh_integration_update(bootstrap->integration, delta_ms);
}

size_t mesh_bootstrap_process_transactions(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 0;
    return mesh_integration_process_transactions(bootstrap->integration);
}

nimcp_error_t mesh_bootstrap_gossip_all(
    mesh_bootstrap_t* bootstrap,
    size_t rounds
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (int ch = 0; ch < MESH_NUM_STANDARD_CHANNELS; ch++) {
        mesh_channel_t* channel = mesh_integration_get_channel(
            bootstrap->integration, (mesh_channel_id_t)ch
        );
        if (channel) {
            for (size_t r = 0; r < rounds; r++) {
                mesh_channel_gossip_round(channel);
            }
        }
    }

    return NIMCP_SUCCESS;
}

bool mesh_bootstrap_has_converged(const mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        return false;
    }
    return mesh_integration_has_converged(bootstrap->integration);
}

float mesh_bootstrap_get_free_energy(const mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) return 1.0f;
    return mesh_integration_get_free_energy(bootstrap->integration);
}

/* ============================================================================
 * Public API: Statistics
 * ============================================================================ */

nimcp_error_t mesh_bootstrap_get_stats(
    const mesh_bootstrap_t* bootstrap,
    mesh_bootstrap_stats_t* stats
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    *stats = bootstrap->stats;
    return NIMCP_SUCCESS;
}

void mesh_bootstrap_print_status(const mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        printf("Mesh Bootstrap: NULL or invalid\n");
        return;
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║               NIMCP MESH NETWORK BOOTSTRAP STATUS                ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  Category              │ Modules Registered                      ║\n");
    printf("╠────────────────────────┼─────────────────────────────────────────╣\n");
    printf("║  Cognitive             │ %6zu                                   ║\n", bootstrap->stats.cognitive_modules);
    printf("║  Sensory               │ %6zu                                   ║\n", bootstrap->stats.sensory_modules);
    printf("║  Motor                 │ %6zu                                   ║\n", bootstrap->stats.motor_modules);
    printf("║  Memory                │ %6zu                                   ║\n", bootstrap->stats.memory_modules);
    printf("║  Security              │ %6zu                                   ║\n", bootstrap->stats.security_modules);
    printf("║  GPU                   │ %6zu                                   ║\n", bootstrap->stats.gpu_modules);
    printf("║  Plasticity            │ %6zu                                   ║\n", bootstrap->stats.plasticity_modules);
    printf("║  Glial                 │ %6zu                                   ║\n", bootstrap->stats.glial_modules);
    printf("║  Swarm                 │ %6zu                                   ║\n", bootstrap->stats.swarm_modules);
    printf("║  Async                 │ %6zu                                   ║\n", bootstrap->stats.async_modules);
    printf("║  LNN                   │ %6zu                                   ║\n", bootstrap->stats.lnn_modules);
    printf("║  SNN                   │ %6zu                                   ║\n", bootstrap->stats.snn_modules);
    printf("╠────────────────────────┼─────────────────────────────────────────╣\n");
    printf("║  TOTAL MODULES         │ %6zu                                   ║\n", bootstrap->stats.total_modules_registered);
    printf("║  Active Channels       │ %6zu                                   ║\n", bootstrap->stats.total_channels_active);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  Initialization Time   │ %6llu ms                               ║\n",
           (unsigned long long)(bootstrap->stats.initialization_time_ns / 1000000));
    printf("║  System Free Energy    │ %6.4f                                  ║\n",
           mesh_bootstrap_get_free_energy(bootstrap));
    printf("║  Converged             │ %6s                                   ║\n",
           mesh_bootstrap_has_converged(bootstrap) ? "YES" : "NO");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  PHASE 14 BRIDGES                                                ║\n");
    printf("╠────────────────────────┼─────────────────────────────────────────╣\n");
    printf("║  Module Registry       │ %6s                                   ║\n",
           bootstrap->module_registry ? "ACTIVE" : "OFF");
    printf("║  Bio Bridge            │ %6s                                   ║\n",
           bootstrap->bio_bridge ? "ACTIVE" : "OFF");
    printf("║  Exception Bridge      │ %6s                                   ║\n",
           bootstrap->exception_bridge ? "ACTIVE" : "OFF");
    printf("║  Health Bridge         │ %6s                                   ║\n",
           bootstrap->health_bridge ? "ACTIVE" : "OFF");
    printf("║  Receptive Fields      │ %6s                                   ║\n",
           bootstrap->receptive_fields_initialized ? "INIT" : "OFF");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* ============================================================================
 * Public API: Manual Registration
 * ============================================================================ */

nimcp_error_t mesh_bootstrap_register_brain(
    mesh_bootstrap_t* bootstrap,
    hemispheric_brain_t* brain
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!brain) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->brain_id = register_generic_module(
        bootstrap, brain, "hemispheric_brain",
        MESH_ADAPTER_CATEGORY_SYSTEM, ENDORSER_ROLE_REQUIRED,
        NULL, 0
    );

    return bootstrap->brain_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_thalamus(
    mesh_bootstrap_t* bootstrap,
    thalamus_t* thalamus
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!thalamus) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->thalamus_id = mesh_integration_register_thalamus(
        bootstrap->integration, thalamus
    );

    return bootstrap->thalamus_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_bbb(
    mesh_bootstrap_t* bootstrap,
    blood_brain_barrier_t* bbb
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!bbb) return NIMCP_ERROR_NULL_POINTER;

    static const char* bbb_policies[] = { MESH_POLICY_SECURITY };
    bootstrap->bbb_id = register_generic_module(
        bootstrap, bbb, "blood_brain_barrier",
        MESH_ADAPTER_CATEGORY_SECURITY, ENDORSER_ROLE_REQUIRED,
        bbb_policies, 1
    );

    return bootstrap->bbb_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_immune(
    mesh_bootstrap_t* bootstrap,
    brain_immune_system_t* immune
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!immune) return NIMCP_ERROR_NULL_POINTER;

    static const char* immune_policies[] = { MESH_POLICY_SECURITY };
    bootstrap->immune_id = register_generic_module(
        bootstrap, immune, "brain_immune_system",
        MESH_ADAPTER_CATEGORY_SECURITY, ENDORSER_ROLE_REQUIRED,
        immune_policies, 1
    );

    /* Phase 14: Now that we have immune system, initialize exception bridge */
    if (bootstrap->immune_id && !bootstrap->exception_bridge) {
        mesh_exception_bridge_config_t exc_config;
        mesh_exception_bridge_default_config(&exc_config);
        exc_config.verbose_logging = bootstrap->config.verbose_logging;
        bootstrap->exception_bridge = mesh_exception_bridge_create(
            bootstrap, immune, &exc_config);
        if (bootstrap->exception_bridge) {
            LOG_DEBUG("Exception bridge created with immune system integration");
        } else {
            LOG_WARN("Failed to create exception bridge");
        }
    }

    return bootstrap->immune_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_amygdala(
    mesh_bootstrap_t* bootstrap,
    amygdala_t* amygdala
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!amygdala) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->amygdala_id = mesh_integration_register_amygdala(
        bootstrap->integration, amygdala
    );

    return bootstrap->amygdala_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_hippocampus(
    mesh_bootstrap_t* bootstrap,
    hippocampus_t* hippocampus
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!hippocampus) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->hippocampus_id = mesh_integration_register_hippocampus(
        bootstrap->integration, hippocampus
    );

    return bootstrap->hippocampus_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_fep(
    mesh_bootstrap_t* bootstrap,
    fep_orchestrator_t* fep
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!fep) return NIMCP_ERROR_NULL_POINTER;

    static const char* fep_policies[] = { MESH_POLICY_COGNITIVE };
    bootstrap->fep_id = register_generic_module(
        bootstrap, fep, "fep_orchestrator",
        MESH_ADAPTER_CATEGORY_COGNITIVE, ENDORSER_ROLE_REQUIRED,
        fep_policies, 1
    );

    return bootstrap->fep_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_bio_router(
    mesh_bootstrap_t* bootstrap,
    bio_router_t* router
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!router) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->bio_router_id = register_generic_module(
        bootstrap, router, "bio_router",
        MESH_ADAPTER_CATEGORY_SYSTEM, ENDORSER_ROLE_OPTIONAL,
        NULL, 0
    );

    return bootstrap->bio_router_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_gpu_recovery(
    mesh_bootstrap_t* bootstrap,
    gpu_recovery_context_t* gpu_recovery
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!gpu_recovery) return NIMCP_ERROR_NULL_POINTER;

    static const char* gpu_policies[] = { MESH_POLICY_GPU_BATCH };
    bootstrap->gpu_recovery_id = register_generic_module(
        bootstrap, gpu_recovery, "gpu_recovery",
        MESH_ADAPTER_CATEGORY_GPU, ENDORSER_ROLE_REQUIRED,
        gpu_policies, 1
    );

    return bootstrap->gpu_recovery_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_plasticity(
    mesh_bootstrap_t* bootstrap,
    plasticity_coordinator_t* plasticity
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!plasticity) return NIMCP_ERROR_NULL_POINTER;

    static const char* plasticity_policies[] = { MESH_POLICY_PLASTICITY };
    bootstrap->plasticity_id = register_generic_module(
        bootstrap, plasticity, "plasticity_coordinator",
        MESH_ADAPTER_CATEGORY_PLASTICITY, ENDORSER_ROLE_REQUIRED,
        plasticity_policies, 1
    );

    return bootstrap->plasticity_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_gossip(
    mesh_bootstrap_t* bootstrap,
    gossip_beliefs_context_t* gossip
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!gossip) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->gossip_id = register_generic_module(
        bootstrap, gossip, "gossip_beliefs",
        MESH_ADAPTER_CATEGORY_SWARM, ENDORSER_ROLE_OPTIONAL,
        NULL, 0
    );

    return bootstrap->gossip_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

nimcp_error_t mesh_bootstrap_register_swarm(
    mesh_bootstrap_t* bootstrap,
    swarm_consensus_t* swarm
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!swarm) return NIMCP_ERROR_NULL_POINTER;

    bootstrap->swarm_id = register_generic_module(
        bootstrap, swarm, "swarm_consensus",
        MESH_ADAPTER_CATEGORY_SWARM, ENDORSER_ROLE_OPTIONAL,
        NULL, 0
    );

    return bootstrap->swarm_id ? NIMCP_SUCCESS : NIMCP_ERROR_SYSTEM;
}

/* ============================================================================
 * Public API: Pattern-Based Routing
 * ============================================================================ */

mesh_pattern_router_t* mesh_bootstrap_get_pattern_router(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_pattern_router: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->pattern_router;
}

nimcp_error_t mesh_bootstrap_register_receptive_field(
    mesh_bootstrap_t* bootstrap,
    mesh_participant_id_t module_id,
    const mesh_receptive_field_t* field
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bootstrap->pattern_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bootstrap: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    return mesh_pattern_router_register_receptive_field(
        bootstrap->pattern_router, module_id, field);
}

nimcp_error_t mesh_bootstrap_route_by_pattern(
    mesh_bootstrap_t* bootstrap,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsers || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bootstrap->pattern_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bootstrap: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    return mesh_pattern_router_get_endorsers(
        bootstrap->pattern_router, tx, endorsers, max_endorsers, count_out);
}

nimcp_error_t mesh_bootstrap_apply_neuromodulation(
    mesh_bootstrap_t* bootstrap,
    mesh_neuromodulator_t neuromod,
    float level
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!bootstrap->pattern_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bootstrap: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    return mesh_pattern_router_apply_neuromodulation(
        bootstrap->pattern_router, neuromod, level);
}

nimcp_error_t mesh_bootstrap_learn_routing_outcome(
    mesh_bootstrap_t* bootstrap,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward_signal
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bootstrap->pattern_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bootstrap: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    return mesh_pattern_router_learn_outcome(
        bootstrap->pattern_router, tx, endorsers, endorser_count,
        success, reward_signal);
}

/* ============================================================================
 * Public API: Phase 14 Bridge Access
 * ============================================================================ */

mesh_module_registry_t* mesh_bootstrap_get_module_registry(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_module_registry: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->module_registry;
}

mesh_bio_bridge_t* mesh_bootstrap_get_bio_bridge(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_bio_bridge: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->bio_bridge;
}

mesh_exception_bridge_t* mesh_bootstrap_get_exception_bridge(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_exception_bridge: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->exception_bridge;
}

mesh_health_bridge_t* mesh_bootstrap_get_health_bridge(mesh_bootstrap_t* bootstrap) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_get_health_bridge: bootstrap is NULL");
        return NULL;
    }
    return bootstrap->health_bridge;
}

nimcp_error_t mesh_bootstrap_register_module(
    mesh_bootstrap_t* bootstrap,
    const mesh_module_descriptor_t* descriptor
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_bootstrap: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!descriptor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bootstrap->module_registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_bootstrap: not initialized");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /* Register in the type-safe module registry */
    nimcp_error_t err = mesh_module_registry_register(
        bootstrap->module_registry, descriptor);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to register module '%s' in registry: %d",
                  descriptor->module_name, err);
        return err;
    }

    /* Also register as mesh participant via legacy path */
    mesh_participant_id_t id = register_generic_module(
        bootstrap,
        descriptor->module_instance,
        descriptor->module_name,
        descriptor->category,
        ENDORSER_ROLE_OPTIONAL,
        NULL, 0
    );

    if (id == 0) {
        LOG_WARN("Module '%s' registered in registry but not as participant",
                 descriptor->module_name);
    }

    /* Register receptive field if provided */
    if (descriptor->receptive_field && bootstrap->pattern_router && id != 0) {
        mesh_pattern_router_register_receptive_field(
            bootstrap->pattern_router, id, descriptor->receptive_field);
    }

    /* Register with health bridge if health agent provided */
    if (descriptor->health_agent && bootstrap->health_bridge && id != 0) {
        mesh_health_bridge_register_agent(
            bootstrap->health_bridge, id, descriptor->health_agent);
    }

    if (bootstrap->config.verbose_logging) {
        LOG_DEBUG("Registered module '%s' (category=%d) with Phase 14 bridges",
                  descriptor->module_name, descriptor->category);
    }

    return NIMCP_SUCCESS;
}

mesh_registered_module_t* mesh_bootstrap_lookup_module(
    mesh_bootstrap_t* bootstrap,
    const char* name
) {
    if (!bootstrap || bootstrap->magic != MESH_BOOTSTRAP_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_lookup_module: bootstrap is NULL");
        return NULL;
    }
    if (!name || !bootstrap->module_registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_bootstrap_lookup_module: required parameter is NULL (name, bootstrap->module_registry)");
        return NULL;
    }

    /* mesh_module_registry_get returns const, but we return non-const for API compatibility */
    return (mesh_registered_module_t*)mesh_module_registry_get(bootstrap->module_registry, name);
}

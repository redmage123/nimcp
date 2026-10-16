//=============================================================================
// nimcp_brain_init_security.c - Security Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_security.c
 * @brief Security subsystem initialization for brain
 *
 * WHAT: Security monitoring, integration, and BBB protection initialization
 * WHY:  Separates security initialization from other subsystems
 * HOW:  Initializes security recovery bridge, security integration (SC-4), and BBB (IS-1)
 *
 * EXTRACTED FROM: nimcp_brain_init.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_security.h"
#include "core/brain/factory/init/nimcp_brain_init_safety_verify.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/factory/init/nimcp_brain_init_validation.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security_recovery_bridge.h"
#include "security/nimcp_security_integration.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

/* LGSS types are forward-declared in brain_internal.h */

#define LOG_MODULE "BRAIN_INIT_SECURITY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_security, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// TO COMPLETE THIS FILE:
// Run: sed -n '3822,4094p' ../nimcp_brain_init.c >> nimcp_brain_init_security.c
//
// This will extract lines 3822-4094 from the original file which contains
// the init_security_subsystem function.
//=============================================================================
bool nimcp_brain_factory_init_security_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_security_subsystem: brain is NULL");
        return false;
    }

    // Initialize security fields to defaults
    brain->security_bridge = NULL;
    brain->enable_security_monitoring = false;
    brain->security_check_interval_ms = 0;
    brain->last_security_check_ms = 0;

    // Check if security monitoring is enabled in config
    // Default to false to avoid performance impact unless explicitly requested
    if (!brain->config.enable_security_monitoring) {
        LOG_DEBUG("Security monitoring disabled in config");
        return true;  // Success - security disabled
    }

    // Create the security recovery bridge
    nimcp_security_recovery_bridge_t* bridge = nimcp_srb_create();
    if (!bridge) {
        LOG_WARNING("Failed to create security recovery bridge - continuing without security monitoring");
        return true;  // Non-fatal - continue without security
    }

    // Create security recovery bridge configuration
    nimcp_srb_config_t srb_config = {
        .mode = NIMCP_SRB_MODE_AUTO_REPAIR,
        .enable_auto_checkpoint = true,
        .checkpoint_interval_ms = 60000,  // 1 minute
        .enable_fractal_verification = true,
        .verification_interval_ms = brain->config.security_check_interval_ms,
        .cooldown_ms = 1000,
        .max_repairs_per_minute = 60,
        .notify_brain = true,
        .log_to_audit = true
    };

    // Initialize the bridge with configuration
    if (nimcp_srb_init(bridge, &srb_config) != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to initialize security recovery bridge - continuing without security monitoring");
        nimcp_srb_destroy(bridge);
        return true;  // Non-fatal
    }

    // Register this brain with the bridge
    if (nimcp_srb_register_brain(bridge, brain) != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to register brain with security bridge");
        nimcp_srb_destroy(bridge);
        return true;  // Non-fatal
    }

    // Register brain's critical memory regions for protection
    uint32_t registered_regions = nimcp_srb_register_brain_regions(bridge, brain);
    LOG_DEBUG("Registered %u critical memory regions for security monitoring", registered_regions);

    // Store bridge in brain structure
    brain->security_bridge = bridge;
    brain->enable_security_monitoring = true;
    brain->security_check_interval_ms = brain->config.security_check_interval_ms;
    brain->last_security_check_ms = 0;

    LOG_INFO("Security subsystem initialized: mode=%s, fractal=%s, checkpoint=%s",
             srb_config.mode == NIMCP_SRB_MODE_AUTO_REPAIR ? "auto_repair" : "monitor",
             srb_config.enable_fractal_verification ? "enabled" : "disabled",
             srb_config.enable_auto_checkpoint ? "enabled" : "disabled");

    // === PHASE SC-4: UNIVERSAL SECURITY INTEGRATION ===
    // Initialize the global security integration framework if enabled
    // This provides entropy monitoring, trust management, and differential privacy

    // Initialize SC-4 fields to defaults
    brain->security_integration = NULL;
    brain->sec_module_id = 0;
    brain->sec_region_ids = NULL;
    brain->num_sec_regions = 0;
    brain->enable_security_integration = false;

    if (brain->config.enable_security_integration) {
        // Create security integration context
        nimcp_sec_integration_t* sec_ctx = nimcp_sec_integration_create();
        if (sec_ctx) {
            // Configure the security integration
            nimcp_sec_integration_config_t sec_config = nimcp_sec_integration_default_config();
            sec_config.trust_threshold = brain->config.security_trust_threshold > 0.0 ?
                                        brain->config.security_trust_threshold : 0.5;
            sec_config.privacy_budget = brain->config.security_privacy_budget > 0.0 ?
                                       brain->config.security_privacy_budget : 10.0;
            sec_config.enable_continuous_monitoring = false;  // Don't start monitoring thread
            sec_config.enable_self_monitoring = true;
            sec_config.enable_event_logging = true;

            if (nimcp_sec_integration_init(sec_ctx, &sec_config) == NIMCP_SUCCESS) {
                // Register brain as a CORE module
                uint32_t module_id;
                if (nimcp_sec_register_module(sec_ctx, brain->config.task_name,
                                             NIMCP_SEC_CAT_CORE, &module_id) == NIMCP_SUCCESS) {
                    brain->security_integration = sec_ctx;
                    brain->sec_module_id = module_id;
                    brain->enable_security_integration = true;

                    // === Register All Cognitive Subsystems ===
                    uint32_t subsys_id;
                    (void)subsys_id;  // Suppress unused warning if some modules not enabled

                    // COGNITIVE MODULES
                    if (brain->global_workspace) {
                        nimcp_sec_register_module(sec_ctx, "global_workspace", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->ethics) {
                        nimcp_sec_register_module(sec_ctx, "ethics_engine", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->knowledge) {
                        nimcp_sec_register_module(sec_ctx, "knowledge_system", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->introspection) {
                        nimcp_sec_register_module(sec_ctx, "introspection", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->curiosity) {
                        nimcp_sec_register_module(sec_ctx, "curiosity_engine", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->salience) {
                        nimcp_sec_register_module(sec_ctx, "salience_evaluator", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->consolidation) {
                        nimcp_sec_register_module(sec_ctx, "consolidation", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->working_memory) {
                        nimcp_sec_register_module(sec_ctx, "working_memory", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->executive) {
                        nimcp_sec_register_module(sec_ctx, "executive_controller", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->theory_of_mind) {
                        nimcp_sec_register_module(sec_ctx, "theory_of_mind", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->emotional_system) {
                        nimcp_sec_register_module(sec_ctx, "emotional_system", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->mirror_neurons) {
                        nimcp_sec_register_module(sec_ctx, "mirror_neurons", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->mental_health_monitor) {
                        nimcp_sec_register_module(sec_ctx, "mental_health_monitor", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->autobio) {
                        nimcp_sec_register_module(sec_ctx, "autobiographical_memory", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->self_model) {
                        nimcp_sec_register_module(sec_ctx, "self_model", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->meta_learner) {
                        nimcp_sec_register_module(sec_ctx, "meta_learner", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->predictive_network) {
                        nimcp_sec_register_module(sec_ctx, "predictive_network", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->symbolic_logic) {
                        nimcp_sec_register_module(sec_ctx, "symbolic_logic", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->epistemic) {
                        nimcp_sec_register_module(sec_ctx, "epistemic_filter", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    if (brain->personality) {
                        nimcp_sec_register_module(sec_ctx, "personality", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    }
                    // empathy_network is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "empathy_network", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);
                    // sleep_system is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "sleep_system", NIMCP_SEC_CAT_COGNITIVE, &subsys_id);

                    // GLIAL MODULES
                    if (brain->glial) {
                        nimcp_sec_register_module(sec_ctx, "glial_integration", NIMCP_SEC_CAT_GLIAL, &subsys_id);
                    }

                    // PLASTICITY MODULES
                    // neuromodulator_system is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "neuromodulators", NIMCP_SEC_CAT_PLASTICITY, &subsys_id);
                    // multihead_attention is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "attention", NIMCP_SEC_CAT_PLASTICITY, &subsys_id);

                    // CORE MODULES
                    if (brain->network) {
                        nimcp_sec_register_module(sec_ctx, "neural_network", NIMCP_SEC_CAT_CORE, &subsys_id);
                    }
                    if (brain->brain_regions) {
                        nimcp_sec_register_module(sec_ctx, "brain_regions", NIMCP_SEC_CAT_CORE, &subsys_id);
                    }
                    if (brain->oscillations) {
                        nimcp_sec_register_module(sec_ctx, "oscillations", NIMCP_SEC_CAT_CORE, &subsys_id);
                    }
                    // multimodal is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "multimodal", NIMCP_SEC_CAT_CORE, &subsys_id);
                    if (brain->cortical_column_pool) {
                        nimcp_sec_register_module(sec_ctx, "cortical_columns", NIMCP_SEC_CAT_CORE, &subsys_id);
                    }

                    // NETWORKING MODULES
                    // distributed is not a pointer - check if non-NULL context
                    if (brain->distributed) {
                        nimcp_sec_register_module(sec_ctx, "distributed_cognition", NIMCP_SEC_CAT_NETWORKING, &subsys_id);
                    }

                    // MIDDLEWARE MODULES
                    if (brain->middleware_controller) {
                        nimcp_sec_register_module(sec_ctx, "middleware_controller", NIMCP_SEC_CAT_MIDDLEWARE, &subsys_id);
                    }

                    // I/O & PERCEPTION MODULES
                    if (brain->visual_cortex) {
                        nimcp_sec_register_module(sec_ctx, "visual_cortex", NIMCP_SEC_CAT_IO, &subsys_id);
                    }
                    if (brain->audio_cortex) {
                        nimcp_sec_register_module(sec_ctx, "audio_cortex", NIMCP_SEC_CAT_IO, &subsys_id);
                    }
                    if (brain->speech_cortex) {
                        nimcp_sec_register_module(sec_ctx, "speech_cortex", NIMCP_SEC_CAT_IO, &subsys_id);
                    }
                    // nlp_network is not a pointer - always registered
                    nimcp_sec_register_module(sec_ctx, "nlp", NIMCP_SEC_CAT_IO, &subsys_id);

                    LOG_INFO("Security integration (SC-4) initialized: module_id=%u, trust_threshold=%.2f, privacy_budget=%.1f",
                            module_id, sec_config.trust_threshold, sec_config.privacy_budget);
                } else {
                    LOG_WARNING("Failed to register brain with security integration");
                    nimcp_sec_integration_destroy(sec_ctx);
                }
            } else {
                LOG_WARNING("Failed to initialize security integration context");
                nimcp_sec_integration_destroy(sec_ctx);
            }
        } else {
            LOG_WARNING("Failed to create security integration context - continuing without SC-4");
        }
    }

    // === PHASE IS-1: BLOOD-BRAIN BARRIER (BBB) PERIMETER DEFENSE ===
    // Initialize BBB fields to defaults
    brain->bbb_system = NULL;
    brain->bbb_memory_region_id = 0;
    brain->bbb_subject_id = 0;
    brain->bbb_enabled = false;

    if (brain->config.enable_bbb_protection) {
        // Get or create the global BBB system
        bbb_system_t bbb = get_global_bbb_system();
        if (bbb) {
            // Register this brain's memory region with the BBB
            // The brain struct itself is the primary protected region
            uint32_t region_id = bbb_register_memory_region(bbb, brain, sizeof(*brain), false);
            if (region_id > 0) {
                brain->bbb_system = bbb;  // Store reference for cleanup
                brain->bbb_memory_region_id = region_id;
                brain->bbb_enabled = true;

                LOG_INFO("BBB protection enabled for brain '%s': region_id=%u",
                        brain->config.task_name, region_id);
            } else {
                LOG_WARNING("Failed to register brain memory region with BBB");
                nimcp_bbb_release_global_system();  // Release our reference on failure
            }
        } else {
            LOG_WARNING("Failed to get global BBB system - continuing without BBB protection");
        }
    } else {
        LOG_DEBUG("BBB protection disabled in config");
    }

    return true;
}

//=============================================================================
// Brain Immune System Initialization
//=============================================================================

/**
 * @brief Initialize brain immune system subsystem
 *
 * WHAT: Creates adaptive defense coordination layer
 * WHY:  Unified threat response across BBB, BFT, and swarm systems
 * HOW:  Creates immune system and auto-connects to available security modules
 */
bool nimcp_brain_factory_init_immune_subsystem(brain_t brain)
{
    // Guard clause: Validate brain pointer
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_immune_subsystem: brain is NULL");
        return false;
    }

    // Initialize immune fields to defaults
    brain->immune_system = NULL;
    brain->immune_enabled = false;

    // Check if immune system is enabled in config
    if (!brain->config.enable_brain_immune) {
        LOG_DEBUG("Brain immune system disabled in config");
        return true;  // Success - immune disabled
    }

    // Build immune system configuration from brain config
    brain_immune_config_t immune_config = {0};
    brain_immune_default_config(&immune_config);

    // Override defaults with brain config values
    immune_config.max_antigens = brain->config.immune_max_antigens > 0 ?
        brain->config.immune_max_antigens : BRAIN_IMMUNE_MAX_ANTIGENS;
    immune_config.max_b_cells = brain->config.immune_max_b_cells > 0 ?
        brain->config.immune_max_b_cells : BRAIN_IMMUNE_MAX_B_CELLS;
    immune_config.max_t_cells = brain->config.immune_max_t_cells > 0 ?
        brain->config.immune_max_t_cells : BRAIN_IMMUNE_MAX_T_CELLS;
    immune_config.max_antibodies = brain->config.immune_max_antibodies > 0 ?
        brain->config.immune_max_antibodies : BRAIN_IMMUNE_MAX_ANTIBODIES;

    // Thresholds
    immune_config.recognition_threshold = brain->config.immune_recognition_threshold > 0.0f ?
        brain->config.immune_recognition_threshold : 0.7f;
    immune_config.activation_threshold = brain->config.immune_activation_threshold > 0.0f ?
        brain->config.immune_activation_threshold : 0.6f;
    immune_config.inflammation_threshold = brain->config.immune_inflammation_threshold > 0.0f ?
        brain->config.immune_inflammation_threshold : 0.8f;
    immune_config.memory_response_multiplier = brain->config.immune_memory_response_multiplier > 0.0f ?
        brain->config.immune_memory_response_multiplier : 2.0f;

    // Timing
    immune_config.activation_delay_ms = brain->config.immune_activation_delay_ms > 0 ?
        brain->config.immune_activation_delay_ms : 10;
    immune_config.memory_formation_delay_ms = brain->config.immune_memory_formation_delay_ms > 0 ?
        brain->config.immune_memory_formation_delay_ms : 100;
    immune_config.antibody_half_life_ms = brain->config.immune_antibody_half_life_ms > 0 ?
        brain->config.immune_antibody_half_life_ms : 60000;

    // Integration enables
    immune_config.enable_bbb_integration = brain->config.immune_enable_bbb_integration && brain->bbb_enabled;
    immune_config.enable_bft_integration = brain->config.immune_enable_bft_integration;
    immune_config.enable_swarm_integration = brain->config.immune_enable_swarm_integration;
    immune_config.enable_bio_async = brain->config.immune_enable_bio_async && brain->bio_async_enabled;
    immune_config.enable_logging = true;

    // Create immune system
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    if (!immune) {
        LOG_WARNING("Failed to create brain immune system - continuing without immune coordination");
        return true;  // Non-fatal - continue without immune system
    }

    /* W11: wire brain back-reference so immune events emit to KG. */
    brain_immune_set_brain_ref(immune, brain);

    // Auto-connect to BBB if enabled
    if (immune_config.enable_bbb_integration && brain->bbb_system) {
        if (brain_immune_connect_bbb(immune, brain->bbb_system) == 0) {
            LOG_DEBUG("Brain immune connected to BBB for threat presentation");
        } else {
            LOG_WARNING("Failed to connect immune system to BBB");
        }
    }

    // Auto-connect to bio-async for cytokine signaling if enabled
    if (immune_config.enable_bio_async && brain->bio_async_enabled) {
        if (brain_immune_connect_bio_async(immune) == 0) {
            LOG_DEBUG("Brain immune connected to bio-async for cytokine signaling");
        } else {
            LOG_WARNING("Failed to connect immune system to bio-async");
        }
    }

    // Note: BFT and swarm immune connections are handled in distributed cognition module
    // when the brain is integrated into a swarm or distributed system

    // Start immune system monitoring
    if (brain_immune_start(immune) != 0) {
        LOG_WARNING("Failed to start immune system monitoring");
        brain_immune_destroy(immune);
        return true;  // Non-fatal
    }

    // Store immune system in brain structure
    brain->immune_system = immune;
    brain->immune_enabled = true;

    // V1 Security Fix: Wire BBB → Immune bidirectional connection
    // brain_immune_connect_bbb() (above) connects immune→BBB for threat presentation.
    // bbb_connect_immune() connects BBB→immune so BBB can report detected threats
    // back to the immune system for adaptive response and memory formation.
    if (brain->bbb_system && brain->bbb_enabled) {
        if (bbb_connect_immune(brain->bbb_system, immune)) {
            LOG_DEBUG("BBB→Immune bidirectional link established");
        } else {
            LOG_WARNING("Failed to connect BBB→Immune (non-fatal, unidirectional only)");
        }
    }

    LOG_INFO("Brain immune system initialized: antigens=%u, b_cells=%u, t_cells=%u, antibodies=%u, "
             "bbb=%s, bft=%s, swarm=%s, bio_async=%s",
             immune_config.max_antigens,
             immune_config.max_b_cells,
             immune_config.max_t_cells,
             immune_config.max_antibodies,
             immune_config.enable_bbb_integration ? "enabled" : "disabled",
             immune_config.enable_bft_integration ? "enabled" : "disabled",
             immune_config.enable_swarm_integration ? "enabled" : "disabled",
             immune_config.enable_bio_async ? "enabled" : "disabled");

    return true;
}

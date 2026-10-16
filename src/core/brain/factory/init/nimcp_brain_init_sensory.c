//=============================================================================
// nimcp_brain_init_sensory.c - Phase 6 Sensory Module Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_sensory.c
 * @brief Brain factory initialization for Phase 6 sensory modules
 *
 * WHAT: Initialization functions for BR-9/10/11 sensory modules
 * WHY:  Integrate somatosensory, olfactory, and gustatory processing
 * HOW:  Creates modules, connects available bridges
 *
 * MODULES INITIALIZED:
 * - BR-9 Somatosensory: Touch, proprioception, temperature, pain
 * - BR-10 Olfactory: Smell processing (piriform cortex)
 * - BR-11 Gustatory: Taste processing (insular cortex)
 *
 * NOTE: Include order is critical to avoid receptor_type_t conflict between
 *       somatosensory and neuromodulator headers.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

//=============================================================================
// Includes - Order matters to avoid type conflicts!
//=============================================================================

// Include sensory module headers FIRST before brain includes
// This avoids receptor_type_t conflict with neuromodulators
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

// Now include brain headers (these will use the already-defined receptor_type_t)
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_SENSORY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_sensory, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Somatosensory Cortex Initialization (BR-9)
//=============================================================================

/**
 * @brief Initialize Somatosensory Cortex subsystem
 *
 * WHAT: Creates the somatosensory cortex (S1/S2) for touch processing
 * WHY:  Process tactile, proprioceptive, thermal, and pain information
 * HOW:  Create module, configure body map, connect to available bridges
 *
 * BIOLOGICAL BASIS:
 * - Area 3a: Proprioception from muscle spindles
 * - Area 3b: Fine touch discrimination (fingertips have highest density)
 * - Area 1: Texture processing
 * - Area 2: Size and shape integration
 * - S2: Bilateral integration and complex tactile processing
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_somatosensory_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_somatosensory_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->somatosensory = NULL;
    brain->somatosensory_substrate_bridge = NULL;
    brain->somatosensory_thalamic_bridge = NULL;
    brain->somatosensory_enabled = false;
    brain->last_somatosensory_update_us = 0;

    /* Check if sensory processing should be enabled */
    /* Use multimodal_integration or visual/audio cortex as proxy for perception being enabled */
    bool should_enable = brain->config.enable_multimodal_integration ||
                         brain->config.enable_visual_cortex ||
                         brain->config.enable_audio_cortex;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Somatosensory subsystem skipped (perception not enabled)");
        return true;
    }

    /* Create default configuration */
    soma_config_t config = soma_default_config();

    /* Create somatosensory module */
    nimcp_somatosensory_t* soma = soma_create(&config);
    if (!soma) {
        NIMCP_LOGGING_WARN("Failed to create somatosensory module - "
                          "continuing without touch processing");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->somatosensory = (struct nimcp_somatosensory_s*)soma;
    brain->somatosensory_enabled = true;

    /* Connect to hypothalamus for temperature regulation */
    if (brain->hypothalamus_enabled && brain->hypothalamus) {
        if (soma_init_hypothalamus_bridge(soma, brain->hypothalamus) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-hypothalamus bridge connected (thermoregulation)");
        }
    }

    /* Connect to parietal cortex for spatial integration */
    if (brain->parietal_cortex_enabled && brain->parietal_cortex) {
        if (soma_init_parietal_bridge(soma, brain->parietal_cortex) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-parietal bridge connected (spatial)");
        }
    }

    /* Connect to immune system for neuroinflammation monitoring */
    if (brain->immune_enabled && brain->immune_system) {
        if (soma_init_immune_bridge(soma, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-immune bridge connected");
        }
    }

    /* Connect to PR memory for sensory consolidation */
    if (brain->pr_memory_enabled && brain->pr_z_ladder) {
        if (soma_init_prime_resonance_bridge(soma, brain->pr_z_ladder) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-PR memory bridge connected");
        }
    }

    /* Connect to dragonfly for target tracking integration */
    if (brain->dragonfly_enabled && brain->dragonfly) {
        if (soma_init_dragonfly_bridge(soma, brain->dragonfly) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-dragonfly bridge connected");
        }
    }

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled) {
        /* Bio-async bridge uses brain itself as runtime context */
        if (soma_init_bio_async_bridge(soma, brain) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory bio-async bridge connected");
        }
    }

    /* Connect to plasticity coordinator for sensory-driven learning */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        void* stdp = NULL;
        if (brain->stdp_omni_bridge_enabled && brain->stdp_omni_bridge) {
            stdp = brain->stdp_omni_bridge;
        } else if (brain->stdp_pr_bridge_enabled && brain->stdp_pr_bridge) {
            stdp = brain->stdp_pr_bridge;
        }
        if (soma_init_plasticity_bridge(soma, brain->plasticity_coordinator, stdp) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-plasticity bridge connected (sensory learning)");
        }
    }

    /* Connect to SNN (using plasticity coordinator as proxy for spiking network) */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        if (soma_init_snn_bridge(soma, brain->plasticity_coordinator) == 0) {
            NIMCP_LOGGING_DEBUG("Somatosensory-SNN bridge connected");
        }
    }

    NIMCP_LOGGING_INFO("Somatosensory subsystem initialized: "
                       "areas=5 (3a/3b/1/2/S2), body_segments=%d, "
                       "hypothalamus=%s, parietal=%s, plasticity=%s",
                       SOMA_MAX_BODY_SEGMENTS,
                       brain->hypothalamus_enabled ? "connected" : "disconnected",
                       brain->parietal_cortex_enabled ? "connected" : "disconnected",
                       brain->plasticity_coordinator_enabled ? "connected" : "disconnected");

    return true;
}

//=============================================================================
// Olfactory Cortex Initialization (BR-10)
//=============================================================================

/**
 * @brief Initialize Olfactory Cortex subsystem
 *
 * WHAT: Creates the olfactory cortex (piriform) for smell processing
 * WHY:  Process odor information with strong emotional/memory associations
 * HOW:  Create module, configure receptor patterns, connect to available bridges
 *
 * BIOLOGICAL BASIS:
 * - ~400 odorant receptor types (combinatorial coding)
 * - Olfactory bulb with glomeruli and mitral cells
 * - Piriform cortex for odor identification
 * - Direct cortical access (bypasses thalamus!)
 * - Strong amygdala connection for emotional associations
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_olfactory_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_olfactory_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->olfactory = NULL;
    brain->olfactory_substrate_bridge = NULL;
    brain->olfactory_enabled = false;
    brain->last_olfactory_update_us = 0;

    /* Check if sensory processing should be enabled */
    bool should_enable = brain->config.enable_multimodal_integration ||
                         brain->config.enable_visual_cortex ||
                         brain->config.enable_audio_cortex;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Olfactory subsystem skipped (perception not enabled)");
        return true;
    }

    /* Create default configuration */
    olfact_config_t config = olfact_default_config();

    /* Create olfactory module */
    nimcp_olfactory_t* olfact = olfact_create(&config);
    if (!olfact) {
        NIMCP_LOGGING_WARN("Failed to create olfactory module - "
                          "continuing without smell processing");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->olfactory = (struct nimcp_olfactory_s*)olfact;
    brain->olfactory_enabled = true;

    /* Note: Olfactory bypasses thalamus - direct cortical access */

    /* Connect to hypothalamus for food seeking and pheromone processing */
    if (brain->hypothalamus_enabled && brain->hypothalamus) {
        if (olfact_init_hypothalamus_bridge(olfact, brain->hypothalamus) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-hypothalamus bridge connected (appetite/pheromones)");
        }
    }

    /* Connect to immune system for olfactory inflammation monitoring */
    if (brain->immune_enabled && brain->immune_system) {
        if (olfact_init_immune_bridge(olfact, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-immune bridge connected");
        }
    }

    /* Connect to PR memory for odor memory consolidation */
    if (brain->pr_memory_enabled && brain->pr_z_ladder) {
        if (olfact_init_prime_resonance_bridge(olfact, brain->pr_z_ladder) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-PR memory bridge connected");
        }
    }

    /* Connect to OFC (executive) for odor valuation and decision making */
    if (brain->executive) {
        if (olfact_init_ofc_bridge(olfact, brain->executive) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-OFC bridge connected (valuation)");
        }
    }

    /* Connect to plasticity coordinator for odor-driven learning */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        void* stdp = NULL;
        if (brain->stdp_omni_bridge_enabled && brain->stdp_omni_bridge) {
            stdp = brain->stdp_omni_bridge;
        } else if (brain->stdp_pr_bridge_enabled && brain->stdp_pr_bridge) {
            stdp = brain->stdp_pr_bridge;
        }
        if (olfact_init_plasticity_bridge(olfact, brain->plasticity_coordinator, stdp) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-plasticity bridge connected (odor learning)");
        }
    }

    /* Connect to SNN (using plasticity coordinator as proxy for spiking network) */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        if (olfact_init_snn_bridge(olfact, brain->plasticity_coordinator) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory-SNN bridge connected");
        }
    }

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled) {
        if (olfact_init_bio_async_bridge(olfact, brain) == 0) {
            NIMCP_LOGGING_DEBUG("Olfactory bio-async bridge connected");
        }
    }

    NIMCP_LOGGING_INFO("Olfactory subsystem initialized: "
                       "receptors=%d, glomeruli=%d, piriform=%d neurons, "
                       "hypothalamus=%s, immune=%s, plasticity=%s",
                       OLFACT_MAX_RECEPTORS,
                       OLFACT_MAX_GLOMERULI,
                       OLFACT_DEFAULT_PIRIFORM,
                       brain->hypothalamus_enabled ? "connected" : "disconnected",
                       brain->immune_enabled ? "connected" : "disconnected",
                       brain->plasticity_coordinator_enabled ? "connected" : "disconnected");

    return true;
}

//=============================================================================
// Gustatory Cortex Initialization (BR-11)
//=============================================================================

/**
 * @brief Initialize Gustatory Cortex subsystem
 *
 * WHAT: Creates the gustatory cortex (insular) for taste processing
 * WHY:  Process taste information for food evaluation and reward
 * HOW:  Create module, configure taste receptors, connect to available bridges
 *
 * BIOLOGICAL BASIS:
 * - Five basic tastes: sweet, salty, sour, bitter, umami
 * - Taste buds on tongue with regional specialization
 * - NTS -> VPM thalamus -> insular cortex pathway
 * - Orbitofrontal cortex for flavor identity
 * - Strong integration with olfactory for flavor perception
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_gustatory_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_gustatory_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->gustatory = NULL;
    brain->gustatory_substrate_bridge = NULL;
    brain->gustatory_enabled = false;
    brain->last_gustatory_update_us = 0;

    /* Check if sensory processing should be enabled */
    bool should_enable = brain->config.enable_multimodal_integration ||
                         brain->config.enable_visual_cortex ||
                         brain->config.enable_audio_cortex;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Gustatory subsystem skipped (perception not enabled)");
        return true;
    }

    /* Create default configuration */
    gust_config_t config = gust_default_config();

    /* Create gustatory module */
    nimcp_gustatory_t* gust = gust_create(&config);
    if (!gust) {
        NIMCP_LOGGING_WARN("Failed to create gustatory module - "
                          "continuing without taste processing");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->gustatory = (struct nimcp_gustatory_s*)gust;
    brain->gustatory_enabled = true;

    /* Connect to olfactory for flavor perception (cross-modal integration) */
    if (brain->olfactory_enabled && brain->olfactory) {
        if (gust_init_olfactory_bridge(gust, brain->olfactory) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-olfactory bridge connected (flavor)");
        }
    }

    /* Connect to hypothalamus for appetite and satiety */
    if (brain->hypothalamus_enabled && brain->hypothalamus) {
        if (gust_init_hypothalamus_bridge(gust, brain->hypothalamus) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-hypothalamus bridge connected (appetite/satiety)");
        }
    }

    /* Connect to immune system for taste-related inflammation */
    if (brain->immune_enabled && brain->immune_system) {
        if (gust_init_immune_bridge(gust, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-immune bridge connected");
        }
    }

    /* Connect to PR memory for taste memory consolidation */
    if (brain->pr_memory_enabled && brain->pr_z_ladder) {
        if (gust_init_prime_resonance_bridge(gust, brain->pr_z_ladder) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-PR memory bridge connected");
        }
    }

    /* Connect to OFC (executive) for taste valuation and reward */
    if (brain->executive) {
        if (gust_init_ofc_bridge(gust, brain->executive) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-OFC bridge connected (reward)");
        }
    }

    /* Connect to plasticity coordinator for taste-driven learning */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        void* stdp = NULL;
        if (brain->stdp_omni_bridge_enabled && brain->stdp_omni_bridge) {
            stdp = brain->stdp_omni_bridge;
        } else if (brain->stdp_pr_bridge_enabled && brain->stdp_pr_bridge) {
            stdp = brain->stdp_pr_bridge;
        }
        if (gust_init_plasticity_bridge(gust, brain->plasticity_coordinator, stdp) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-plasticity bridge connected (taste learning)");
        }
    }

    /* Connect to SNN (using plasticity coordinator as proxy for spiking network) */
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        if (gust_init_snn_bridge(gust, brain->plasticity_coordinator) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory-SNN bridge connected");
        }
    }

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled) {
        if (gust_init_bio_async_bridge(gust, brain) == 0) {
            NIMCP_LOGGING_DEBUG("Gustatory bio-async bridge connected");
        }
    }

    NIMCP_LOGGING_INFO("Gustatory subsystem initialized: "
                       "basic_tastes=%d, insula=%d neurons, "
                       "olfactory=%s (flavor), hypothalamus=%s, plasticity=%s",
                       GUST_NUM_BASIC_TASTES,
                       GUST_DEFAULT_INSULA_NEURONS,
                       brain->olfactory_enabled ? "connected" : "disconnected",
                       brain->hypothalamus_enabled ? "connected" : "disconnected",
                       brain->plasticity_coordinator_enabled ? "connected" : "disconnected");

    return true;
}

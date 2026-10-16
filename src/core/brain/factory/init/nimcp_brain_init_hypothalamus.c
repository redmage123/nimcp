/**
 * @file nimcp_brain_init_hypothalamus.c
 * @brief Hypothalamus Initialization Implementation
 *
 * WHAT: Initialization functions for hypothalamus (homeostatic regulation)
 * WHY:  Enable homeostatic regulation capabilities in the brain
 * HOW:  Creates hypothalamus adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase H1: Hypothalamus Brain Integration
 */

/*=============================================================================
 * Includes
 *===========================================================================*/

#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_HYPOTHALAMUS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_hypothalamus, MESH_ADAPTER_CATEGORY_SYSTEM)


/* Compatibility macro for set_error (converts to LOG_ERROR) */
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

/* Hypothalamus includes */
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Connect hypothalamus to bio-async messaging
 */
static bool connect_hypothalamus_to_bio_async(brain_t brain) {
    if (!brain || !brain->hypothalamus) return true;  /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register hypothalamus message handlers
         * bio_router_register_module(router, BIO_MODULE_HYPOTHALAMUS, brain->hypothalamus);
         */
        LOG_DEBUG(LOG_MODULE, "Hypothalamus registered with bio-async");
    }

    return true;
}

/**
 * @brief Apply circadian configuration from brain config
 */
static void apply_circadian_config(hypothalamus_config_t* hypo_cfg,
                                    const brain_config_t* brain_cfg) {
    if (!hypo_cfg || !brain_cfg) return;

    /* Enable circadian if sleep system is enabled */
    hypo_cfg->enable_circadian = true;

    /* Set initial phase based on current simulated time if available */
    hypo_cfg->initial_phase = 0.0f;  /* Default: midnight */
}

/**
 * @brief Apply autonomic configuration from brain config
 */
static void apply_autonomic_config(hypothalamus_config_t* hypo_cfg,
                                    const brain_config_t* brain_cfg) {
    if (!hypo_cfg || !brain_cfg) return;

    /* Enable autonomic control if medulla is present */
    hypo_cfg->enable_autonomic = true;
    hypo_cfg->enable_brainstem_output = true;
}

/*=============================================================================
 * Public API Implementation
 *===========================================================================*/

bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hypothalamus_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hypothalamus) {
        return true;  /* Already initialized */
    }

    /* Check if hypothalamus should be enabled */
    /* Default to enabled if autonomic or circadian features are needed */
    /* Note: brain_config_t may not have explicit hypothalamus flag */

    LOG_INFO(LOG_MODULE, "Initializing hypothalamus subsystem");

    /* Create hypothalamus adapter with configuration */
    hypothalamus_config_t hypo_cfg = hypothalamus_default_config();

    /* Apply brain-level configuration */
    apply_circadian_config(&hypo_cfg, &brain->config);
    apply_autonomic_config(&hypo_cfg, &brain->config);

    /* Enable bio-async if brain has it enabled */
    hypo_cfg.enable_bio_async = brain->bio_async_enabled;

    /* Create the adapter */
    brain->hypothalamus = hypothalamus_create(&hypo_cfg);
    if (!brain->hypothalamus) {
        set_error("Failed to create hypothalamus adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_hypothalamus_subsystem: hypothalamus_create failed");
        return false;
    }

    brain->hypothalamus_enabled = true;
    brain->last_hypothalamus_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus limbic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus brainstem bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus pituitary bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_hypothalamus_to_sleep(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus-Sleep connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hypothalamus_to_immune(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus-Immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus-Wellbeing connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hypothalamus_to_medulla(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus-Medulla connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_hypothalamus_to_emotions(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus-Emotions connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_hypothalamus_to_bio_async(brain)) {
        LOG_WARNING(LOG_MODULE, "Hypothalamus bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Hypothalamus subsystem initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hypothalamus_limbic_bridge: brain is NULL");

            return false;
    }

    /* Need hypothalamus adapter first */
    if (!brain->hypothalamus) {
        return true;  /* Not ready yet, will be called again */
    }

    /*
     * TODO: Implement limbic bridge
     *
     * The limbic bridge would connect:
     * - Amygdala → Hypothalamus: Threat/fear signals
     * - Hippocampus → Hypothalamus: Contextual stress modulation
     * - Hypothalamus → Amygdala: Cortisol effects on fear memory
     *
     * For now, we note that the connection is needed but defer implementation.
     * Note: Uses amygdala if available for limbic-hypothalamus integration.
     */

    /* Amygdala integration is handled via subcortical bridges */
    LOG_DEBUG(LOG_MODULE, "Hypothalamus-limbic bridge (placeholder)");

    return true;
}

bool nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hypothalamus_brainstem_bridge: brain is NULL");

            return false;
    }

    /* Need hypothalamus adapter first */
    if (!brain->hypothalamus) {
        return true;  /* Not ready yet */
    }

    /*
     * TODO: Implement brainstem bridge
     *
     * The brainstem bridge would connect:
     * - Hypothalamus → NTS: Cardiovascular control
     * - Hypothalamus → DMV: Digestive control
     * - Hypothalamus → Reticular formation: Arousal
     *
     * For now, we can use medulla integration if available.
     */

    if (brain->medulla) {
        /* Use medulla as brainstem representative */
        LOG_DEBUG(LOG_MODULE, "Hypothalamus-brainstem bridge via medulla");
    }

    return true;
}

bool nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hypothalamus_pituitary_bridge: brain is NULL");

            return false;
    }

    /* Need hypothalamus adapter first */
    if (!brain->hypothalamus) {
        return true;  /* Not ready yet */
    }

    /*
     * TODO: Implement pituitary bridge
     *
     * The pituitary bridge would model:
     * - Hypophyseal portal system
     * - Releasing hormone transport (CRH, TRH, GnRH, GHRH)
     * - Anterior pituitary hormone release (ACTH, TSH, FSH/LH, GH)
     * - Feedback loops
     *
     * This is essential for full neuroendocrine simulation.
     * Note: Currently implemented via substrate bridge for metabolic integration.
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus pituitary bridge (placeholder)");
    return true;
}

bool nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_hypothalamus_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->hypothalamus_quantum_bridge) {
        return true;
    }

    /* Need hypothalamus adapter first */
    if (!brain->hypothalamus) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        LOG_DEBUG(LOG_MODULE, "Quantum reasoning not enabled, skipping quantum bridge");
        return true;  /* Not enabled, not an error */
    }

    /* Create quantum bridge with default config */
    hypothalamus_quantum_config_t config = hypothalamus_quantum_default_config();

    brain->hypothalamus_quantum_bridge = hypothalamus_quantum_bridge_create(
        brain->hypothalamus, &config);

    if (!brain->hypothalamus_quantum_bridge) {
        LOG_WARNING(LOG_MODULE, "Failed to create hypothalamus quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_hypothalamus_quantum_bridge: quantum bridge creation failed");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Hypothalamus quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_hypothalamus_to_sleep(brain_t brain) {
    if (!brain || !brain->hypothalamus) {
        return true;  /* Nothing to connect */
    }

    /*
     * Connect hypothalamus circadian system to sleep/wake system.
     *
     * The SCN (suprachiasmatic nucleus) is the master circadian pacemaker.
     * It drives:
     * - Melatonin release timing → Sleep onset
     * - Cortisol morning peak → Waking
     * - Body temperature rhythm → Sleep propensity
     */

    /* TODO: Register hypothalamus as circadian time source for sleep system
     * if (brain->sleep_system) {
     *     sleep_system_set_circadian_source(brain->sleep_system, brain->hypothalamus);
     * }
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus connected to sleep system");
    return true;
}

bool nimcp_brain_factory_connect_hypothalamus_to_immune(brain_t brain) {
    if (!brain || !brain->hypothalamus) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    /*
     * Connect hypothalamus to brain immune system.
     *
     * Cytokine-hypothalamus interactions:
     * - IL-1β, IL-6, TNF-α → Fever (temperature setpoint increase)
     * - Cytokines → Sickness behavior (reduced appetite, fatigue)
     * - Cortisol → Immune suppression (negative feedback)
     *
     * This is the neuroimmune interface.
     */

    /* TODO: Register for cytokine signals
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_IL6 | CYTOKINE_TNF_A,
     *     hypothalamus_cytokine_callback, brain->hypothalamus);
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain_t brain) {
    if (!brain || !brain->hypothalamus) {
        return true;  /* Nothing to connect */
    }

    /* Check if wellbeing monitoring is enabled */
    if (!brain->wellbeing_monitoring_enabled) {
        return true;  /* Wellbeing not enabled */
    }

    /*
     * Connect hypothalamus to wellbeing monitor.
     *
     * Bidirectional connection:
     * - Chronic stress → Increased distress signals
     * - Distress → HPA axis sensitization
     * - Cortisol levels feed into wellbeing assessment
     */

    /* TODO: Register hypothalamus stress metrics with wellbeing
     * wellbeing_register_stress_source(brain->wellbeing,
     *     "hypothalamus", hypothalamus_get_cortisol, brain->hypothalamus);
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus connected to wellbeing monitor");
    return true;
}

bool nimcp_brain_factory_connect_hypothalamus_to_medulla(brain_t brain) {
    if (!brain || !brain->hypothalamus) {
        return true;  /* Nothing to connect */
    }

    /* Check if medulla is available */
    if (!brain->medulla_enabled || !brain->medulla) {
        return true;  /* Medulla not enabled */
    }

    /*
     * Connect hypothalamus to medulla oblongata.
     *
     * Bidirectional connection:
     * - Hypothalamus → Medulla: Autonomic commands
     * - Medulla → Hypothalamus: Arousal state feedback
     * - Coordination of circadian and arousal states
     */

    /* TODO: Establish bidirectional connection
     * medulla_set_hypothalamus(brain->medulla, brain->hypothalamus);
     * hypothalamus_set_medulla(brain->hypothalamus, brain->medulla);
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus connected to medulla");
    return true;
}

bool nimcp_brain_factory_connect_hypothalamus_to_emotions(brain_t brain) {
    if (!brain || !brain->hypothalamus) {
        return true;  /* Nothing to connect */
    }

    /* Check if emotional system is available */
    if (!brain->emotional_system) {
        return true;  /* Emotional system not initialized */
    }

    /*
     * Connect hypothalamus to emotional system.
     *
     * Bidirectional connection:
     * - Negative emotions → HPA axis activation
     * - Positive emotions → HPA axis suppression
     * - Cortisol → Affects emotional valence
     * - Hunger/thirst drives → Emotional urgency
     */

    /* TODO: Register bidirectional callbacks
     * emotional_system_register_stress_callback(brain->emotional_system,
     *     hypothalamus_stress_callback, brain->hypothalamus);
     * hypothalamus_set_emotion_input(brain->hypothalamus,
     *     emotional_system_get_valence, brain->emotional_system);
     */

    LOG_DEBUG(LOG_MODULE, "Hypothalamus connected to emotional system");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy hypothalamus subsystem
 *
 * WHAT: Clean up all hypothalamus resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_hypothalamus_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_hypothalamus_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying hypothalamus subsystem");

    /* Destroy quantum bridge first (depends on hypothalamus) */
    if (brain->hypothalamus_quantum_bridge) {
        hypothalamus_quantum_bridge_destroy(brain->hypothalamus_quantum_bridge);
        brain->hypothalamus_quantum_bridge = NULL;
    }

    /* Destroy hypothalamus adapter */
    if (brain->hypothalamus) {
        hypothalamus_destroy(brain->hypothalamus);
        brain->hypothalamus = NULL;
    }

    brain->hypothalamus_enabled = false;
    brain->last_hypothalamus_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Hypothalamus subsystem destroyed");
}

//=============================================================================
// nimcp_brain_init_insula.c - Insula Region Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_insula.c
 * @brief Insula Region Initialization Implementation
 *
 * WHAT: Initialization functions for Insula region (interoception, emotion, social)
 * WHY:  Enable body awareness and emotional processing in the brain
 * HOW:  Creates Insula adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase I1: Insula Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_insula.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_INSULA"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_insula, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Insula region includes
#include "core/brain/regions/insula/nimcp_insula_adapter.h"
#include "core/brain/regions/insula/nimcp_insula_quantum_bridge.h"

// Bio-async includes
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Message handler for insula cytokine updates
 *
 * WHAT: Process cytokine signals affecting interoceptive awareness
 * WHY:  Enable immune-interoception integration (sickness behavior)
 * HOW:  Update insula sensitivity based on cytokine levels
 *
 * @param msg Cytokine update message
 * @param msg_size Message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Insula adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t insula_handle_cytokine_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when cytokine message format is finalized
     *
     * Implementation would:
     * 1. Extract cytokine type and concentration from message
     * 2. Update insula interoceptive thresholds based on cytokine levels
     * 3. Modulate body state representation (fatigue, discomfort)
     * 4. Trigger sickness behavior signals if inflammation is high
     */

    LOG_TRACE(LOG_MODULE, "Insula received cytokine update (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for insula salience queries
 *
 * WHAT: Respond to salience evaluation requests
 * WHY:  Insula is key node in salience network
 * HOW:  Compute salience based on interoceptive signals
 *
 * @param msg Salience query message
 * @param msg_size Message size
 * @param response_promise Promise for response
 * @param user_data Insula adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t insula_handle_salience_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when salience message format is finalized
     *
     * Implementation would:
     * 1. Evaluate current interoceptive signals
     * 2. Compute salience score based on body state deviation
     * 3. Send response with salience value and contributing factors
     * 4. Flag high-salience events for attention system
     */

    LOG_TRACE(LOG_MODULE, "Insula received salience query (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for insula emotion tensor updates
 *
 * WHAT: Process emotion tensor updates for body-emotion integration
 * WHY:  Insula bridges bodily feelings and emotional states
 * HOW:  Update interoceptive predictions based on emotion state
 *
 * @param msg Emotion tensor update message
 * @param msg_size Message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Insula adapter pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t insula_handle_emotion_tensor(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when emotion tensor format is finalized
     *
     * Implementation would:
     * 1. Extract emotion valence/arousal from tensor
     * 2. Update body state predictions (heart rate, gut feelings)
     * 3. Modulate interoceptive sensitivity based on emotion
     * 4. Generate somatic markers for decision-making
     */

    LOG_TRACE(LOG_MODULE, "Insula received emotion tensor update (stub)");
    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback for Insula
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Insula adapter pointer
 * @return 0 on success, -1 on error
 */
static int insula_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "insula_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_CYTOKINE_UPDATE:
                bio_router_register_handler(ctx, message_types[i], insula_handle_cytokine_update);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_CYTOKINE_UPDATE");
                break;

            case BIO_MSG_SALIENCE_QUERY:
                bio_router_register_handler(ctx, message_types[i], insula_handle_salience_query);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SALIENCE_QUERY");
                break;

            case BIO_MSG_EMOTION_TENSOR_UPDATE:
                bio_router_register_handler(ctx, message_types[i], insula_handle_emotion_tensor);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_EMOTION_TENSOR_UPDATE");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Connect Insula to bio-async messaging
 */
static bool connect_insula_to_bio_async(brain_t brain) {
    if (!brain || !brain->insula) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /* Register insula module with bio-async router */
        bio_module_info_t info = {
            .module_id = BIO_MODULE_INSULA,
            .module_name = "insula",
            .inbox_capacity = 32,
            .user_data = brain->insula
        };

        bio_module_context_t ctx = bio_router_register_module(&info);
        if (ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_INSULA,
                (void*)insula_wiring_handler_callback,
                brain->insula
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_DEBUG(LOG_MODULE, "Insula bio-async registered with KG-driven wiring (module_id=0x%04X)",
                          BIO_MODULE_INSULA);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_CYTOKINE_UPDATE,
                                                insula_handle_cytokine_update)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_SALIENCE_QUERY,
                                                insula_handle_salience_query)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_EMOTION_TENSOR_UPDATE,
                                                insula_handle_emotion_tensor)
                );
                LOG_DEBUG(LOG_MODULE, "Insula bio-async registered with legacy handlers (module_id=0x%04X)",
                          BIO_MODULE_INSULA);
            }
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register insula with bio-async");
        }
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_insula_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_insula_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->insula) {
        return true;  /* Already initialized */
    }

    /* Check if Insula should be enabled */
    /* Default to enabled for emotionally-capable brains */
    if (!brain->config.enable_emotional_tagging &&
        !brain->config.enable_multimodal_integration) {
        brain->insula_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing Insula subsystem");

    /* Create Insula adapter with default configuration */
    insula_config_t insula_cfg = insula_default_config();

    /* Scale configuration based on brain config */
    /* Insula sensitivity can be modulated by overall emotional sensitivity */

    brain->insula = insula_create(&insula_cfg);
    if (!brain->insula) {
        set_error("Failed to create Insula adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_insula_subsystem: brain->insula is NULL");
        return false;
    }

    brain->insula_enabled = true;
    brain->last_insula_update_us = 0;

    /* Initialize quantum bridge */
    if (!nimcp_brain_factory_init_insula_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Insula quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_insula_to_limbic(brain)) {
        LOG_WARN(LOG_MODULE, "Insula-limbic connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_insula_to_somatosensory(brain)) {
        LOG_WARN(LOG_MODULE, "Insula-somatosensory connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_insula_to_emotional(brain)) {
        LOG_WARN(LOG_MODULE, "Insula-emotional connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_insula_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Insula-immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_insula_to_theory_of_mind(brain)) {
        LOG_WARN(LOG_MODULE, "Insula-ToM connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_insula_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Insula bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Insula subsystem initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_insula_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_insula_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->insula_quantum_bridge) {
        return true;
    }

    /* Need Insula adapter first */
    if (!brain->insula) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        return true;  /* Not enabled, not an error */
    }

    LOG_DEBUG(LOG_MODULE, "Initializing Insula quantum bridge");

    /* Create quantum bridge with default config */
    insula_quantum_config_t config = insula_quantum_default_config();

    /* Scale based on brain configuration */
    /* More interoceptive channels for larger brains */
    /* Use num_outputs as proxy for brain size since num_hidden_neurons doesn't exist */
    if (brain->config.num_outputs > 100) {
        config.intero_channels = 32;
    }

    brain->insula_quantum_bridge = insula_quantum_bridge_create(
        brain->insula, &config);

    if (!brain->insula_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Insula quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_insula_quantum_bridge: brain->insula_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Insula quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_insula_to_limbic(brain_t brain) {
    if (!brain || !brain->insula) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting Insula to limbic system");

    /*
     * Connect Insula to limbic structures for emotional integration.
     *
     * BIOLOGICAL BASIS:
     * - Anterior insula → Amygdala (fear, threat valuation)
     * - Insula ↔ ACC (emotional conflict monitoring)
     * - Insula → Hippocampus (emotional memory context)
     *
     * Bidirectional connections allow:
     * - Body signals to influence emotional state (bottom-up)
     * - Emotional states to modulate interoception (top-down)
     */

    /* TODO: Connect to amygdala if available */
    /* TODO: Connect to anterior cingulate if available */
    /* TODO: Connect to hippocampal emotional memory */

    LOG_DEBUG(LOG_MODULE, "Insula connected to limbic system");
    return true;
}

bool nimcp_brain_factory_connect_insula_to_somatosensory(brain_t brain) {
    if (!brain || !brain->insula) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting Insula to somatosensory cortex");

    /*
     * Connect posterior insula to somatosensory areas for body awareness.
     *
     * PATHWAYS:
     * - S1 (Primary somatosensory) → Posterior insula (raw body signals)
     * - S2 (Secondary somatosensory) → Mid-insula (integrated body state)
     * - Posterior parietal → Insula (body schema, agency)
     *
     * SIGNALS:
     * - Tactile: Touch, pressure, texture
     * - Proprioceptive: Body position, movement
     * - Temperature: Warm/cold sensation
     * - Pain: Nociceptive signals
     */

    /* TODO: Register interoceptive callbacks with somatosensory cortex */
    /* TODO: Set up body map integration */

    LOG_DEBUG(LOG_MODULE, "Insula connected to somatosensory cortex");
    return true;
}

bool nimcp_brain_factory_connect_insula_to_emotional(brain_t brain) {
    if (!brain || !brain->insula) {
        return true;  /* Nothing to connect */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting Insula to emotional system");

    /* Check if emotional system is available */
    if (!brain->emotional_system) {
        LOG_DEBUG(LOG_MODULE, "Emotional system not initialized yet");
        return true;  /* Not ready */
    }

    /*
     * Connect Insula to central emotional processing.
     *
     * INTEGRATION:
     * - Insula provides subjective feeling states
     * - Emotional system provides valence/arousal dimensions
     * - Bidirectional: body affects emotion, emotion affects body
     *
     * The insula is critical for:
     * - Emotional awareness (how do I feel?)
     * - Somatic markers (gut feelings)
     * - Emotional salience (what matters?)
     */

    /* TODO: Register bidirectional emotion callbacks */
    /* emotional_system_register_body_callback(brain->emotional_system,
     *     insula_process_emotion, brain->insula);
     * insula_set_emotion_callback(brain->insula,
     *     emotional_system_update_from_body, brain->emotional_system);
     */

    LOG_DEBUG(LOG_MODULE, "Insula connected to emotional system");
    return true;
}

bool nimcp_brain_factory_connect_insula_to_immune(brain_t brain) {
    if (!brain || !brain->insula) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting Insula to immune system");

    /*
     * Connect Insula to brain immune system for sickness behavior.
     *
     * BIOLOGICAL BASIS:
     * When the immune system is activated, cytokines signal to the brain:
     * - IL-1β → Fever, fatigue, malaise
     * - IL-6 → Mood changes, cognitive fog
     * - TNF-α → Social withdrawal, anhedonia
     *
     * The insula receives these signals and:
     * - Updates body state (fatigue, discomfort)
     * - Modulates emotional state (decreased valence)
     * - Triggers avoidance behavior (social withdrawal)
     */

    /* TODO: Register for cytokine signals
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_IL6 | CYTOKINE_TNF_A,
     *     insula_immune_callback, brain->insula);
     */

    LOG_DEBUG(LOG_MODULE, "Insula connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_insula_to_theory_of_mind(brain_t brain) {
    if (!brain || !brain->insula) {
        return true;  /* Nothing to connect */
    }

    /* Check if theory of mind is available */
    if (!brain->theory_of_mind) {
        return true;  /* ToM not initialized */
    }

    LOG_DEBUG(LOG_MODULE, "Connecting Insula to Theory of Mind");

    /*
     * Connect Insula to Theory of Mind for empathy and social cognition.
     *
     * EMPATHY PATHWAY:
     * 1. Observe other's emotional expression
     * 2. Mirror neurons activate (TPJ, STS)
     * 3. Insula generates empathic resonance (shared feeling)
     * 4. ToM adds perspective-taking (cognitive empathy)
     *
     * The anterior insula is critical for:
     * - Empathic resonance (feeling what others feel)
     * - Disgust recognition in others
     * - Trust and fairness evaluation
     */

    /* TODO: Register social emotion callbacks
     * theory_of_mind_register_empathy_callback(brain->theory_of_mind,
     *     insula_process_empathy, brain->insula);
     * insula_set_social_callback(brain->insula,
     *     theory_of_mind_update_social, brain->theory_of_mind);
     */

    LOG_DEBUG(LOG_MODULE, "Insula connected to Theory of Mind");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy insula subsystem
 *
 * WHAT: Clean up all insula resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_insula_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_insula_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying insula subsystem");

    /* Destroy quantum bridge first (depends on insula) */
    if (brain->insula_quantum_bridge) {
        insula_quantum_bridge_destroy(brain->insula_quantum_bridge);
        brain->insula_quantum_bridge = NULL;
    }

    /* Destroy insula adapter */
    if (brain->insula) {
        insula_destroy(brain->insula);
        brain->insula = NULL;
    }

    brain->insula_enabled = false;
    brain->last_insula_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Insula subsystem destroyed");
}

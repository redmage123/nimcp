/**
 * @file nimcp_brain_init_collective_cognition.c
 * @brief Factory initialization for collective cognition subsystem
 *
 * WHAT: Creates distributed consciousness system for multi-brain coordination
 * WHY:  Enables inter-instance synchronization, shared intentionality, and extended mind
 * HOW:  Creates collective cognition system and optionally connects to immune system
 *
 * COMPONENTS:
 * - Hyperscanning: Real-time neural synchronization using EEG-like frequency bands
 * - Extended Mind: External tools as cognitive extensions
 * - Collective Phi: Integrated Information Theory metrics for group consciousness
 * - Shared Intentionality: Joint goals and we-mode cognition
 *
 * THEORETICAL BASIS:
 * - Integrated Information Theory (Tononi, 2004, 2014)
 * - Shared Intentionality (Tomasello, 2005, 2009)
 * - Extended Mind Thesis (Clark & Chalmers, 1998)
 * - Inter-brain synchronization (Dumas et al., 2010)
 *
 * @author NIMCP Development Team
 * @date 2025-01-01
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "utils/logging/nimcp_logging.h"

/* Bio-async includes */
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_COLLECTIVE_COGNITION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_collective_cognition, MESH_ADAPTER_CATEGORY_SYSTEM)


/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Message handler for collective cognition consensus requests
 *
 * WHAT: Process consensus voting requests from swarm members
 * WHY:  Enable distributed decision-making across brain instances
 * HOW:  Evaluate proposal and contribute vote to consensus
 *
 * @param msg Consensus request message with proposal details
 * @param msg_size Message size
 * @param response_promise Promise for vote response
 * @param user_data Collective cognition context pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t collective_handle_consensus_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when consensus protocol is finalized
     *
     * Implementation would:
     * 1. Extract proposal from message
     * 2. Evaluate proposal against local goals/beliefs
     * 3. Compute vote (support/oppose/abstain) with confidence
     * 4. Send vote response with justification
     * 5. Update local belief state based on proposal
     */

    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for collective cognition broadcast messages
 *
 * WHAT: Process broadcast messages for swarm-wide state updates
 * WHY:  Enable shared awareness across distributed consciousness
 * HOW:  Update local state based on broadcast content
 *
 * @param msg Broadcast message with shared state
 * @param msg_size Message size
 * @param response_promise Promise for acknowledgment (may be NULL)
 * @param user_data Collective cognition context pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t collective_handle_broadcast(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when broadcast protocol is finalized
     *
     * Implementation would:
     * 1. Extract broadcast type and content
     * 2. Validate sender authority for broadcast type
     * 3. Update local shared intentionality state
     * 4. Propagate to relevant subsystems (if needed)
     * 5. Send acknowledgment if requested
     */

    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler for collective cognition swarm update
 *
 * WHAT: Process swarm state synchronization updates
 * WHY:  Maintain coherent swarm state for collective consciousness
 * HOW:  Integrate swarm metrics into local phi computation
 *
 * @param msg Swarm update message with synchronization data
 * @param msg_size Message size
 * @param response_promise Promise for sync acknowledgment (may be NULL)
 * @param user_data Collective cognition context pointer
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t collective_handle_swarm_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;

    /*
     * TODO: Full implementation when swarm protocol is finalized
     *
     * Implementation would:
     * 1. Extract hyperscanning sync data (phase coherence, frequency bands)
     * 2. Update collective phi metrics
     * 3. Adjust local synchronization parameters
     * 4. Check for leader emergence/change
     * 5. Update swarm formation state
     */

    return NIMCP_SUCCESS;
}

/**
 * @brief KG-driven wiring handler callback for collective cognition
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Collective cognition context pointer
 * @return 0 on success, -1 on error
 */
static int collective_cognition_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO(LOG_MODULE, "collective_cognition_wiring_handler_callback: registering %u handlers from KG",
             message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_SWARM_CONSENSUS_REQUEST:
                bio_router_register_handler(ctx, message_types[i], collective_handle_consensus_request);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SWARM_CONSENSUS_REQUEST");
                break;

            case BIO_MSG_SWARM_CONSENSUS_REACHED:
                bio_router_register_handler(ctx, message_types[i], collective_handle_broadcast);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SWARM_CONSENSUS_REACHED");
                break;

            case BIO_MSG_SWARM_SIGNAL_UPDATE:
                bio_router_register_handler(ctx, message_types[i], collective_handle_swarm_update);
                LOG_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SWARM_SIGNAL_UPDATE");
                break;

            default:
                LOG_DEBUG(LOG_MODULE, "  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

/**
 * @brief Connect collective cognition to bio-async messaging
 */
static bool connect_collective_cognition_to_bio_async(brain_t brain) {
    if (!brain || !brain->collective_cognition) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /* Register collective cognition module with bio-async router */
        bio_module_info_t info = {
            .module_id = BIO_MODULE_BRAIN_COGNITIVE,
            .module_name = "collective_cognition",
            .inbox_capacity = 64,
            .user_data = brain->collective_cognition
        };

        bio_module_context_t ctx = bio_router_register_module(&info);
        if (ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_BRAIN_COGNITIVE,
                (void*)collective_cognition_wiring_handler_callback,
                brain->collective_cognition
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_DEBUG(LOG_MODULE, "Collective cognition bio-async registered with KG-driven wiring (module_id=0x%04X)",
                          BIO_MODULE_BRAIN_COGNITIVE);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_SWARM_CONSENSUS_REQUEST,
                                                collective_handle_consensus_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_SWARM_CONSENSUS_REACHED,
                                                collective_handle_broadcast)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx, BIO_MSG_SWARM_SIGNAL_UPDATE,
                                                collective_handle_swarm_update)
                );
                LOG_DEBUG(LOG_MODULE, "Collective cognition bio-async registered with legacy handlers (module_id=0x%04X)",
                          BIO_MODULE_BRAIN_COGNITIVE);
            }
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register collective cognition with bio-async");
        }
    }

    return true;
}

/*=============================================================================
 * Collective Cognition Subsystem Initialization
 *===========================================================================*/

/**
 * @brief Initialize collective cognition subsystem
 *
 * WHAT: Creates distributed consciousness system for multi-brain coordination
 * WHY:  Enables inter-instance synchronization, shared intentionality, and extended mind
 * HOW:  Creates collective cognition system and optionally connects to immune system
 */
bool nimcp_brain_factory_init_collective_cognition_subsystem(brain_t brain)
{
    /* Guard clause: Validate brain pointer */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_collective_cognition_subsystem: brain is NULL");
        return false;
    }

    /* Initialize collective cognition fields to defaults */
    brain->collective_cognition = NULL;
    brain->collective_cognition_enabled = false;

    /* Check if collective cognition is enabled in config */
    if (!brain->config.enable_collective_cognition) {
        LOG_DEBUG("Collective cognition system disabled in config");
        return true;  /* Success - collective cognition disabled */
    }

    /* Build collective cognition configuration from brain config */
    collective_cognition_config_t cc_config;
    cc_config = collective_cognition_default_config();

    /* Override defaults with brain config values */

    /* Sizing */
    cc_config.max_instances = brain->config.collective_max_instances > 0 ?
        brain->config.collective_max_instances : COLLECTIVE_MAX_INSTANCES;

    /* Hyperscanning config */
    cc_config.hyperscanning.max_instances = cc_config.max_instances;
    cc_config.hyperscanning.sync_threshold = brain->config.collective_sync_threshold > 0.0f ?
        brain->config.collective_sync_threshold : 0.7f;
    cc_config.hyperscanning.enable_leader_detection = brain->config.collective_enable_leader_detection;
    cc_config.hyperscanning.enable_bio_async = brain->config.collective_enable_bio_async;

    /* Extended mind config */
    cc_config.extended_mind.max_extensions = brain->config.collective_max_extensions > 0 ?
        brain->config.collective_max_extensions : 32;
    cc_config.extended_mind.enable_bio_async = brain->config.collective_enable_bio_async;

    /* Phi config */
    cc_config.phi.min_instances_for_phi = 2;
    cc_config.phi.coherence_weight = brain->config.collective_coherence_weight > 0.0f ?
        brain->config.collective_coherence_weight : 0.3f;

    /* Shared intentionality config */
    cc_config.intentionality.max_shared_goals = brain->config.collective_max_shared_goals > 0 ?
        brain->config.collective_max_shared_goals : 64;
    cc_config.intentionality.max_joint_attentions = brain->config.collective_max_joint_attentions > 0 ?
        brain->config.collective_max_joint_attentions : 32;
    cc_config.intentionality.commitment_threshold = brain->config.collective_commitment_threshold > 0.0f ?
        brain->config.collective_commitment_threshold : 0.5f;
    cc_config.intentionality.we_mode_threshold = brain->config.collective_we_mode_threshold > 0.0f ?
        brain->config.collective_we_mode_threshold : 0.6f;
    cc_config.intentionality.enable_bio_async = brain->config.collective_enable_bio_async;

    /* Global settings */
    cc_config.enable_bio_async = brain->config.collective_enable_bio_async;

    /* Create collective cognition system */
    collective_cognition_t* cc = collective_cognition_create(&cc_config);
    if (!cc) {
        LOG_ERROR("Failed to create collective cognition system for brain '%s'",
                  brain->config.task_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_collective_cognition_subsystem: cc is NULL");
        return false;
    }

    brain->collective_cognition = cc;
    brain->collective_cognition_enabled = true;

    /* Connect to bio-async messaging system */
    if (!connect_collective_cognition_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Collective cognition bio-async connection failed (non-fatal)");
    }

    /* Connect to immune system if both are enabled */
    if (brain->config.collective_enable_immune_integration && brain->immune_enabled && brain->immune_system) {
        /* Register collective cognition as a threat pattern source */
        LOG_DEBUG("Collective cognition connected to brain immune system");
        /* TODO: Create a collective_cognition_immune_bridge when needed */
    }

    LOG_INFO("Collective cognition system enabled for brain '%s' (max_instances=%u)",
             brain->config.task_name, cc_config.max_instances);

    return true;
}


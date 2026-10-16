//=============================================================================
// nimcp_brain_init_security_perception_bridge.c - Security-Perception Bridge Init
//=============================================================================
/**
 * @file nimcp_brain_init_security_perception_bridge.c
 * @brief Security-Perception Bridge subsystem initialization for brain
 *
 * WHAT: Security-Perception Bridge initialization and integration setup
 * WHY:  Sensory threat analysis and defense at the perception layer
 * HOW:  Creates bridge, connects BBB/anomaly/immune/perception, starts monitoring
 *
 * BIOLOGICAL BASIS:
 * Models the nervous system's sensory gating for threats:
 * - Thalamus: Sensory relay/gatekeeper before cortical processing
 * - Amygdala: Rapid threat detection in visual/auditory streams
 * - Prefrontal Cortex: Top-down filtering of suspicious inputs
 * - Immune System: Responds to "danger signals" from sensory pathways
 *
 * DESIGN PATTERNS:
 * - Factory: Created by brain factory during lifecycle init
 * - Bridge: Connects security and perception layers
 * - Chain of Responsibility: Threat detection escalation
 * - Strategy: Pluggable detection algorithms
 * - Observer: Threat notification callbacks
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "security/nimcp_security_perception_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_SEC_PERCEPT"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_security_perception_bridge, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

/**
 * @brief Initialize security-perception bridge subsystem
 *
 * WHAT: Creates and configures security-perception bridge for brain
 * WHY:  Provides sensory threat detection and defense
 * HOW:  Guard clause checks, create bridge, wire security/perception modules
 *
 * @param brain Brain instance to initialize bridge for
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_security_perception_bridge_subsystem(brain_t brain) {
    /* Guard clause: NULL check */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_security_perception_bridge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields to defaults */
    brain->security_perception_bridge = NULL;
    brain->security_perception_bridge_enabled = false;

    /* Check if security and perception are present */
    bool has_security = brain->bbb_system != NULL;
    bool has_perception = brain->visual_cortex || brain->audio_cortex;

    if (!has_security && !has_perception) {
        NIMCP_LOGGING_DEBUG("Security-perception bridge skipped (no dependencies)");
        return true;
    }

    /* Create bridge configuration */
    sec_percept_config_t config;
    sec_percept_default_config(&config);

    /* Configure based on brain settings */
    config.enable_bbb = (brain->bbb_system != NULL);
    config.enable_anomaly_detector = false;  /* Anomaly detector is separate subsystem */
    config.enable_immune_system = brain->immune_enabled;
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_statistical_checks = true;
    config.enable_adversarial_detection = true;
    config.enable_cross_modal_validation = (brain->visual_cortex && brain->audio_cortex);

    /* Create bridge */
    security_perception_bridge_t* bridge = sec_percept_create(&config);
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to create security-perception bridge - "
                          "continuing without sensory threat defense");
        return true;  /* Non-fatal */
    }

    /* Connect to BBB if available */
    if (brain->bbb_system) {
        if (sec_percept_connect_bbb(bridge, brain->bbb_system) == 0) {
            NIMCP_LOGGING_DEBUG("Security-perception bridge connected to BBB");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect security-perception bridge to BBB");
        }
    }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        if (sec_percept_connect_immune(bridge, brain->immune_system) == 0) {
            NIMCP_LOGGING_DEBUG("Security-perception bridge connected to brain immune");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect security-perception bridge to immune");
        }
    }

    /* Connect to visual cortex if available */
    if (brain->visual_cortex) {
        if (sec_percept_connect_visual_cortex(bridge, brain->visual_cortex) == 0) {
            NIMCP_LOGGING_DEBUG("Security-perception bridge connected to visual cortex");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect security-perception bridge to visual");
        }
    }

    /* Connect to audio cortex if available */
    if (brain->audio_cortex) {
        if (sec_percept_connect_audio_cortex(bridge, brain->audio_cortex) == 0) {
            NIMCP_LOGGING_DEBUG("Security-perception bridge connected to audio cortex");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect security-perception bridge to audio");
        }
    }

    /* Connect to bio-async if available */
    if (brain->bio_async_enabled) {
        if (sec_percept_connect_bio_async(bridge) == 0) {
            NIMCP_LOGGING_DEBUG("Security-perception bridge connected to bio-async");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect security-perception bridge to bio-async");
        }
    }

    /* Start the bridge */
    if (sec_percept_start(bridge) != 0) {
        NIMCP_LOGGING_WARN("Failed to start security-perception bridge");
        sec_percept_destroy(bridge);
        return true;  /* Non-fatal */
    }

    /* Store bridge in brain */
    brain->security_perception_bridge = bridge;
    brain->security_perception_bridge_enabled = true;

    /* Log success */
    sec_percept_stats_t stats;
    if (sec_percept_get_stats(bridge, &stats) == 0) {
        NIMCP_LOGGING_INFO("Security-perception bridge initialized: "
                          "state=%s, bbb=%s, immune=%s, visual=%s, audio=%s",
                          sec_percept_state_name(stats.state),
                          brain->bbb_system ? "connected" : "N/A",
                          brain->immune_enabled ? "connected" : "disabled",
                          brain->visual_cortex ? "connected" : "N/A",
                          brain->audio_cortex ? "connected" : "N/A");
    } else {
        NIMCP_LOGGING_INFO("Security-perception bridge initialized");
    }

    return true;
}

//=============================================================================
// nimcp_brain_init_medulla.c - Medulla Oblongata Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_medulla.c
 * @brief Medulla oblongata subsystem initialization for brain
 *
 * WHAT: Medulla initialization, update, and destruction functions
 * WHY:  Provides foundational brainstem autonomic regulation for brain
 * HOW:  Creates medulla, connects integrations, manages lifecycle
 *
 * BIOLOGICAL BASIS:
 * The medulla oblongata is the most primitive part of the brain, essential
 * for survival. It must be initialized early and destroyed last.
 *
 * MESH NETWORK INTEGRATION:
 * When mesh bootstrap is available, the medulla registers as a subcortical
 * participant and can coordinate arousal/protection changes through
 * distributed consensus.
 *
 * @version 1.1.0
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/medulla/nimcp_medulla.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Mesh integration - forward declarations to avoid header conflicts */
struct mesh_bootstrap;
typedef struct mesh_bootstrap mesh_bootstrap_t;
struct mesh_medulla_integration;
typedef struct mesh_medulla_integration mesh_medulla_integration_t;

/* Mesh medulla integration function declarations */
extern mesh_medulla_integration_t* mesh_medulla_create(
    mesh_bootstrap_t* bootstrap,
    void* medulla,
    const void* config
);
extern void mesh_medulla_destroy(mesh_medulla_integration_t* integration);
extern int mesh_medulla_register_participant(mesh_medulla_integration_t* integration);
extern int mesh_medulla_set_health_agent(mesh_medulla_integration_t* integration,
                                          void* agent);

#define LOG_MODULE "BRAIN_INIT_MEDULLA"

//=============================================================================
// Global Mesh Bootstrap Handle (set externally)
//=============================================================================

static mesh_bootstrap_t* g_mesh_bootstrap = NULL;
static mesh_medulla_integration_t* g_medulla_mesh_integration = NULL;

/**
 * @brief Set the mesh bootstrap handle for medulla mesh registration
 *
 * WHAT: Configures medulla init to register with mesh network
 * WHY:  Enable coordinated arousal/protection via mesh consensus
 * HOW:  Stores bootstrap handle, used during medulla init
 *
 * @param bootstrap Mesh bootstrap handle (NULL to disable)
 */
void nimcp_brain_medulla_set_mesh_bootstrap(mesh_bootstrap_t* bootstrap) {
    g_mesh_bootstrap = bootstrap;
    if (bootstrap) {
        NIMCP_LOGGING_INFO("Mesh bootstrap set for medulla integration");
    }
}

/**
 * @brief Get the medulla mesh integration handle
 *
 * @return Current mesh integration or NULL if not registered
 */
mesh_medulla_integration_t* nimcp_brain_medulla_get_mesh_integration(void) {
    return g_medulla_mesh_integration;
}
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_medulla, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Main Initialization Function
//=============================================================================

bool nimcp_brain_factory_init_medulla_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("Null brain in init_medulla_subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_medulla_subsystem: brain is NULL");
        return false;
    }

    /* Initialize medulla fields to defaults */
    brain->medulla = NULL;
    brain->medulla_enabled = false;
    brain->last_medulla_update_us = 0;

    /* Medulla should always be enabled - it's foundational */
    bool should_enable = true;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Medulla not enabled");
        return true;  /* Not an error - just not needed */
    }

    /* Create medulla configuration */
    medulla_config_t config = medulla_default_config();

    /* Configure based on brain settings */
    config.enable_bio_async = brain->bio_async_enabled;

    /* Set biologically-plausible update interval (50ms = 20Hz) */
    config.update_interval_ms = 50;

    /* Configure arousal for balanced baseline */
    config.arousal.baseline_arousal = 0.5f;
    config.arousal.arousal_decay_rate = 0.02f;
    config.arousal.min_arousal = 0.1f;
    config.arousal.max_arousal = 0.95f;

    /* Configure protection for reasonable thresholds */
    config.protection.health_threshold_critical = 0.2f;
    config.protection.health_threshold_defensive = 0.4f;
    config.protection.recovery_time_ms = 1000.0f;

    /* Configure circadian for 24-hour cycle */
    config.circadian.period_hours = 24.0f;
    config.circadian.phase_offset_hours = 0.0f;
    config.circadian.amplitude = 0.3f;

    /* Configure coupling for moderate strength */
    config.coupling.coupling_strength = 0.5f;
    config.coupling.latency_ms = 10.0f;

    /* Create medulla */
    medulla_t medulla = medulla_create(&config);
    if (!medulla) {
        NIMCP_LOGGING_WARN("Failed to create medulla - continuing without brainstem regulation");
        return true;  /* Non-fatal for now, but limits functionality */
    }

    /* Connect to bio-async if available */
    if (brain->bio_async_enabled) {
        if (medulla_connect_bio_async(medulla) == 0) {
            NIMCP_LOGGING_DEBUG("Medulla connected to bio-async router");
        } else {
            NIMCP_LOGGING_WARN("Failed to connect medulla to bio-async");
        }
    }

    /* Start the medulla */
    int result = medulla_start(medulla);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Failed to start medulla (error %d)", result);
        medulla_destroy(medulla);
        return true;  /* Non-fatal */
    }

    /* Store in brain */
    brain->medulla = medulla;
    brain->medulla_enabled = true;
    brain->last_medulla_update_us = brain->current_time_us;

    /* Register with mesh network if bootstrap is available */
    if (g_mesh_bootstrap) {
        brain_init_medulla_heartbeat("mesh_registration", 0.0f);

        mesh_medulla_integration_t* mesh_integration = mesh_medulla_create(
            g_mesh_bootstrap,
            medulla,
            NULL  /* Use default config */
        );

        if (mesh_integration) {
            /* Set health agent if available */
            if (g_brain_init_medulla_health_agent) {
                mesh_medulla_set_health_agent(mesh_integration,
                                              g_brain_init_medulla_health_agent);
            }

            /* Register as mesh participant */
            int mesh_result = mesh_medulla_register_participant(mesh_integration);
            if (mesh_result == NIMCP_SUCCESS) {
                g_medulla_mesh_integration = mesh_integration;
                NIMCP_LOGGING_INFO("Medulla registered with mesh network");
            } else {
                NIMCP_LOGGING_WARN("Failed to register medulla with mesh (error %d)",
                                   mesh_result);
                mesh_medulla_destroy(mesh_integration);
            }
        } else {
            NIMCP_LOGGING_WARN("Failed to create medulla mesh integration");
        }

        brain_init_medulla_heartbeat("mesh_registration", 1.0f);
    }

    /* Log initialization */
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    NIMCP_LOGGING_INFO("Medulla oblongata subsystem initialized successfully");
    NIMCP_LOGGING_INFO("  State: %s",
        stats.state == MEDULLA_STATE_RUNNING ? "RUNNING" :
        stats.state == MEDULLA_STATE_STOPPED ? "STOPPED" :
        stats.state == MEDULLA_STATE_DEGRADED ? "DEGRADED" : "UNKNOWN");
    NIMCP_LOGGING_INFO("  Arousal: %.2f (baseline: %.2f)",
        stats.current_arousal, config.arousal.baseline_arousal);
    NIMCP_LOGGING_INFO("  Protection Level: %s",
        stats.protection_level == PROTECTION_LEVEL_NORMAL ? "NORMAL" :
        stats.protection_level == PROTECTION_LEVEL_CAUTIOUS ? "CAUTIOUS" :
        stats.protection_level == PROTECTION_LEVEL_GUARDED ? "GUARDED" :
        stats.protection_level == PROTECTION_LEVEL_DEFENSIVE ? "DEFENSIVE" :
        stats.protection_level == PROTECTION_LEVEL_CRITICAL ? "CRITICAL" : "SHUTDOWN");
    NIMCP_LOGGING_INFO("  Circadian Phase: %d (%.1f hours)",
        (int)medulla_get_circadian_phase(medulla), stats.circadian_time_hours);
    NIMCP_LOGGING_INFO("  Bio-Async: %s",
        medulla_is_bio_async_connected(medulla) ? "CONNECTED" : "DISABLED");

    return true;
}

//=============================================================================
// Update Function
//=============================================================================

int nimcp_brain_update_medulla_subsystem(brain_t brain, float delta_time_s) {
    if (!brain) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!brain->medulla_enabled || !brain->medulla) {
        return NIMCP_SUCCESS;  /* No medulla to update */
    }

    /* Update medulla */
    int result = medulla_update(brain->medulla, delta_time_s);
    if (result != NIMCP_SUCCESS) {
        NIMCP_LOGGING_WARN("Medulla update failed (error %d)", result);
        return result;
    }

    /* Update timestamp */
    uint64_t delta_us = (uint64_t)(delta_time_s * 1000000.0f);
    brain->last_medulla_update_us = brain->current_time_us;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Destruction Function
//=============================================================================

void nimcp_brain_destroy_medulla_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    /* Destroy mesh integration first */
    if (g_medulla_mesh_integration) {
        mesh_medulla_destroy(g_medulla_mesh_integration);
        g_medulla_mesh_integration = NULL;
        NIMCP_LOGGING_DEBUG("Medulla mesh integration destroyed");
    }

    if (brain->medulla) {
        /* Stop medulla first */
        medulla_stop(brain->medulla);

        /* Destroy medulla */
        medulla_destroy(brain->medulla);
        brain->medulla = NULL;

        NIMCP_LOGGING_DEBUG("Medulla subsystem destroyed");
    }

    brain->medulla_enabled = false;
    brain->last_medulla_update_us = 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

float nimcp_brain_get_arousal_level(brain_t brain) {
    if (!brain || !brain->medulla_enabled || !brain->medulla) {
        return 0.5f;  /* Default to neutral arousal */
    }

    medulla_stats_t stats;
    if (medulla_get_stats(brain->medulla, &stats) != NIMCP_SUCCESS) {
        return 0.5f;
    }

    return stats.current_arousal;
}

circadian_phase_t nimcp_brain_get_circadian_phase(brain_t brain) {
    if (!brain || !brain->medulla_enabled || !brain->medulla) {
        return CIRCADIAN_PHASE_MORNING;  /* Default to morning */
    }

    return medulla_get_circadian_phase(brain->medulla);
}

protection_level_t nimcp_brain_get_protection_level(brain_t brain) {
    if (!brain || !brain->medulla_enabled || !brain->medulla) {
        return PROTECTION_LEVEL_NORMAL;  /* Default to normal */
    }

    return medulla_get_protection_level(brain->medulla);
}

bool nimcp_brain_is_medulla_emergency(brain_t brain) {
    if (!brain || !brain->medulla_enabled || !brain->medulla) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_brain_is_medulla_emergency: invalid parameters");

            return false;
    }

    medulla_stats_t stats;
    if (medulla_get_stats(brain->medulla, &stats) != NIMCP_SUCCESS) {
        return false;
    }

    return (stats.state == MEDULLA_STATE_EMERGENCY ||
            stats.protection_level >= PROTECTION_LEVEL_CRITICAL);
}

medulla_t brain_get_medulla(brain_t brain) {
    if (!brain || !brain->medulla_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_get_medulla: invalid parameters");

            return NULL;
    }
    return brain->medulla;
}

//=============================================================================
// Control Functions
//=============================================================================

int nimcp_brain_trigger_emergency(brain_t brain, const char* reason) {
    if (!brain) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!brain->medulla_enabled || !brain->medulla) {
        NIMCP_LOGGING_WARN("Cannot trigger emergency - medulla not enabled");
        return NIMCP_ERROR_INVALID_STATE;
    }

    NIMCP_LOGGING_WARN("Brain triggering medulla emergency: %s",
        reason ? reason : "unspecified");

    return medulla_emergency_shutdown(brain->medulla, reason);
}

int nimcp_brain_request_medulla_state(brain_t brain, medulla_state_t new_state) {
    if (!brain) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!brain->medulla_enabled || !brain->medulla) {
        NIMCP_LOGGING_WARN("Cannot change medulla state - medulla not enabled");
        return NIMCP_ERROR_INVALID_STATE;
    }

    return medulla_request_state_change(brain->medulla, new_state);
}

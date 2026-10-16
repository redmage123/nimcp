/**
 * @file nimcp_brain_init_glymphatic.c
 * @brief Glymphatic Subsystem Factory Initialization
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Initialize, update, and destroy glymphatic waste clearance during brain lifecycle
 * WHY:  Provides metabolic waste management — clearance is critical for brain health
 *       and is strongly modulated by sleep/wake state
 * HOW:  Creates glymphatic_system_t during brain init, stores on brain->glymphatic,
 *       updates during brain update cycle, destroys during brain destruction
 *
 * BIOLOGICAL BASIS:
 * The glymphatic system must be initialized after the sleep/wake system and
 * immune system (which it reports to), but before training and inference
 * pipelines (which query it for waste penalties). It operates continuously
 * but is most active during NREM sleep.
 *
 * @author NIMCP Development Team
 */

#include "core/brain/factory/init/nimcp_brain_init_glymphatic.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <time.h>

#define LOG_MODULE "BRAIN_INIT_GLYMPHATIC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_glymphatic, MESH_ADAPTER_CATEGORY_GLIAL)

/*=============================================================================
 * Local Helpers
 *===========================================================================*/

/**
 * @brief Get current monotonic time in microseconds
 */
static uint64_t glymphatic_init_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/*=============================================================================
 * Lifecycle Implementation
 *===========================================================================*/

bool nimcp_brain_factory_init_glymphatic_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_glymphatic_subsystem: brain is NULL");
        return false;
    }

    /* Idempotency: skip if already initialized */
    if (brain->glymphatic) {
        LOG_INFO(LOG_MODULE, "Glymphatic system already initialized, skipping");
        return true;
    }

    /* Create glymphatic system with default configuration */
    glymphatic_config_t cfg = glymphatic_default_config();

    glymphatic_system_t* system = glymphatic_create(&cfg);
    if (!system) {
        LOG_WARN(LOG_MODULE, "Failed to create glymphatic system (non-fatal)");
        return true;  /* Non-fatal: brain works without waste clearance */
    }

    /* Store on brain struct */
    brain->glymphatic = system;
    brain->glymphatic_enabled = true;
    brain->last_glymphatic_update_us = glymphatic_init_get_time_us();

    LOG_INFO(LOG_MODULE, "Glymphatic waste clearance system initialized: "
             "base_clearance=%.4f, aqp4=%.2f, nrem_mult=%.1f",
             cfg.base_clearance_rate,
             cfg.aqp4_expression,
             cfg.nrem_clearance_multiplier);

    return true;
}

int nimcp_brain_update_glymphatic_subsystem(brain_t brain, float delta_time_s) {
    if (!brain) {
        return -1;
    }

    if (!brain->glymphatic || !brain->glymphatic_enabled) {
        return 0;  /* Not initialized or disabled — skip silently */
    }

    return glymphatic_update(brain->glymphatic, delta_time_s);
}

void nimcp_brain_destroy_glymphatic_subsystem(brain_t brain) {
    if (!brain) {
        return;
    }

    if (brain->glymphatic) {
        glymphatic_destroy(brain->glymphatic);
        brain->glymphatic = NULL;
        brain->glymphatic_enabled = false;
        LOG_INFO(LOG_MODULE, "Glymphatic subsystem destroyed");
    }
}

/*=============================================================================
 * Query Functions (via brain handle)
 *===========================================================================*/

float nimcp_brain_get_waste_level(brain_t brain) {
    if (!brain || !brain->glymphatic) {
        return 0.0f;
    }
    return glymphatic_get_waste_level(brain->glymphatic);
}

float nimcp_brain_get_clearance_rate(brain_t brain) {
    if (!brain || !brain->glymphatic) {
        return 0.0f;
    }
    return glymphatic_get_clearance_rate(brain->glymphatic);
}

int nimcp_brain_glymphatic_sleep_transition(brain_t brain, uint32_t sleep_state) {
    if (!brain || !brain->glymphatic) {
        return -1;
    }
    return glymphatic_on_sleep_state_change(brain->glymphatic, sleep_state);
}

int nimcp_brain_glymphatic_flush(brain_t brain) {
    if (!brain || !brain->glymphatic) {
        return -1;
    }
    return glymphatic_flush(brain->glymphatic);
}

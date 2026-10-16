//=============================================================================
// nimcp_brain_init_inferior_colliculus.c - IC Subsystem Initialization
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_inferior_colliculus.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.h"  /* W4 */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_ic, MESH_ADAPTER_CATEGORY_SYSTEM)

//=============================================================================
// Lifecycle
//=============================================================================

bool nimcp_brain_factory_init_inferior_colliculus_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("Cannot init inferior colliculus: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_inferior_colliculus_subsystem: brain is NULL");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    /* Idempotency guard */
    if (b->inferior_colliculus) {
        NIMCP_LOGGING_WARN("Inferior colliculus already initialized");
        return true;
    }

    NIMCP_LOGGING_INFO("Initializing inferior colliculus subsystem...");

    /* Create with default configuration */
    ic_config_t config = ic_default_config();

    /* Adjust config based on brain context */
    if (b->audio_cortex != NULL) {
        /* If audio cortex is present, use more channels for finer resolution */
        config.num_frequency_channels = 128;
        config.frequency_resolution = 0.95f;
    }

    b->inferior_colliculus = ic_create(&config);
    if (!b->inferior_colliculus) {
        NIMCP_LOGGING_ERROR("Failed to create inferior colliculus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_brain_factory_init_inferior_colliculus_subsystem: "
            "failed to create IC");
        return false;
    }

    b->inferior_colliculus_enabled = true;
    b->last_inferior_colliculus_update_us = 0;

    NIMCP_LOGGING_INFO("Inferior colliculus initialized successfully "
                       "(%u frequency channels)",
                       config.num_frequency_channels);

    return true;
}

void nimcp_brain_ic_destroy(brain_t brain) {
    if (!brain) return;

    struct brain_struct* b = (struct brain_struct*)brain;

    if (b->inferior_colliculus) {
        ic_destroy(b->inferior_colliculus);
        b->inferior_colliculus = NULL;
        b->inferior_colliculus_enabled = false;
        NIMCP_LOGGING_INFO("Inferior colliculus destroyed");
    }
}

int nimcp_brain_ic_step(brain_t brain, float dt_s) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_ic_step: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->inferior_colliculus_enabled || !b->inferior_colliculus) {
        return 0;  /* Not enabled, skip silently */
    }

    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_ic_step: invalid dt_s");
        return -1;
    }

    int result = ic_update(b->inferior_colliculus, dt_s);
    if (result == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        b->last_inferior_colliculus_update_us =
            (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
    }

    return result;
}

int nimcp_brain_ic_process_audio(brain_t brain,
                                  const float* left,
                                  const float* right,
                                  uint32_t num_samples) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_ic_process_audio: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->inferior_colliculus_enabled || !b->inferior_colliculus) {
        NIMCP_LOGGING_WARN("IC not enabled for audio processing");
        return -1;
    }

    int rc = ic_process_audio(b->inferior_colliculus, left, right, num_samples);
    if (rc == 0) {
        /* W4: emit auditory localization KG event. Azimuth/elevation not
         * surfaced by ic_process_audio; use 0.0 placeholders — W5 network-level
         * wiring will pass real values when IC internal state exposes them. */
        subcortical_emit_auditory_localization(brain, 0.0f, 0.0f);
    }
    return rc;
}

bool nimcp_brain_ic_is_enabled(brain_t brain) {
    if (!brain) return false;
    struct brain_struct* b = (struct brain_struct*)brain;
    return b->inferior_colliculus_enabled && (b->inferior_colliculus != NULL);
}

inferior_colliculus_t* nimcp_brain_ic_get_handle(brain_t brain) {
    if (!brain) return NULL;
    struct brain_struct* b = (struct brain_struct*)brain;
    return b->inferior_colliculus;
}

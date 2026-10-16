/**
 * @file nimcp_brain_init_entorhinal.c
 * @brief MINIMAL IMPLEMENTATION — create/destroy brain->entorhinal.
 *
 * This is scaffolding so the Wave 8B-c tick driver has a real adapter
 * to operate on. Grid-cell dynamics are NOT simulated here — this just
 * calls entorhinal_adapter_create() with a default config and stores the
 * result on brain->entorhinal.
 *
 * Wired into nimcp_brain_parallel_init.c adjacent to the hypothalamus
 * init block (Wave 15 serial).
 */

#include "core/brain/factory/init/nimcp_brain_init_entorhinal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_adapter.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_ENTORHINAL"

bool nimcp_brain_factory_init_entorhinal_subsystem(brain_t brain) {
    if (!brain) return false;

    /* Idempotent: if already created, nothing to do. */
    if (brain->entorhinal) {
        return true;
    }

    LOG_INFO(LOG_MODULE, "Initializing entorhinal subsystem (minimal impl)");

    entorhinal_adapter_config_t cfg = entorhinal_adapter_default_config();

    brain->entorhinal = entorhinal_adapter_create(&cfg);
    if (!brain->entorhinal) {
        LOG_WARNING(LOG_MODULE,
                    "entorhinal_adapter_create returned NULL — leaving disabled");
        brain->entorhinal_enabled = false;
        return false;
    }

    brain->entorhinal_enabled = true;
    LOG_INFO(LOG_MODULE, "Entorhinal subsystem initialized");
    return true;
}

void nimcp_brain_factory_destroy_entorhinal_subsystem(brain_t brain) {
    if (!brain) return;

    if (brain->entorhinal) {
        entorhinal_adapter_destroy(brain->entorhinal);
        brain->entorhinal = NULL;
    }
    brain->entorhinal_enabled = false;
}

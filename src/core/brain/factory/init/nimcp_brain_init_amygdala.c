/**
 * @file nimcp_brain_init_amygdala.c
 * @brief Amygdala subsystem — factory init (Phase 3c).
 *
 * Creates the amygdala fear/emotional-salience module on brain startup.
 * Independent module: does not depend on other cognitive subsystems.
 *
 * @see include/core/brain/subcortical/nimcp_amygdala.h
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "utils/logging/nimcp_logging.h"

#include <stdbool.h>
#include <stddef.h>

bool nimcp_brain_factory_init_amygdala_subsystem(brain_t brain) {
    if (!brain) return false;
    if (brain->amygdala) return true;  /* idempotent */

    amyg_config_t cfg;
    if (amygdala_default_config(&cfg) != 0) {
        NIMCP_LOGGING_WARN("brain_init_amygdala: default config unavailable; "
                           "amygdala disabled");
        return false;
    }

    amygdala_t* amy = amygdala_create(&cfg);
    if (!amy) {
        NIMCP_LOGGING_ERROR("brain_init_amygdala: amygdala_create failed");
        return false;
    }
    brain->amygdala = (void*)amy;
    brain->amygdala_enabled = true;
    NIMCP_LOGGING_INFO("brain_init_amygdala: amygdala module initialized");
    return true;
}

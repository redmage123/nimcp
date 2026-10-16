/**
 * @file nimcp_brain_init_ofc.c
 * @brief Orbitofrontal cortex (OFC) subsystem initialization.
 *
 * WHAT: Creates the OFC adapter on brain_t with default config so the
 *       grounded_language regional bridge can attach to it.
 *
 * WHY:  Pre-wave, src/core/brain/regions/ofc/ existed but no `brain->ofc`
 *       field was wired. The 5th anatomical region needed for
 *       value/reward language tagging was a complete statue.
 *
 * HOW:  Mirror the cingulate / prefrontal init pattern: create with
 *       defaults + opt-out via config flag. Bridges (immune, KG,
 *       quantum) deferred — first wave only stands up the adapter.
 */

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/ofc/nimcp_ofc.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "BRAIN_INIT_OFC"

bool nimcp_brain_factory_init_ofc_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_ofc_subsystem: brain is NULL");
        return false;
    }

    if (brain->ofc) return true;     /* idempotent */

    /* Reuse executive_control as the gating flag — OFC is part of the
     * executive value-evaluation circuit. Same proxy that prefrontal
     * uses, keeps the config surface unchanged. */
    if (!brain->config.enable_executive_control) {
        brain->ofc_enabled = false;
        return true;                 /* not enabled, not an error */
    }

    LOG_INFO(LOG_MODULE, "Initializing orbitofrontal cortex subsystem");

    ofc_config_t cfg;
    if (ofc_default_config(&cfg) != 0) {
        LOG_WARN(LOG_MODULE, "ofc_default_config failed");
        return false;
    }

    brain->ofc = ofc_create(&cfg);
    if (!brain->ofc) {
        LOG_ERROR(LOG_MODULE, "ofc_create failed");
        return false;
    }

    if (ofc_init(brain->ofc) != 0) {
        LOG_WARN(LOG_MODULE, "ofc_init returned non-zero (continuing)");
    }

    brain->ofc_enabled = true;
    return true;
}

bool nimcp_brain_factory_shutdown_ofc_subsystem(brain_t brain) {
    if (!brain) return false;
    if (brain->ofc) {
        ofc_destroy(brain->ofc);
        brain->ofc = NULL;
    }
    brain->ofc_enabled = false;
    return true;
}

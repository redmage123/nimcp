/**
 * @file nimcp_brain_init_brainstem.c
 * @brief Implementation of brainstem factory initialization
 *
 * WHAT: Brain factory initialization for full brainstem integration
 * WHY:  Provides unified brainstem initialization during brain_create()
 * HOW:  Initializes brainstem adapter, quantum bridge, and connections
 *
 * @version Phase BS-3: Brainstem Factory Integration
 * @date 2025-12-30
 */

#include "core/brain/factory/init/nimcp_brain_init_brainstem.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"
#include "core/brain/regions/brainstem/nimcp_brainstem_quantum_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_brainstem, MESH_ADAPTER_CATEGORY_SYSTEM)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define BRAINSTEM_INIT_LOG_MODULE "BRAIN_INIT_BRAINSTEM"

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

bool nimcp_brain_init_brainstem(struct brain_struct* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_init_brainstem: brain is NULL");
        return false;
    }

    LOG_INFO("[%s] Initializing brainstem subsystem", BRAINSTEM_INIT_LOG_MODULE);

    /* Check if already initialized */
    if (brain->brainstem != NULL) {
        LOG_WARN("[%s] Brainstem already initialized", BRAINSTEM_INIT_LOG_MODULE);
        return true;
    }

    /* Prepare brainstem configuration */
    brainstem_config_t bs_config = brainstem_default_config();

    /* Use external medulla if available */
    medulla_t external_medulla = NULL;
    if (brain->medulla_enabled && brain->medulla != NULL) {
        external_medulla = brain->medulla;
        bs_config.use_external_medulla = true;
        LOG_DEBUG("[%s] Using existing medulla instance", BRAINSTEM_INIT_LOG_MODULE);
    } else {
        bs_config.use_external_medulla = false;
        LOG_DEBUG("[%s] Brainstem will create internal medulla", BRAINSTEM_INIT_LOG_MODULE);
    }

    /* Configure based on brain settings */
    bs_config.enable_reflexes = true;
    bs_config.enable_vital_monitoring = true;
    bs_config.enable_arousal_control = true;
    bs_config.enable_events = brain->enable_event_broadcasting;
    bs_config.enable_bio_async = brain->bio_async_enabled;

    /* Create brainstem adapter */
    brain->brainstem = brainstem_create(&bs_config, external_medulla);
    if (!brain->brainstem) {
        LOG_ERROR("[%s] Failed to create brainstem adapter", BRAINSTEM_INIT_LOG_MODULE);
        brain->brainstem_enabled = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_init_brainstem: brain->brainstem is NULL");
        return false;
    }

    /* Create quantum bridge */
    brainstem_quantum_config_t qconfig = brainstem_quantum_default_config();

    /* Check if brain has quantum resources */
    if (brain->quantum_reasoning_enabled && brain->quantum_reasoner) {
        LOG_DEBUG("[%s] Quantum resources available for brainstem", BRAINSTEM_INIT_LOG_MODULE);
    }

    brain->brainstem_quantum_bridge = brainstem_quantum_bridge_create(
        brain->brainstem, &qconfig);

    if (!brain->brainstem_quantum_bridge) {
        LOG_WARN("[%s] Failed to create quantum bridge (continuing without)",
                 BRAINSTEM_INIT_LOG_MODULE);
    } else {
        /* Connect to brain's quantum reasoner if available */
        if (brain->quantum_reasoning_enabled && brain->quantum_reasoner) {
            brainstem_quantum_bridge_connect_reasoner(
                brain->brainstem_quantum_bridge,
                (brain_qreason_ctx_t*)brain->quantum_reasoner);
            LOG_DEBUG("[%s] Connected quantum reasoner to brainstem",
                      BRAINSTEM_INIT_LOG_MODULE);
        }

        /* Connect to brain's quantum annealer if available */
        if (brain->quantum_annealer) {
            brainstem_quantum_bridge_connect_annealer(
                brain->brainstem_quantum_bridge,
                brain->quantum_annealer);
            LOG_DEBUG("[%s] Connected quantum annealer to brainstem",
                      BRAINSTEM_INIT_LOG_MODULE);
        }
    }

    /* Register default reflexes */
    brainstem_reflex_t startle = {
        .reflex_id = 1,
        .name = "startle",
        .threshold = 0.7f,
        .latency_ms = 20.0f,
        .gain = 1.5f,
        .is_active = true
    };
    brainstem_register_reflex(brain->brainstem, &startle);

    brainstem_reflex_t orienting = {
        .reflex_id = 2,
        .name = "orienting",
        .threshold = 0.3f,
        .latency_ms = 50.0f,
        .gain = 1.0f,
        .is_active = true
    };
    brainstem_register_reflex(brain->brainstem, &orienting);

    brainstem_reflex_t pupillary = {
        .reflex_id = 3,
        .name = "pupillary",
        .threshold = 0.5f,
        .latency_ms = 200.0f,
        .gain = 0.8f,
        .is_active = true
    };
    brainstem_register_reflex(brain->brainstem, &pupillary);

    brainstem_reflex_t vestibulo_ocular = {
        .reflex_id = 4,
        .name = "vestibulo_ocular",
        .threshold = 0.2f,
        .latency_ms = 15.0f,
        .gain = 1.0f,
        .is_active = true
    };
    brainstem_register_reflex(brain->brainstem, &vestibulo_ocular);

    /* Mark as enabled */
    brain->brainstem_enabled = true;
    brain->last_brainstem_update_us = 0;

    LOG_INFO("[%s] Brainstem subsystem initialized successfully", BRAINSTEM_INIT_LOG_MODULE);
    return true;
}

void nimcp_brain_destroy_brainstem(struct brain_struct* brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_destroy_brainstem: brain is NULL"); return; }

    LOG_INFO("[%s] Destroying brainstem subsystem", BRAINSTEM_INIT_LOG_MODULE);

    /* Destroy quantum bridge first */
    if (brain->brainstem_quantum_bridge) {
        brainstem_quantum_bridge_destroy(brain->brainstem_quantum_bridge);
        brain->brainstem_quantum_bridge = NULL;
        LOG_DEBUG("[%s] Quantum bridge destroyed", BRAINSTEM_INIT_LOG_MODULE);
    }

    /* Destroy brainstem adapter */
    if (brain->brainstem) {
        brainstem_destroy(brain->brainstem);
        brain->brainstem = NULL;
        LOG_DEBUG("[%s] Brainstem adapter destroyed", BRAINSTEM_INIT_LOG_MODULE);
    }

    brain->brainstem_enabled = false;
    LOG_INFO("[%s] Brainstem subsystem destroyed", BRAINSTEM_INIT_LOG_MODULE);
}

bool nimcp_brain_is_brainstem_enabled(const struct brain_struct* brain) {
    return brain && brain->brainstem_enabled && brain->brainstem;
}

bool nimcp_brain_connect_brainstem_thalamus(struct brain_struct* brain) {
    if (!brain || !brain->brainstem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_connect_brainstem_thalamus: brain or brainstem is NULL");
        return false;
    }

    /* Check if thalamus is available (via thalamic router) */
    /* This would connect through the middleware routing system */
    LOG_INFO("[%s] Connecting brainstem to thalamus",
             BRAINSTEM_INIT_LOG_MODULE);

    /* Note: Full thalamic connection would require thalamic_router integration */
    /* For now, we mark the connection as established */

    return true;
}

bool nimcp_brain_connect_brainstem_cerebellum(struct brain_struct* brain) {
    if (!brain || !brain->brainstem) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_connect_brainstem_cerebellum: brain or brainstem is NULL");
        return false;
    }

    LOG_INFO("[%s] Connecting brainstem to cerebellum",
             BRAINSTEM_INIT_LOG_MODULE);

    /* The pons provides the corticopontine relay to cerebellum */
    /* This connection enables motor coordination and timing */

    /* Note: Full cerebellar connection would require bg_cerebellar_coord integration */

    return true;
}

bool nimcp_brain_update_brainstem(struct brain_struct* brain, float dt) {
    if (!brain || !brain->brainstem_enabled || !brain->brainstem) {
        return true; /* Not an error if disabled */
    }

    /* Update brainstem adapter */
    if (!brainstem_update(brain->brainstem, dt)) {
        LOG_WARN("[%s] Brainstem update failed", BRAINSTEM_INIT_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_update_brainstem: brainstem_update is NULL");
        return false;
    }

    /* Update quantum bridge if present */
    if (brain->brainstem_quantum_bridge) {
        brainstem_quantum_bridge_update(brain->brainstem_quantum_bridge, dt);
    }

    /* Synchronize arousal with medulla if both are present */
    if (brain->medulla_enabled && brain->medulla) {
        float brainstem_arousal = brainstem_get_arousal_value(brain->brainstem);
        float medulla_arousal = medulla_get_arousal_level(brain->medulla);

        /* Weighted average to keep them synchronized */
        if (medulla_arousal >= 0.0f) {
            float diff = medulla_arousal - brainstem_arousal;
            if (fabsf(diff) > 0.1f) {
                if (diff > 0) {
                    brainstem_boost_arousal(brain->brainstem, diff * 0.1f);
                } else {
                    brainstem_reduce_arousal(brain->brainstem, -diff * 0.1f);
                }
            }
        }
    }

    /* Update timestamp */
    brain->last_brainstem_update_us = brain->current_time_us;

    return true;
}

//=============================================================================
// nimcp_creative_orchestrator.c - Creative System Orchestrator Implementation
//=============================================================================
/**
 * @file nimcp_creative_orchestrator.c
 * @brief Master orchestrator for creative/artistic cognitive system
 *
 * WHAT: Coordinates all creative subsystems for artistic cognition
 * WHY:  Unified interface for appreciation and generation capabilities
 * HOW:  Manages lifecycle and routing between creative components
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/nimcp_creative_orchestrator.h"
#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/appreciation/nimcp_aesthetic_evaluation.h"
#include "cognitive/creative/bridges/nimcp_creative_bridge.h"
#include "cognitive/creative/generation/nimcp_text_generation.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>
#include <time.h>

#define LOG_MODULE "CREATIVE_ORCH"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE(creative_orchestrator, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Lifecycle API
//=============================================================================

creative_orchestrator_t* creative_orchestrator_create(
    const creative_config_t* config) {

    creative_orchestrator_t* orch = nimcp_calloc(1, sizeof(creative_orchestrator_t));
    if (!orch) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_orchestrator_create: allocation failed");
        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        memcpy(&orch->config, config, sizeof(creative_config_t));
    } else {
        creative_config_init_defaults(&orch->config);
    }

    /* Initialize statistics */
    memset(&orch->stats, 0, sizeof(creative_orchestrator_stats_t));

    /* Initialize state */
    orch->state = CREATIVE_STATE_INITIALIZING;

    LOG_INFO(LOG_MODULE, "Creative orchestrator created");

    creative_orchestrator_heartbeat("orchestrator_create", 1.0f);

    orch->state = CREATIVE_STATE_READY;
    return orch;
}

void creative_orchestrator_destroy(creative_orchestrator_t* orch) {
    if (!orch) return;

    orch->state = CREATIVE_STATE_SHUTDOWN;

    /* Destroy subsystems */
    if (orch->text_gen) {
        text_generator_destroy(orch->text_gen);
        orch->text_gen = NULL;
    }
    if (orch->creative_bridge) {
        creative_bridge_destroy(orch->creative_bridge);
        orch->creative_bridge = NULL;
    }
    if (orch->aesthetic_eval) {
        aesthetic_evaluator_destroy(orch->aesthetic_eval);
        orch->aesthetic_eval = NULL;
    }

    /* P3-COG-03: Log before free to avoid use-after-free */
    LOG_INFO(LOG_MODULE, "Creative orchestrator destroyed");
    nimcp_free(orch);
    orch = NULL;
}

int creative_orchestrator_init_subsystems(creative_orchestrator_t* orch) {
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_orchestrator_init_subsystems: orch is NULL");
        return -1;
    }

    creative_orchestrator_heartbeat("init_subsystems", 0.0f);

    orch->state = CREATIVE_STATE_INITIALIZING;
    int failures = 0;

    /* Initialize aesthetic evaluator (appreciation subsystem) */
    if (!orch->aesthetic_eval) {
        orch->aesthetic_eval = aesthetic_evaluator_create(NULL);
        if (!orch->aesthetic_eval) {
            LOG_WARN(LOG_MODULE, "Failed to create aesthetic evaluator");
            failures++;
        }
    }

    /* Initialize creative bridge (validation subsystem) */
    if (!orch->creative_bridge) {
        orch->creative_bridge = creative_bridge_create(NULL);
        if (!orch->creative_bridge) {
            LOG_WARN(LOG_MODULE, "Failed to create creative bridge");
            failures++;
        }
    }

    /* Initialize text generator */
    if (!orch->text_gen) {
        text_generator_config_t tg_cfg;
        text_generator_config_defaults(&tg_cfg);
        orch->text_gen = text_generator_create(&tg_cfg);
        if (!orch->text_gen) {
            LOG_WARN(LOG_MODULE, "Failed to create text generator");
            failures++;
        }
    }

    /* Wire evaluator into text generator if both exist */
    if (orch->text_gen && orch->aesthetic_eval) {
        text_generator_set_evaluator(orch->text_gen, orch->aesthetic_eval);
    }
    if (orch->text_gen && orch->creative_bridge) {
        text_generator_set_bridge(orch->text_gen, orch->creative_bridge);
    }

    orch->state = CREATIVE_STATE_READY;
    LOG_INFO(LOG_MODULE, "Creative subsystems initialized (%d failures)", failures);

    creative_orchestrator_heartbeat("init_subsystems", 1.0f);

    return (failures > 0) ? -1 : 0;
}

void creative_orchestrator_shutdown(creative_orchestrator_t* orch) {
    if (!orch) return;

    orch->state = CREATIVE_STATE_SHUTDOWN;
}

//=============================================================================
// Update API
//=============================================================================

int creative_orchestrator_update(creative_orchestrator_t* orch, uint64_t dt_us) {
    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_orchestrator_update: orch is NULL");
        return -1;
    }

    creative_orchestrator_heartbeat("orchestrator_update", 0.0f);

    /* Update statistics */
    orch->stats.update_cycles++;
    orch->stats.last_update_us = dt_us;

    creative_orchestrator_heartbeat("orchestrator_update", 1.0f);

    return 0;
}

creative_orchestrator_state_t creative_orchestrator_get_state(
    const creative_orchestrator_t* orch) {
    if (!orch) return CREATIVE_STATE_UNINITIALIZED;
    return orch->state;
}

int creative_orchestrator_get_stats(creative_orchestrator_t* orch,
                                     creative_orchestrator_stats_t* out) {
    if (!orch || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_orchestrator_get_stats: required parameter is NULL (orch, out)");
        return -1;
    }
    /* P2: Copy stats under mutex to prevent torn reads */
    if (orch->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)orch->mutex);
    }
    *out = orch->stats;
    if (orch->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)orch->mutex);
    }
    return 0;
}

void creative_orchestrator_reset_stats(creative_orchestrator_t* orch) {
    if (!orch) return;
    memset(&orch->stats, 0, sizeof(creative_orchestrator_stats_t));
}

//=============================================================================
// Brain Integration API
//=============================================================================

void creative_orchestrator_set_brain(creative_orchestrator_t* orch, void* brain) {
    if (!orch) return;
    orch->brain = brain;
}

void creative_orchestrator_set_emotion_system(creative_orchestrator_t* orch,
                                               void* emotion) {
    if (!orch) return;
    orch->emotion_system = emotion;
}

void creative_orchestrator_set_hippocampus(creative_orchestrator_t* orch,
                                            void* hippo) {
    if (!orch) return;
    orch->hippocampus = hippo;
}

void creative_orchestrator_set_ethics_engine(creative_orchestrator_t* orch,
                                              void* ethics) {
    if (!orch) return;
    orch->ethics_engine = ethics;
}

void creative_orchestrator_set_immune_system(creative_orchestrator_t* orch,
                                              void* immune) {
    if (!orch) return;
    orch->immune_system = immune;
}

void creative_orchestrator_set_temporal_lobe(creative_orchestrator_t* orch,
                                              void* temporal) {
    if (!orch) return;
    orch->temporal_lobe = temporal;
}

void creative_orchestrator_set_vae_system(creative_orchestrator_t* orch,
                                           void* vae) {
    if (!orch) return;
    orch->vae_system = vae;
}

//=============================================================================
// Component Accessors
//=============================================================================

aesthetic_evaluator_t* creative_orchestrator_get_aesthetic_eval(
    creative_orchestrator_t* orch) {
    return orch ? orch->aesthetic_eval : NULL;
}

style_representer_t* creative_orchestrator_get_style_repr(
    creative_orchestrator_t* orch) {
    return orch ? orch->style_repr : NULL;
}

text_generator_t* creative_orchestrator_get_text_gen(
    creative_orchestrator_t* orch) {
    return orch ? orch->text_gen : NULL;
}

music_generator_t* creative_orchestrator_get_music_gen(
    creative_orchestrator_t* orch) {
    return orch ? orch->music_gen : NULL;
}

visual_generator_t* creative_orchestrator_get_visual_gen(
    creative_orchestrator_t* orch) {
    return orch ? orch->visual_gen : NULL;
}

diffusion_bridge_t* creative_orchestrator_get_diffusion_bridge(
    creative_orchestrator_t* orch) {
    return orch ? orch->diffusion_bridge : NULL;
}

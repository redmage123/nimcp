/**
 * @file nimcp_brain_init_cognitive_engines.c
 * @brief Factory initialization for cognitive engine modules
 *
 * Initializes: Recursive Cognition, Inner Dialogue, Reasoning, Imagination
 * These modules are wired into brain_decide() for inference-time cognition.
 */

#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"

/* Slice B (Option 1 architectural rebuild). Cross-modal population
 * binding registry. */
#include "language/nimcp_concept_registry.h"

/* Imagination Engine: forward declarations only to avoid type conflicts
 * (nimcp_imagination_engine.h re-typedefs audio_cortex_t, visual_training_state_t,
 * etc. which conflict with already-included definitions from brain_internal.h).
 * We only need create/destroy + brain field assignment here. */
typedef struct imagination_engine imagination_engine_t;
typedef struct imagination_engine_config imagination_engine_config_t;
extern imagination_engine_t* imagination_engine_create(
    const imagination_engine_config_t* config);
extern void imagination_engine_destroy(imagination_engine_t* engine);

#define LOG_MODULE "BRAIN_INIT"

/*=============================================================================
 * Recursive Cognition Engine
 *===========================================================================*/

bool nimcp_brain_factory_init_rcog_engine_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_rcog_engine_subsystem: brain is NULL");
        return false;
    }

    brain->rcog_engine = NULL;
    brain->rcog_engine_enabled = false;

    /* Create with defaults */
    rcog_engine_t* engine = rcog_engine_create_default();
    if (!engine) {
        LOG_WARN(LOG_MODULE, "Failed to create recursive cognition engine (non-fatal)");
        return true;  /* Non-fatal — brain works without it */
    }

    /* Initialize engine */
    if (rcog_engine_init(engine) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to init recursive cognition engine (non-fatal)");
        rcog_engine_destroy(engine);
        return true;
    }

    /* Start engine (begin accepting goals) */
    if (rcog_engine_start(engine) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to start recursive cognition engine (non-fatal)");
        rcog_engine_destroy(engine);
        return true;
    }

    brain->rcog_engine = engine;
    brain->rcog_engine_enabled = true;

    LOG_INFO(LOG_MODULE, "Recursive cognition engine enabled for brain '%s'",
             brain->config.task_name);
    return true;
}

/*=============================================================================
 * Inner Dialogue Engine
 *===========================================================================*/

bool nimcp_brain_factory_init_inner_dialogue_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_inner_dialogue_subsystem: brain is NULL");
        return false;
    }

    brain->inner_dialogue = NULL;
    brain->inner_dialogue_enabled = false;

    /* Create with defaults */
    inner_dialogue_engine_t* engine = inner_dialogue_engine_create(NULL);
    if (!engine) {
        LOG_WARN(LOG_MODULE, "Failed to create inner dialogue engine (non-fatal)");
        return true;
    }

    /* Connect brain so perspectives can access cognitive modules */
    inner_dialogue_engine_connect_brain(engine, brain);

    /* Register the 7 built-in perspectives so deliberation has voices */
    inner_dialogue_perspective_registry_t* registry =
        inner_dialogue_engine_get_registry(engine);
    if (registry) {
        inner_dialogue_register_builtin_perspectives(registry);
    }

    brain->inner_dialogue = engine;
    brain->inner_dialogue_enabled = true;

    LOG_INFO(LOG_MODULE, "Inner dialogue engine enabled for brain '%s'",
             brain->config.task_name);
    return true;
}

/*=============================================================================
 * Reasoning Engine
 *===========================================================================*/

bool nimcp_brain_factory_init_reasoning_engine_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_reasoning_engine_subsystem: brain is NULL");
        return false;
    }

    brain->reasoning_engine = NULL;
    brain->reasoning_engine_enabled = false;

    /* Create with defaults */
    reasoning_engine_t* engine = reasoning_engine_create(NULL);
    if (!engine) {
        LOG_WARN(LOG_MODULE, "Failed to create reasoning engine (non-fatal)");
        return true;
    }

    /* Connect to brain so reasoning can access subsystems */
    reasoning_engine_connect_brain(engine, brain);

    brain->reasoning_engine = engine;
    brain->reasoning_engine_enabled = true;

    LOG_INFO(LOG_MODULE, "Reasoning engine enabled for brain '%s'",
             brain->config.task_name);
    return true;
}

/*=============================================================================
 * Imagination Engine
 *===========================================================================*/

bool nimcp_brain_factory_init_imagination_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_imagination_subsystem: brain is NULL");
        return false;
    }

    brain->imagination = NULL;
    brain->imagination_enabled = false;

    /* Create with defaults */
    imagination_engine_t* engine = imagination_engine_create(NULL);
    if (!engine) {
        LOG_WARN(LOG_MODULE, "Failed to create imagination engine (non-fatal)");
        return true;
    }

    /* NOTE: Cannot set engine->brain here because imagination_engine_t is
     * an incomplete type (forward-declared to avoid header conflicts).
     * The brain_decide() code uses brain->imagination as an opaque handle
     * and passes it to imagination_begin_scenario() which has internal
     * access to the full engine struct. */

    brain->imagination = engine;
    brain->imagination_enabled = true;

    LOG_INFO(LOG_MODULE, "Imagination engine enabled for brain '%s'",
             brain->config.task_name);
    return true;
}

/*=============================================================================
 * Concept Registry (Slice B — Option 1 architectural rebuild)
 *
 * Allocates a registry mapping (text label, visual feature digest, audio
 * feature digest) → canonical concept_pop_id_t. Wired into brain_learn_vector
 * so that supervised (features, label) training installs the (modality,
 * label) binding, enforcing the Quian Quiroga single-pop-per-referent
 * invariant.
 *
 * Non-fatal: if creation fails the brain still works, just without the
 * cross-modal binding hook (downstream callers NULL-check brain->concept_registry).
 *===========================================================================*/

bool nimcp_brain_factory_init_concept_registry_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_concept_registry_subsystem: brain is NULL");
        return false;
    }

    brain->concept_registry = NULL;

    /* Initial capacity: 1024 is plenty of headroom for stage-0/1 vocab.
     * The table auto-grows; this is just a sizing hint. */
    concept_registry_t* reg = concept_registry_create(1024);
    if (!reg) {
        LOG_WARN(LOG_MODULE, "Failed to create concept registry (non-fatal)");
        return true;  /* non-fatal — brain still trains */
    }

    brain->concept_registry = reg;

    LOG_INFO(LOG_MODULE, "Concept registry enabled for brain '%s'",
             brain->config.task_name);
    return true;
}

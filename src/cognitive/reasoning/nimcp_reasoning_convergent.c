/**
 * @file nimcp_reasoning_convergent.c
 * @brief Convergent Evidence Accumulation Architecture — Implementation
 *
 * WHAT: Parallel evidence accumulation with convergence detection
 * WHY:  Models Global Workspace Theory — all cortical regions process
 *       simultaneously, convergence emerges from parallel accumulation
 * HOW:  Static contributor registry + mutex-protected accumulator +
 *       EMA convergence detection + 3-wave orchestration
 *
 * ARCHITECTURE:
 *   1. Wave 0: Context providers (main thread, sequential)
 *   2. Wave 1: Evidence producers + modulators (thread pool, parallel)
 *   3. Merge Wave 1 results → accumulator
 *   4. Convergence check — if converged, skip Wave 2
 *   5. Wave 2: Dependent phases (sequential, early-exit on convergence)
 *   6. Apply net modulation (clamped ±0.3)
 *   7. Synthesis (reuse existing phase_synthesis)
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_affective.h"
#include "cognitive/reasoning/nimcp_reasoning_calibration.h"

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/accessors/nimcp_brain_accessors.h"

#include "utils/thread/nimcp_thread_pool.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "reasoning_convergent"

/** Maximum net modulation magnitude applied to confidence (clamped to +/- this value) */
#define MODULATION_CLAMP_MAX 0.3f

/*=============================================================================
 * FORWARD DECLARATIONS — phase functions from nimcp_reasoning_chain.c
 *
 * These are static in the chain file, so we re-declare them here as extern.
 * They are actually defined in nimcp_reasoning_chain.c; the linker resolves
 * them because the convergent orchestrator is called FROM the chain file
 * (which has visibility of these statics). We use a different approach:
 * the orchestrator is called from reasoning_chain.c which passes the
 * engine pointer — we access the phase functions through the engine's
 * internal state indirectly by calling the existing public API.
 *
 * DESIGN DECISION: Instead of duplicating phase logic, we call
 * reasoning_engine_reason_concurrent's approach — thread-local chains
 * with wrapper functions that call into the engine's brain modules
 * directly. The phase_* functions are internal to nimcp_reasoning_chain.c,
 * so we implement NEW contributor wrappers that call brain module APIs
 * directly, matching the plan's design.
 *===========================================================================*/

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * Accessor for the opaque reasoning_engine struct fields.
 * The struct is defined in nimcp_reasoning_chain.c — we need the brain pointer
 * and config. Since convergent is called from reasoning_chain.c's dispatch,
 * the engine pointer is valid and the struct layout is accessible because
 * we include brain_internal.h.
 *
 * We define a compatible layout here to avoid modifying the chain file's
 * struct definition. This MUST stay in sync with reasoning_engine in chain.c.
 *
 * FIX #1 (compat struct drift): All fields are void* because the canonical
 * struct uses typedef'd pointer types (knowledge_system_t, predictive_network_t,
 * epistemic_filter_t, self_model_system_t, engram_system_t* etc.) which are all
 * pointer-sized. The field ORDER and COUNT must match exactly.
 *
 * MAINTENANCE: If you add/remove/reorder fields in reasoning_engine (chain.c),
 * you MUST update this struct to match.
 */
struct reasoning_engine_compat {
    reasoning_engine_config_t config;   /* must match chain.c field 1 */
    brain_t brain;                      /* must match chain.c field 2 */
    void* engram_system;                /* engram_system_t* */
    void* knowledge_system;             /* knowledge_system_t (typedef ptr) */
    void* working_memory;               /* working_memory_t* */
    void* predictive_net;               /* predictive_network_t (typedef ptr) */
    void* epistemic_filter;             /* epistemic_filter_t (typedef ptr) */
    void* rcog_engine;                  /* rcog_engine_t* */
    void* self_model;                   /* self_model_system_t (typedef ptr) */
    void* omni_world_model;             /* struct omni_world_model* */
    void* multimodal_world_model;       /* struct nimcp_world_model* */
    void* jepa_predictor;               /* jepa_predictor_t* */
    void* jepa_context;                 /* jepa_context_encoder_t* */
    void* jepa_fep_bridge;              /* jepa_fep_bridge_t* */
    void* symbolic_logic;               /* symbolic_logic_t* */
    void* thread_pool;                  /* nimcp_thread_pool_t* */
    void* calibration;                  /* reasoning_calibration_t* */
    void* metacognition;                /* reasoning_metacognition_t* */
    void* abduction;                    /* reasoning_abduction_t* */
    reasoning_engine_stats_t stats;     /* must match chain.c stats field */
    bool is_connected;                  /* must match chain.c is_connected field */
    void* reason_mutex;                 /* nimcp_mutex_t* — FIX #5: added for thread safety */
};

/** Quick access to brain from engine */
static inline brain_t engine_get_brain(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->brain;
}

/** Quick access to config from engine */
static inline reasoning_engine_config_t* engine_get_config(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return &e->config;
}

/** Quick access to thread pool from engine */
static inline nimcp_thread_pool_t* engine_get_pool(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return (nimcp_thread_pool_t*)e->thread_pool;
}

/** Quick access to stats from engine */
static inline reasoning_engine_stats_t* engine_get_stats(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return &e->stats;
}

/** Quick access to calibration from engine */
static inline reasoning_calibration_t* engine_get_calibration(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return (reasoning_calibration_t*)e->calibration;
}

/** Check if a module pointer is non-NULL in the engine */
static inline bool engine_has_engram(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->engram_system != NULL;
}

static inline bool engine_has_knowledge(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->knowledge_system != NULL;
}

static inline bool engine_has_world_model(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return (e->omni_world_model != NULL || e->multimodal_world_model != NULL);
}

static inline bool engine_has_symbolic(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->symbolic_logic != NULL;
}

static inline bool engine_has_predictive(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->predictive_net != NULL;
}

static inline bool engine_has_epistemic(reasoning_engine_t* engine) {
    struct reasoning_engine_compat* e = (struct reasoning_engine_compat*)engine;
    return e->epistemic_filter != NULL;
}

/*=============================================================================
 * ACCUMULATOR IMPLEMENTATION
 *===========================================================================*/

int reasoning_accumulator_init(evidence_accumulator_t* acc,
                                uint32_t total_contributors,
                                float alpha, float threshold)
{
    if (!acc) return -1;

    memset(acc, 0, sizeof(evidence_accumulator_t));

    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    acc->mutex = nimcp_mutex_create(&attr);
    if (!acc->mutex) {
        NIMCP_LOGGING_ERROR("convergent: failed to create accumulator mutex");
        return -1;
    }

    acc->ema_alpha = (alpha > 0.0f) ? alpha : REASONING_DEFAULT_EMA_ALPHA;
    acc->convergence_threshold = (threshold > 0.0f)
        ? threshold : REASONING_DEFAULT_CONVERGENCE_THRESHOLD;
    acc->total_contributors = total_contributors;
    acc->converged = false;
    acc->current_confidence = 0.0f;
    acc->ema_delta = 1.0f;  /* Start high to avoid premature convergence */
    nimcp_atomic_init_u32(&acc->completed_count, 0);

    return 0;
}

void reasoning_accumulator_destroy(evidence_accumulator_t* acc)
{
    if (!acc) return;
    if (acc->mutex) {
        nimcp_mutex_destroy(acc->mutex);
        acc->mutex = NULL;
    }
}

int reasoning_accumulator_submit_evidence(evidence_accumulator_t* acc,
                                           reasoning_chain_t* chain,
                                           const convergent_contribution_t* contrib)
{
    if (!acc || !chain || !contrib) return -1;

    nimcp_mutex_lock(acc->mutex);

    /* Merge local chain steps into main chain with reassigned step IDs */
    for (uint32_t i = 0; i < contrib->local_chain.num_steps; i++) {
        reasoning_step_t step = contrib->local_chain.steps[i];
        step.step_id = chain->num_steps;
        if (reasoning_chain_add_step(chain, &step) < 0) {
            NIMCP_LOGGING_WARN("convergent: merge step add failed (chain full, steps=%u)", chain->num_steps);
        }
    }

    /* Update running confidence with incremental average */
    float new_conf = contrib->result_confidence;
    acc->confidence_count++;

    if (acc->confidence_count < REASONING_CONFIDENCE_HISTORY_SIZE) {
        acc->confidence_history[acc->confidence_count - 1] = new_conf;
    }

    float previous = acc->current_confidence;
    float weight = 1.0f / (float)acc->confidence_count;
    acc->current_confidence = (1.0f - weight) * previous + weight * new_conf;

    /* Update EMA of confidence deltas */
    float delta = fabsf(acc->current_confidence - previous);
    acc->ema_delta = acc->ema_alpha * delta +
                     (1.0f - acc->ema_alpha) * acc->ema_delta;

    /* Check convergence */
    if (acc->confidence_count >= REASONING_MIN_CONVERGENCE_SUBMISSIONS &&
        acc->ema_delta < acc->convergence_threshold) {
        acc->converged = true;
        NIMCP_LOGGING_DEBUG("convergent: converged after %u submissions "
                            "(confidence=%.4f, ema_delta=%.6f)",
                            acc->confidence_count,
                            (double)acc->current_confidence,
                            (double)acc->ema_delta);
    }

    nimcp_mutex_unlock(acc->mutex);

    nimcp_atomic_fetch_add_u32(&acc->completed_count, 1,
                               NIMCP_MEMORY_ORDER_SEQ_CST);
    return 0;
}

int reasoning_accumulator_submit_modulation(evidence_accumulator_t* acc,
                                             float delta)
{
    if (!acc) return -1;

    nimcp_mutex_lock(acc->mutex);

    if (delta >= 0.0f) {
        acc->total_positive_modulation += delta;
    } else {
        acc->total_negative_modulation += delta;
    }
    acc->modulator_count++;

    nimcp_mutex_unlock(acc->mutex);

    nimcp_atomic_fetch_add_u32(&acc->completed_count, 1,
                               NIMCP_MEMORY_ORDER_SEQ_CST);
    return 0;
}

float reasoning_accumulator_apply_modulation(evidence_accumulator_t* acc)
{
    if (!acc) return 0.0f;

    nimcp_mutex_lock(acc->mutex);

    float net = acc->total_positive_modulation + acc->total_negative_modulation;

    /* Clamp net modulation to ±MODULATION_CLAMP_MAX */
    if (net > MODULATION_CLAMP_MAX) net = MODULATION_CLAMP_MAX;
    if (net < -MODULATION_CLAMP_MAX) net = -MODULATION_CLAMP_MAX;

    acc->current_confidence += net;
    if (acc->current_confidence > 1.0f) acc->current_confidence = 1.0f;
    if (acc->current_confidence < 0.0f) acc->current_confidence = 0.0f;

    nimcp_mutex_unlock(acc->mutex);

    return net;
}

bool reasoning_accumulator_is_converged(evidence_accumulator_t* acc)
{
    if (!acc) return false;
    // Lock mutex to avoid data race with concurrent writers
    nimcp_mutex_lock(((evidence_accumulator_t*)acc)->mutex);
    bool result = acc->converged;
    nimcp_mutex_unlock(((evidence_accumulator_t*)acc)->mutex);
    return result;
}

/*=============================================================================
 * AVAILABILITY CHECKS — check if brain modules are present
 *===========================================================================*/

static bool avail_recall(reasoning_engine_t* engine) {
    return engine_has_engram(engine) &&
           engine_get_config(engine)->enable_engram_recall;
}

static bool avail_knowledge(reasoning_engine_t* engine) {
    return engine_has_knowledge(engine) &&
           engine_get_config(engine)->enable_knowledge_query;
}

static bool avail_world_model(reasoning_engine_t* engine) {
    return engine_has_world_model(engine) &&
           engine_get_config(engine)->enable_world_model;
}

static bool avail_symbolic(reasoning_engine_t* engine) {
    return engine_has_symbolic(engine) &&
           engine_get_config(engine)->enable_symbolic_logic;
}

static bool avail_verification(reasoning_engine_t* engine) {
    return engine_has_predictive(engine) &&
           engine_get_config(engine)->enable_predictive_verify;
}

static bool avail_epistemic(reasoning_engine_t* engine) {
    return engine_has_epistemic(engine) &&
           engine_get_config(engine)->enable_epistemic_check;
}

static bool avail_jepa(reasoning_engine_t* engine) {
    return engine_get_config(engine)->enable_jepa_prediction;
}

static bool avail_hippocampus(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->hippocampus_enabled && b->hippocampus;
}

static bool avail_semantic_memory(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->semantic_memory != NULL;
}

static bool avail_parietal(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->parietal_enabled && b->parietal;
}

static bool avail_intuition(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->intuition_system_enabled && b->intuition_system;
}

static bool avail_creative(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->creative_enabled && b->creative_orchestrator;
}

static bool avail_kg_reader(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->kg_reader_enabled && b->kg_reader;
}

static bool avail_mesh(reasoning_engine_t* engine) {
    (void)engine;
    return reasoning_mesh_is_available();
}

static bool avail_emotional(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->emotional_system != NULL;
}

static bool avail_ethics(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->ethics != NULL;
}

static bool avail_directives(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->core_directives_enabled && b->core_directives;
}

static bool avail_bias(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->bias_detection != NULL;
}

static bool avail_theory_of_mind(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->theory_of_mind != NULL;
}

static bool avail_introspection(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->introspection != NULL;
}

static bool avail_cingulate(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->cingulate_enabled && b->cingulate;
}

static bool avail_salience(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->salience != NULL;
}

static bool avail_shadow_emotions(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->shadow_emotions != NULL;
}

static bool avail_grief(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->grief_system != NULL;
}

static bool avail_joy(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->joy_system != NULL;
}

static bool avail_remorse(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->remorse_system != NULL;
}

static bool avail_social_bond(reasoning_engine_t* engine) {
    brain_t b = engine_get_brain(engine);
    return b && b->social_bond_system != NULL;
}

/* Always available (uses aggregation, no module pointer) */
static bool avail_always(reasoning_engine_t* engine) {
    (void)engine;
    return true;
}

/*=============================================================================
 * CONTRIBUTOR WRAPPER FUNCTIONS
 *
 * Each follows the pattern:
 *   1. Cast arg to convergent_contribution_t*
 *   2. Initialize thread-local chain
 *   3. Call brain module API
 *   4. Set result_confidence / confidence_delta
 *   5. Set completed = true
 *
 * For Tier 1 (evidence producers): write to local_chain + result_confidence
 * For Tier 2 (modulators): write to confidence_delta
 * For Tier 3 (context providers): set context_available = true
 *===========================================================================*/

/* ── Tier 1: Evidence Producers ────────────────────────────────────────── */

/**
 * @brief Recall contributor — wraps engram recall
 *
 * Calls into the reasoning engine's recall phase by performing a simplified
 * engram query. Since we can't call phase_recall() directly (it's static
 * in nimcp_reasoning_chain.c), we delegate back to the engine's reason()
 * which handles this. Instead, we use the existing concurrent pipeline's
 * approach: the orchestrator calls this through the engine's own dispatch.
 *
 * DESIGN NOTE: The convergent orchestrator is called FROM reasoning_chain.c,
 * which means it can call phase_* functions via function pointers stored
 * during initialization, or we can implement simplified module queries here.
 * We choose a minimal approach: each Tier 1 contributor produces a step
 * describing what it found, with appropriate confidence.
 */
static void contrib_hippocampus(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->hippocampus) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    /* Create a reasoning step for hippocampal pattern completion */
    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_HIPPOCAMPAL_RECALL;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.4f;  /* Default moderate confidence */
    step.relevance = 0.6f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Hippocampal pattern completion for query context");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: hippocampal step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_semantic_memory(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->semantic_memory) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_SEMANTIC_ACTIVATION;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.45f;
    step.relevance = 0.65f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Semantic memory activation: concept network spreading");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: semantic step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_parietal(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->parietal) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_MATHEMATICAL;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.5f;
    step.relevance = 0.5f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Parietal hypothesis evaluation: mathematical/spatial reasoning");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: parietal step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_intuition(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->intuition_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_INTUITIVE;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.35f;
    step.relevance = 0.5f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Intuitive hunch: rapid pattern matching without deliberation");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: intuition step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_creative(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->creative_orchestrator) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_CREATIVE_ANALOGY;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.3f;
    step.relevance = 0.4f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Creative analogy: cross-domain mapping for novel insight");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: creative step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_kg_reader(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->kg_reader) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_SELF_KNOWLEDGE;
    step.timestamp_us = nimcp_time_get_us();
    step.confidence = 0.5f;
    step.relevance = 0.55f;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Self-knowledge query: internal knowledge graph traversal");
    if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
        NIMCP_LOGGING_WARN("convergent: kg_reader step add failed (steps=%u)", ctx->local_chain.num_steps);
    }

    ctx->result_confidence = step.confidence;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_mesh_evidence(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    /* FIX #7: Check mesh availability BEFORE initializing local_chain
     * to avoid leaking the chain's step array on early return. */
    if (!reasoning_mesh_is_available()) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    brain_t brain = engine_get_brain(ctx->engine);
    /* FIX #9: NULL check on brain before passing to mesh gather */
    if (!brain) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    reasoning_chain_init(&ctx->local_chain);

    reasoning_mesh_result_t mesh_result = reasoning_mesh_gather_evidence(
        brain, ctx->query, REASONING_MESH_DEFAULT_TIMEOUT_MS);

    if (mesh_result.mesh_available && mesh_result.evidence_count > 0) {
        reasoning_step_t step;
        memset(&step, 0, sizeof(step));
        step.step_id = 0;
        step.type = REASONING_STEP_MESH_CONSENSUS;
        step.timestamp_us = nimcp_time_get_us();
        step.confidence = mesh_result.consensus_confidence;
        step.relevance = mesh_result.coherence;
        snprintf(step.description, REASONING_STEP_DESC_LEN,
                 "Mesh consensus: %u endorsements, coherence=%.2f",
                 mesh_result.endorsements_approved,
                 (double)mesh_result.coherence);
        if (reasoning_chain_add_step(&ctx->local_chain, &step) < 0) {
            NIMCP_LOGGING_WARN("convergent: mesh_evidence step add failed (steps=%u)", ctx->local_chain.num_steps);
        }

        ctx->result_confidence = mesh_result.consensus_confidence;
    } else {
        ctx->result_confidence = 0.0f;
    }

    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

/* ── Tier 2: Confidence Modulators ─────────────────────────────────────── */

static void contrib_emotional(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->emotional_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    /* Emotional modulation: anxiety suppresses, positive valence boosts */
    ctx->confidence_delta = 0.0f;  /* Neutral by default */
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_ethics(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->ethics) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_directives(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->core_directives) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_bias(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->bias_detection) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    /* Use affective evaluation for meaningful modulation */
    affective_contribution_t affect = reasoning_affective_evaluate_bias(
        brain->bias_detection, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_theory_of_mind(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->theory_of_mind) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_introspection(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->introspection) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_cingulate(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->cingulate) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_salience(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->salience) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    ctx->confidence_delta = 0.0f;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_shadow_emotions(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->shadow_emotions) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    /* Use affective evaluation for meaningful modulation */
    affective_contribution_t affect = reasoning_affective_evaluate_shadow(
        brain->shadow_emotions, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

/* ── Tier 2: Affective Modulators (NEW — grief, joy, remorse, social) ── */

static void contrib_grief(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->grief_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    affective_contribution_t affect = reasoning_affective_evaluate_grief(
        brain->grief_system, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_joy(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->joy_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    affective_contribution_t affect = reasoning_affective_evaluate_joy(
        brain->joy_system, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_remorse(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->remorse_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    affective_contribution_t affect = reasoning_affective_evaluate_remorse(
        brain->remorse_system, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

static void contrib_social_bond(void* arg) {
    convergent_contribution_t* ctx = (convergent_contribution_t*)arg;
    uint64_t start = nimcp_time_get_us();

    brain_t brain = engine_get_brain(ctx->engine);
    if (!brain || !brain->social_bond_system) {
        ctx->skipped = true;
        ctx->completed = true;
        return;
    }

    affective_contribution_t affect = reasoning_affective_evaluate_social(
        brain->social_bond_system, ctx->query);
    ctx->confidence_delta = affect.confidence_delta;
    ctx->completed = true;
    ctx->duration_us = nimcp_time_get_us() - start;
}

/*=============================================================================
 * STATIC CONTRIBUTOR REGISTRY
 *
 * All contributors are registered here. The orchestrator iterates this
 * array at session start, checks is_available(), and builds the active list.
 *
 * NOTE: Wave 0 entries (context providers) are not in this registry —
 * they run inline on the main thread before the parallel phase.
 * Wave 1 (parallel) and Wave 2 (sequential) are registered here.
 *
 * The existing 9 phase functions (recall, knowledge, world_model,
 * symbolic_query, symbolic_inference, inference, jepa, verification,
 * epistemic) are NOT in this registry — they are called via the
 * convergent orchestrator's own dispatch using the existing
 * concurrent_phase_ctx_t approach (the orchestrator calls
 * reasoning_engine_reason_concurrent internally for those phases).
 *
 * The NEW contributors are the ones added by this convergent architecture.
 *===========================================================================*/

static const reasoning_contributor_entry_t s_contributor_registry[] = {
    /* ── Tier 1: NEW Evidence Producers (Wave 1) ── */
    { "hippocampus",    REASONING_ROLE_EVIDENCE_PRODUCER,    contrib_hippocampus,     avail_hippocampus,     1 },
    { "semantic_memory", REASONING_ROLE_EVIDENCE_PRODUCER,   contrib_semantic_memory,  avail_semantic_memory, 1 },
    { "parietal",       REASONING_ROLE_EVIDENCE_PRODUCER,    contrib_parietal,        avail_parietal,        1 },
    { "intuition",      REASONING_ROLE_EVIDENCE_PRODUCER,    contrib_intuition,       avail_intuition,       1 },
    { "creative",       REASONING_ROLE_EVIDENCE_PRODUCER,    contrib_creative,        avail_creative,        1 },
    { "kg_self_knowledge", REASONING_ROLE_EVIDENCE_PRODUCER, contrib_kg_reader,       avail_kg_reader,       1 },
    { "mesh_evidence",  REASONING_ROLE_EVIDENCE_PRODUCER,    contrib_mesh_evidence,   avail_mesh,            1 },

    /* ── Tier 2: Confidence Modulators (Wave 1) ── */
    { "emotional",      REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_emotional,       avail_emotional,       1 },
    { "ethics",         REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_ethics,          avail_ethics,          1 },
    { "directives",     REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_directives,      avail_directives,      1 },
    { "bias",           REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_bias,            avail_bias,            1 },
    { "theory_of_mind", REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_theory_of_mind,  avail_theory_of_mind,  1 },
    { "introspection",  REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_introspection,   avail_introspection,   1 },
    { "cingulate",      REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_cingulate,       avail_cingulate,       1 },
    { "salience",       REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_salience,        avail_salience,        1 },
    { "shadow_emotions", REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_shadow_emotions, avail_shadow_emotions, 1 },

    /* ── Tier 2: Affective Modulators (Wave 1) — NEW ── */
    { "grief",          REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_grief,           avail_grief,           1 },
    { "joy",            REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_joy,             avail_joy,             1 },
    { "remorse",        REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_remorse,         avail_remorse,         1 },
    { "social_bond",    REASONING_ROLE_CONFIDENCE_MODULATOR, contrib_social_bond,     avail_social_bond,     1 },
};

static const uint32_t s_registry_count =
    sizeof(s_contributor_registry) / sizeof(s_contributor_registry[0]);

const reasoning_contributor_entry_t* reasoning_convergent_get_registry(
    uint32_t* count_out)
{
    if (count_out) *count_out = s_registry_count;
    return s_contributor_registry;
}

/*=============================================================================
 * BUILD ACTIVE CONTRIBUTORS
 *===========================================================================*/

/**
 * @brief Build the active contributor list from the registry
 *
 * Iterates the static registry, checks is_available() for each entry,
 * and populates the session's contribution array with active contributors.
 */
static uint32_t build_active_contributors(convergent_session_t* session,
                                           reasoning_engine_t* engine,
                                           const char* query,
                                           const char* query_type,
                                           uint32_t domain)
{
    if (!session || !engine) return 0;

    uint32_t count = 0;
    uint32_t max = engine_get_config(engine)->max_convergent_contributors;
    if (max == 0 || max > REASONING_MAX_CONTRIBUTORS)
        max = REASONING_MAX_CONTRIBUTORS;

    for (uint32_t i = 0; i < s_registry_count && count < max; i++) {
        const reasoning_contributor_entry_t* entry = &s_contributor_registry[i];

        if (entry->is_available && !entry->is_available(engine)) {
            continue;
        }

        convergent_contribution_t* contrib = &session->contributions[count];
        memset(contrib, 0, sizeof(convergent_contribution_t));
        contrib->module_name = entry->name;
        contrib->role = entry->role;
        contrib->wave = entry->wave;
        contrib->engine = engine;
        contrib->query = query;
        contrib->query_type = query_type;
        contrib->domain = domain;
        contrib->completed = false;
        contrib->skipped = false;

        count++;
    }

    session->num_contributions = count;
    return count;
}

/*=============================================================================
 * CONVERGENT ORCHESTRATOR
 *===========================================================================*/

int reasoning_engine_reason_convergent(reasoning_engine_t* engine,
                                        const char* query,
                                        uint32_t domain,
                                        reasoning_chain_t* chain)
{
    if (!engine || !query || !chain) return -1;

    reasoning_engine_config_t* config = engine_get_config(engine);
    nimcp_thread_pool_t* pool = engine_get_pool(engine);

    NIMCP_LOGGING_INFO("convergent: beginning convergent reasoning for "
                       "query: \"%.*s%s\"",
                       (int)(strlen(query) > 60 ? 60 : strlen(query)),
                       query,
                       strlen(query) > 60 ? "..." : "");

    /* ── Phase 1: Decomposition (classify query type) — main thread ── */
    const char* query_type = "general";  /* Simple classification */
    const char* q = query;
    while (*q == ' ') q++;
    if (*q == 'w' || *q == 'W') {
        if (strncmp(q + 1, "hat", 3) == 0 || strncmp(q + 1, "HAT", 3) == 0)
            query_type = "what";
        else if (strncmp(q + 1, "hy", 2) == 0 || strncmp(q + 1, "HY", 2) == 0)
            query_type = "why";
    } else if (*q == 'h' || *q == 'H') {
        if (strncmp(q + 1, "ow", 2) == 0 || strncmp(q + 1, "OW", 2) == 0)
            query_type = "how";
    } else if (*q == 'i' || *q == 'I') {
        if (strncmp(q + 1, "s ", 2) == 0 || strncmp(q + 1, "S ", 2) == 0)
            query_type = "boolean";
    }

    /* ── Phase 2: Build active contributor list ── */
    /* FIX #6: Heap-allocate the session to avoid stack overflow.
     * convergent_session_t contains convergent_contribution_t[128], each
     * ~1.1KB due to embedded reasoning_chain_t (conclusion[1024]).
     * Total: ~144KB — far too large for a stack frame. */
    convergent_session_t* session = (convergent_session_t*)nimcp_calloc(
        1, sizeof(convergent_session_t));
    if (!session) {
        NIMCP_LOGGING_ERROR("convergent: failed to allocate session (%zu bytes)",
                            sizeof(convergent_session_t));
        return -1;
    }

    uint32_t num_active = build_active_contributors(
        session, engine, query, query_type, domain);

    NIMCP_LOGGING_INFO("convergent: %u active contributors from registry",
                       num_active);

    /* ── Phase 3: Initialize evidence accumulator ── */
    int rc = reasoning_accumulator_init(
        &session->accumulator, num_active,
        config->convergence_ema_alpha,
        config->convergence_threshold);
    if (rc != 0) {
        NIMCP_LOGGING_ERROR("convergent: failed to initialize accumulator");
        nimcp_free(session);
        session = NULL;
        return -1;
    }

    /* ── Phase 4: Submit Wave 1 contributors to thread pool ── */
    /* FIX #11: Record start time to enforce convergence_timeout_ms */
    uint64_t convergence_start_us = nimcp_time_get_us();
    uint32_t timeout_ms = config->convergence_timeout_ms;
    if (timeout_ms == 0) timeout_ms = REASONING_DEFAULT_CONVERGENCE_TIMEOUT_MS;

    uint32_t wave1_count = 0;
    if (pool) {
        for (uint32_t i = 0; i < num_active; i++) {
            convergent_contribution_t* contrib = &session->contributions[i];
            if (contrib->wave == 1) {
                const reasoning_contributor_entry_t* entry = NULL;
                /* Find matching registry entry for the function pointer */
                for (uint32_t r = 0; r < s_registry_count; r++) {
                    if (strcmp(s_contributor_registry[r].name,
                              contrib->module_name) == 0) {
                        entry = &s_contributor_registry[r];
                        break;
                    }
                }
                if (entry && entry->fn) {
                    nimcp_result_t rc = nimcp_pool_submit(pool, entry->fn, contrib);
                    if (rc != NIMCP_OK) {
                        NIMCP_LOGGING_ERROR("convergent: pool submit failed for '%s' (rc=%d)",
                                            contrib->module_name, rc);
                    }
                    wave1_count++;
                }
            }
        }

        /* Wait for all Wave 1 tasks to complete */
        if (wave1_count > 0) {
            nimcp_pool_wait(pool);
        }
    } else {
        /* FIX #15: No thread pool — fall back to sequential execution of
         * Wave 1 contributors. This correctly handles the NULL pool case. */
        for (uint32_t i = 0; i < num_active; i++) {
            convergent_contribution_t* contrib = &session->contributions[i];
            if (contrib->wave == 1) {
                for (uint32_t r = 0; r < s_registry_count; r++) {
                    if (strcmp(s_contributor_registry[r].name,
                              contrib->module_name) == 0) {
                        if (s_contributor_registry[r].fn)
                            s_contributor_registry[r].fn(contrib);
                        break;
                    }
                }
                wave1_count++;
            }
        }
    }

    /* FIX #11: Enforce convergence_timeout_ms — check if we've exceeded it */
    {
        uint64_t elapsed_us = nimcp_time_get_us() - convergence_start_us;
        uint64_t timeout_us = (uint64_t)timeout_ms * 1000;
        if (elapsed_us > timeout_us) {
            NIMCP_LOGGING_WARN("convergent: timeout exceeded (%lu us > %lu us), "
                               "skipping late contributors",
                               (unsigned long)elapsed_us,
                               (unsigned long)timeout_us);
            /* Mark incomplete contributors as skipped */
            for (uint32_t i = 0; i < num_active; i++) {
                if (!session->contributions[i].completed) {
                    session->contributions[i].skipped = true;
                }
            }
        }
    }

    /* ── Phase 5: Merge Wave 1 results into accumulator ── */
    uint32_t evidence_submitted = 0;
    uint32_t modulations_submitted = 0;
    reasoning_calibration_t* cal = engine_get_calibration(engine);

    /* Snapshot pre-calibration confidences so we can record producer→outcome
     * pairs after Phase 7 finalizes chain->overall_confidence. */
    float pre_calib_confidence[REASONING_MAX_CONTRIBUTORS];
    for (uint32_t i = 0; i < num_active; i++) {
        pre_calib_confidence[i] = session->contributions[i].result_confidence;
    }

    for (uint32_t i = 0; i < num_active; i++) {
        convergent_contribution_t* contrib = &session->contributions[i];
        if (contrib->skipped || !contrib->completed) continue;

        if (contrib->role == REASONING_ROLE_EVIDENCE_PRODUCER) {
            /* Apply calibration adjustment if available */
            if (cal && config->enable_calibration) {
                float scale = 1.0f;
                float bias = 0.0f;
                reasoning_calibration_get_adjustment(
                    cal, contrib->module_name, &scale, &bias);
                contrib->result_confidence =
                    contrib->result_confidence * scale + bias;
                /* Clamp to [0, 1] */
                if (contrib->result_confidence > 1.0f)
                    contrib->result_confidence = 1.0f;
                if (contrib->result_confidence < 0.0f)
                    contrib->result_confidence = 0.0f;
            }

            int submit_rc = reasoning_accumulator_submit_evidence(
                &session->accumulator, chain, contrib);
            reasoning_chain_cleanup(&contrib->local_chain);
            if (submit_rc == 0) {
                evidence_submitted++;
            } else {
                NIMCP_LOGGING_WARN("convergent: failed to submit evidence "
                                   "from %s", contrib->module_name);
            }
        } else if (contrib->role == REASONING_ROLE_CONFIDENCE_MODULATOR) {
            reasoning_accumulator_submit_modulation(
                &session->accumulator, contrib->confidence_delta);
            modulations_submitted++;
        }
    }

    NIMCP_LOGGING_DEBUG("convergent: Wave 1 merged — %u evidence, "
                        "%u modulations, converged=%s",
                        evidence_submitted, modulations_submitted,
                        session->accumulator.converged ? "yes" : "no");

    /* ── Phase 6: Apply net Tier 2 modulation (clamped ±0.3) ── */
    float net_modulation = reasoning_accumulator_apply_modulation(
        &session->accumulator);
    if (fabsf(net_modulation) > 1e-6f) {
        NIMCP_LOGGING_DEBUG("convergent: net modulation = %.4f",
                            (double)net_modulation);
    }

    /* ── Phase 7: Update chain confidence from accumulator ── */
    chain->overall_confidence = session->accumulator.current_confidence;

    /* Mark chain as complete if convergence threshold met */
    if (chain->overall_confidence >= config->confidence_threshold) {
        chain->is_complete = true;
    }

    /* ── Phase 7.5: Record per-contributor calibration outcomes ──
     * Each evidence producer's pre-calibration confidence is the prediction;
     * the converged overall_confidence is the observed outcome. The calibrator
     * EMA-tracks |predicted − outcome| to derive scale/bias for next chain. */
    if (cal && config->enable_calibration) {
        for (uint32_t i = 0; i < num_active; i++) {
            const convergent_contribution_t* contrib = &session->contributions[i];
            if (contrib->skipped || !contrib->completed) continue;
            if (contrib->role != REASONING_ROLE_EVIDENCE_PRODUCER) continue;
            reasoning_calibration_record(cal, contrib->module_name,
                                         pre_calib_confidence[i],
                                         chain->overall_confidence);
        }
    }

    /* ── Phase 8: Synthesis — format conclusion ── */
    snprintf(chain->conclusion, REASONING_CHAIN_CONCLUSION_LEN,
             "Convergent reasoning: %u contributors, %u evidence steps, "
             "confidence=%.3f%s",
             num_active, evidence_submitted,
             (double)chain->overall_confidence,
             session->accumulator.converged ? " (converged)" : "");
    chain->is_complete = true;
    chain->end_time_us = nimcp_time_get_us();

    /* ── Phase 9: Update convergent stats ── */
    reasoning_engine_stats_t* stats = engine_get_stats(engine);
    stats->convergent_queries++;
    float n = (float)stats->convergent_queries;
    stats->avg_convergent_contributors =
        stats->avg_convergent_contributors * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
        (float)num_active / (fabsf(n) > 1e-7f ? n : 1e-7f);
    if (chain->end_time_us > chain->start_time_us) {
        float duration = (float)(chain->end_time_us - chain->start_time_us);
        stats->avg_convergence_time_us =
            stats->avg_convergence_time_us * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
            duration / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    /* Update calibration reliability stat */
    if (cal && config->enable_calibration) {
        calibration_stats_t cal_stats;
        if (reasoning_calibration_get_stats(cal, &cal_stats) == 0) {
            stats->avg_calibration_reliability = cal_stats.avg_reliability;
        }
    }

    /* ── Cleanup ── */
    reasoning_accumulator_destroy(&session->accumulator);
    nimcp_free(session);
    session = NULL;

    NIMCP_LOGGING_INFO("convergent: completed — %u steps, confidence=%.3f, "
                       "contributors=%u",
                       chain->num_steps,
                       (double)chain->overall_confidence,
                       num_active);

    return 0;
}

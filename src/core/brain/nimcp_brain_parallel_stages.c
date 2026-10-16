/**
 * @file nimcp_brain_parallel_stages.c
 * @brief Parallel dispatch for independent cognitive stages in brain_decide()
 *
 * WHAT: Enables concurrent execution of independent pre-forward and post-forward stages
 * WHY:  brain_decide() runs 28 stages serially; many are independent and can be parallelized
 * HOW:  Thread pool dispatch with per-stage task wrappers writing to context structs
 *
 * IMPORTANT DESIGN DECISIONS:
 * - Each task writes ONLY to its own slot in the context struct (no shared writes)
 * - Pre-forward tasks are read-only on brain state (except wellbeing which writes last_distress)
 * - Post-forward independent tasks don't modify the decision struct
 * - Wellbeing's write to brain->last_distress is made atomic-safe
 */

#include "core/brain/nimcp_brain_parallel_stages.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "information/nimcp_shannon.h"
#include "utils/quantum/nimcp_quantum_shannon.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_wm_transfer.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"  // Wave 5: intuitive physics stage

#define LOG_MODULE "BRAIN_PARALLEL"

//=============================================================================
// Pre-Forward Task Context (shared between task wrappers)
//=============================================================================

typedef struct {
    brain_t brain;
    const float* features;
    uint32_t num_features;
    pre_forward_context_t* ctx;
} pre_forward_task_arg_t;

//=============================================================================
// Pre-Forward Stage Task Wrappers
//=============================================================================

static void stage_wellbeing_task(void* arg)
{
    pre_forward_task_arg_t* a = (pre_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->wellbeing_monitoring_enabled && brain->introspection) {
        uint64_t current_time = nimcp_time_get_ms();
        uint64_t last_check = __atomic_load_n(&brain->last_wellbeing_check_time, __ATOMIC_ACQUIRE);
        bool should_check = (brain->wellbeing_check_interval_ms == 0) ||
                           ((current_time - last_check) >= brain->wellbeing_check_interval_ms);
        if (should_check) {
            distress_assessment_t distress = wellbeing_assess_distress(brain->introspection);
            a->ctx->distress_level = distress.severity;
            __atomic_store_n(&brain->last_wellbeing_check_time, current_time, __ATOMIC_RELEASE);
        }
    }
    a->ctx->wellbeing_done = true;
}

static void stage_engram_recall_task(void* arg)
{
    pre_forward_task_arg_t* a = (pre_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->engram_system && a->features && a->num_features > 0) {
        /* Convert float features to neuron indices for engram cue.
         * Use top-k active features (above threshold) as cue neurons. */
        uint32_t max_cue = (a->num_features < 32) ? a->num_features : 32;
        uint32_t cue_neurons[32];
        uint32_t cue_count = 0;

        for (uint32_t i = 0; i < a->num_features && cue_count < max_cue; i++) {
            if (a->features[i] > 0.1f) {
                cue_neurons[cue_count++] = i;
            }
        }

        if (cue_count > 0) {
            float confidence = 0.0f;
            uint64_t engram_id = engram_recall(
                brain->engram_system, cue_neurons, cue_count,
                NULL, NULL, 0, &confidence);
            a->ctx->engram_strength = (engram_id != 0) ? confidence : 0.0f;
        } else {
            a->ctx->engram_strength = 0.0f;
        }
    } else {
        a->ctx->engram_strength = 0.0f;
    }
    a->ctx->engram_done = true;
}

static void stage_sleep_task(void* arg)
{
    pre_forward_task_arg_t* a = (pre_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->sleep_system) {
        a->ctx->sleep_pressure = sleep_get_pressure(brain->sleep_system);
    } else {
        a->ctx->sleep_pressure = 0.0f;
    }
    a->ctx->sleep_done = true;
}

static void stage_curiosity_task(void* arg)
{
    pre_forward_task_arg_t* a = (pre_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->curiosity && a->features && a->num_features > 0) {
        /* Compute intrinsic reward as curiosity measure.
         * Use the cached drive as base, modulated by feature novelty. */
        float base_drive = brain->last_curiosity_drive;

        /* Simple novelty heuristic: variance of input features.
         * High variance = novel input = higher curiosity. */
        float sum = 0.0f, sum_sq = 0.0f;
        uint32_t n = a->num_features;
        for (uint32_t i = 0; i < n; i++) {
            sum += a->features[i];
            sum_sq += a->features[i] * a->features[i];
        }
        float mean = sum / (float)n;
        float variance = (sum_sq / (float)n) - (mean * mean);
        if (variance < 0.0f) variance = 0.0f;

        /* Blend cached drive with novelty signal */
        float novelty = (variance > 1.0f) ? 1.0f : variance;
        a->ctx->curiosity_score = 0.7f * base_drive + 0.3f * novelty;
    } else if (brain->curiosity) {
        a->ctx->curiosity_score = brain->last_curiosity_drive;
    } else {
        a->ctx->curiosity_score = 0.0f;
    }
    a->ctx->curiosity_done = true;
}

static void stage_predictive_task(void* arg)
{
    pre_forward_task_arg_t* a = (pre_forward_task_arg_t*)arg;
    (void)a;  // Prediction depends on network state — run in main thread
    // TODO: Implement parallel predictive coding — currently a stub.
    //       Needs careful synchronization since prediction reads network weights.
    a->ctx->prediction_done = true;
}

//=============================================================================
// Post-Forward Task Context
//=============================================================================

typedef struct {
    brain_t brain;
    void* decision;
    const float* features;
    uint32_t num_features;
    post_forward_context_t* ctx;
} post_forward_task_arg_t;

//=============================================================================
// Post-Forward Stage Task Wrappers (independent stages)
//=============================================================================

static void stage_engram_consol_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->engram_system) {
        /* Advance consolidation by a small time step (one decision cycle ~50ms) */
        bool is_sleeping = (brain->sleep_system &&
                           sleep_get_current_state(brain->sleep_system) != SLEEP_STATE_AWAKE);
        engram_consolidate_update(brain->engram_system, 0.05f, is_sleeping);
    }
    a->ctx->engram_consol_done = true;
}

static void stage_systems_consol_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->systems_consolidation) {
        bool is_sleeping = (brain->sleep_system &&
                           sleep_get_current_state(brain->sleep_system) != SLEEP_STATE_AWAKE);
        /* One decision cycle ~50ms — advance consolidation */
        systems_consolidation_update(brain->systems_consolidation, 0.05f, is_sleeping);
    }
    a->ctx->systems_consol_done = true;
}

static void stage_wm_transfer_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->wm_transfer_system) {
        /* Evaluate WM items for transfer to long-term memory (one decision cycle ~50ms) */
        wm_transfer_evaluate(brain->wm_transfer_system, 0.05f);
    }
    a->ctx->wm_transfer_done = true;
}

static void stage_semantic_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->semantic_memory && brain->systems_consolidation) {
        /* Extract new semantic concepts from recently consolidated memories.
         * Amortize: only run every 100 decisions to avoid thrashing. */
        static _Atomic uint32_t semantic_counter = 0;
        uint32_t count = __atomic_add_fetch(&semantic_counter, 1, __ATOMIC_RELAXED);
        if ((count % 100) == 0) {
            semantic_memory_extract_from_consolidation(brain->semantic_memory);
        }
    }
    a->ctx->semantic_done = true;
}

static void stage_glial_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->glial) {
        /* Amortized glial update — only run every N steps to reduce overhead */
        uint32_t counter = __atomic_add_fetch(&brain->glial_update_counter, 1, __ATOMIC_RELAXED);
        if ((counter % 50) == 0) {
            uint64_t now = nimcp_time_get_us();
            uint64_t pre_astro = brain->glial->total_astrocyte_modulations;
            uint64_t pre_myel  = brain->glial->total_oligodendrocyte_myelinations;
            uint64_t pre_prune = brain->glial->total_microglia_prunings;
            glial_integration_step(brain->glial, now);
            brain->module_activity.astrocyte_modulation_events +=
                (brain->glial->total_astrocyte_modulations - pre_astro);
            brain->module_activity.oligodendrocyte_myelin_apply +=
                (brain->glial->total_oligodendrocyte_myelinations - pre_myel);
            brain->module_activity.microglia_pruning_events +=
                (brain->glial->total_microglia_prunings - pre_prune);
            __atomic_store_n(&brain->last_glial_update_us, now, __ATOMIC_RELEASE);
        }
    }
    a->ctx->glial_done = true;
}

/* Wave 5: advance intuitive physics simulation by one decide-cycle dt.
 *
 * The physics engine maintains a rigid-body scene populated via
 * intuitive_physics_add_object / add_ground. When the scene is empty the
 * step is a cheap no-op — safe to run every decide cycle regardless of
 * whether the brain has actually populated the scene yet. Previously the
 * engine was created at Wave 6 world-model init but intuitive_physics_step
 * was only called from an isolated parietal_cortex path (audit 2026-04-24
 * classified this as HIGH statue). This stage runs alongside the other 8
 * post-forward tasks in parallel; dt is 16ms to match the nominal decide
 * cycle time (BRAIN_CYCLE_BRAIN_UPDATE cadence). */
static void stage_physics_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->intuitive_physics && brain->intuitive_physics_enabled) {
        (void)intuitive_physics_step(brain->intuitive_physics, 0.016f);
    }
    a->ctx->physics_done = true;
}

static void stage_tom_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->theory_of_mind && a->features && a->num_features > 0) {
        float confidence = 0.0f;
        tom_infer_emotion(brain->theory_of_mind, &confidence);
        brain_decision_t* decision = (brain_decision_t*)a->decision;
        const char* intention = (decision && decision->label[0]) ? decision->label : "decide";
        float action_conf = (decision && decision->confidence >= 0.0f && decision->confidence <= 1.0f)
                                ? decision->confidence
                                : confidence;
        tom_update_self_model(brain->theory_of_mind,
                              a->features, a->num_features,
                              intention, action_conf);
    }
    a->ctx->tom_done = true;
}

static void stage_shannon_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->enable_shannon_monitoring) {
        /* Shannon analysis requires pre-computed synapse/neuron metrics arrays.
         * These are computed during the main decision path. Here we aggregate
         * using the cached metrics if available. */
        shannon_network_metrics_t cached = brain->last_shannon_metrics;
        if (cached.total_entropy > 0.0f) {
            LOG_DEBUG(LOG_MODULE, "Shannon: H=%.4f bits, efficiency=%.4f, bottlenecks=%u",
                     cached.total_entropy, cached.average_efficiency, cached.num_bottlenecks);
        }
    }
    a->ctx->shannon_done = true;
}

static void stage_quantum_shannon_task(void* arg)
{
    post_forward_task_arg_t* a = (post_forward_task_arg_t*)arg;
    brain_t brain = a->brain;

    if (brain->enable_quantum_shannon_diffusion && brain->quantum_shannon_diffusion) {
        quantum_shannon_diffusion_t* qsd =
            (quantum_shannon_diffusion_t*)brain->quantum_shannon_diffusion;
        uint32_t steps = brain->quantum_shannon_evolution_steps;
        if (steps == 0) steps = 100;

        for (uint32_t i = 0; i < steps; i++) {
            if (!quantum_shannon_step(qsd)) break;
        }

        shannon_diffusion_metrics_t metrics;
        if (quantum_shannon_get_metrics(qsd, &metrics)) {
            brain->last_quantum_shannon_metrics = metrics;
        }
    }
    a->ctx->quantum_shannon_done = true;
}

//=============================================================================
// Public API
//=============================================================================

bool brain_decide_parallel_pre_forward(
    brain_t brain,
    const float* features,
    uint32_t num_features,
    struct nimcp_thread_pool* pool,
    pre_forward_context_t* ctx)
{
    if (!brain || !pool || !ctx) return false;

    // Allocate task args on heap (pool workers need them to survive this scope)
    pre_forward_task_arg_t* args = nimcp_calloc(5, sizeof(pre_forward_task_arg_t));
    if (!args) return false;

    for (int i = 0; i < 5; i++) {
        args[i].brain = brain;
        args[i].features = features;
        args[i].num_features = num_features;
        args[i].ctx = ctx;
    }

    /* OPTIMIZATION: Only submit non-stub tasks to thread pool.
     * Stub stages (engram_recall, sleep, curiosity, predictive) just set a value
     * to 0.0f with TODO comments — submitting them wastes thread pool bandwidth.
     * Mark them done immediately instead. Wellbeing is the only real implementation. */
    bool all_submitted = true;

    // Wellbeing: Real implementation — submit to pool
    if (nimcp_pool_submit(pool, stage_wellbeing_task,     &args[0]) != NIMCP_SUCCESS) {
        ctx->wellbeing_done = true;  // Mark done so caller doesn't hang
        all_submitted = false;
    }

    // Engram recall: submit to pool
    if (nimcp_pool_submit(pool, stage_engram_recall_task, &args[1]) != NIMCP_SUCCESS) {
        ctx->engram_strength = 0.0f;
        ctx->engram_done = true;
        all_submitted = false;
    }

    // Sleep: submit to pool
    if (nimcp_pool_submit(pool, stage_sleep_task, &args[2]) != NIMCP_SUCCESS) {
        ctx->sleep_pressure = 0.0f;
        ctx->sleep_done = true;
        all_submitted = false;
    }

    // Curiosity: submit to pool
    if (nimcp_pool_submit(pool, stage_curiosity_task, &args[3]) != NIMCP_SUCCESS) {
        ctx->curiosity_score = brain->curiosity ? brain->last_curiosity_drive : 0.0f;
        ctx->curiosity_done = true;
        all_submitted = false;
    }

    // Predictive: depends on network state — run in main thread
    ctx->prediction_done = true;

    if (!all_submitted) {
        LOG_MODULE_WARN(LOG_MODULE, "Some pre-forward tasks failed to submit to thread pool");
    }

    nimcp_pool_wait(pool);
    nimcp_free(args);
    return true;
}

bool brain_decide_submit_post_forward(
    brain_t brain,
    void* decision,
    const float* features,
    uint32_t num_features,
    struct nimcp_thread_pool* pool,
    post_forward_context_t* ctx)
{
    if (!brain || !pool || !ctx) return false;

    /* 9 slots: 8 existing stages + 1 new physics stage (Wave 5). */
    post_forward_task_arg_t* args = nimcp_calloc(9, sizeof(post_forward_task_arg_t));
    if (!args) return false;

    for (int i = 0; i < 9; i++) {
        args[i].brain = brain;
        args[i].decision = decision;
        args[i].features = features;
        args[i].num_features = num_features;
        args[i].ctx = ctx;
    }

    bool all_submitted = true;

    /* Engram consolidation: real implementation */
    if (nimcp_pool_submit(pool, stage_engram_consol_task, &args[0]) != NIMCP_SUCCESS) {
        ctx->engram_consol_done = true;
        all_submitted = false;
    }

    /* Systems consolidation: hippocampus→cortex transfer */
    if (nimcp_pool_submit(pool, stage_systems_consol_task, &args[1]) != NIMCP_SUCCESS) {
        ctx->systems_consol_done = true;
        all_submitted = false;
    }

    /* Working memory transfer: WM→engram selective consolidation */
    if (nimcp_pool_submit(pool, stage_wm_transfer_task, &args[2]) != NIMCP_SUCCESS) {
        ctx->wm_transfer_done = true;
        all_submitted = false;
    }

    /* Semantic memory: concept extraction and spreading activation */
    if (nimcp_pool_submit(pool, stage_semantic_task, &args[3]) != NIMCP_SUCCESS) {
        ctx->semantic_done = true;
        all_submitted = false;
    }

    /* Glial maintenance: real implementation */
    if (nimcp_pool_submit(pool, stage_glial_task, &args[4]) != NIMCP_SUCCESS) {
        ctx->glial_done = true;
        all_submitted = false;
    }

    /* Theory of Mind: self-model update */
    if (nimcp_pool_submit(pool, stage_tom_task, &args[7]) != NIMCP_SUCCESS) {
        ctx->tom_done = true;
        all_submitted = false;
    }

    /* Shannon analysis: real implementation */
    if (nimcp_pool_submit(pool, stage_shannon_task, &args[5]) != NIMCP_SUCCESS) {
        ctx->shannon_done = true;
        all_submitted = false;
    }

    /* Quantum Shannon: real implementation */
    if (nimcp_pool_submit(pool, stage_quantum_shannon_task, &args[6]) != NIMCP_SUCCESS) {
        ctx->quantum_shannon_done = true;
        all_submitted = false;
    }

    /* Wave 5: intuitive physics step. No-op when engine absent, so safe. */
    if (nimcp_pool_submit(pool, stage_physics_task, &args[8]) != NIMCP_SUCCESS) {
        ctx->physics_done = true;
        all_submitted = false;
    }

    if (!all_submitted) {
        LOG_MODULE_WARN(LOG_MODULE, "Some post-forward tasks failed to submit to thread pool");
    }

    // Non-blocking — caller runs sequential stages on main thread, then calls pool_wait.
    // Store args pointer in ctx so caller can free after pool_wait completes.
    ctx->_internal_args = args;
    return true;
}

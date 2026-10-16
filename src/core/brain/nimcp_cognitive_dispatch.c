/**
 * @file nimcp_cognitive_dispatch.c
 * @brief Parallel cognitive module dispatch — actor pattern implementation
 *
 * Each cognitive training module runs as an independent actor:
 * - Receives input via task argument struct (immutable during dispatch)
 * - Trains its own weights (exclusive ownership, no shared writes)
 * - Returns result via promise (loss, timing, success)
 *
 * The coordinator (main thread) collects all results after a barrier
 * and updates cognitive_stats single-threaded. Zero mutex contention.
 */

#include "core/brain/nimcp_cognitive_dispatch.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_lazy_init.h"  /* BRAIN_ENSURE_FEP_ORCHESTRATOR */
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_future.h"

/* Cognitive module headers */
#include "language/nimcp_grounded_language.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/bridges/nimcp_vae_training_bridge.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/immune/nimcp_self_heal.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "snn/nimcp_snn_fno.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "COG_DISPATCH"

/* Forward declarations for subsystem APIs */
struct creative_training_bridge;
typedef struct creative_training_bridge creative_training_bridge_t;
extern int creative_training_submit_feedback(creative_training_bridge_t* bridge,
                                              const void* content,
                                              int modality,
                                              uint8_t rating,
                                              const char* feedback);

/*=============================================================================
 * Task Argument — passed to each actor via thread pool
 *=============================================================================*/

typedef struct {
    brain_t              brain;
    const float*         features;
    uint32_t             num_features;
    const float*         target;
    uint32_t             target_size;
    const char*          label;
    float                loss;
    nimcp_promise_t      promise;
    cognitive_module_id_t module_id;
} cog_task_arg_t;

/*=============================================================================
 * Helper: create a "not executed" result and complete promise
 *=============================================================================*/

static void complete_skip(nimcp_promise_t promise, cognitive_module_id_t id)
{
    cognitive_task_result_t r = {
        .module_id = id, .executed = false, .success = true, .loss = NAN
    };
    nimcp_promise_complete(promise, &r);
}

/*=============================================================================
 * 12 Task Wrapper Functions — one per cognitive module
 *=============================================================================*/

/* 1. Grounded Language */
static void task_grounded_lang(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_GROUNDED_LANG, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->grounded_lang && a->label && a->label[0]) {
        grounded_language_learn_from_text(a->brain->grounded_lang, a->label);
        grounded_language_learn_syntax(a->brain->grounded_lang, a->label);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 2. Knowledge System */
static void task_knowledge(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_KNOWLEDGE, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->knowledge && a->label && a->label[0]) {
        knowledge_learn_from_text(a->brain->knowledge, a->label,
                                  KNOWLEDGE_DOMAIN_GENERAL);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 3. VAE */
static void task_vae(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_VAE, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->vae_training_bridge && a->brain->vae_enabled) {
        vae_training_bridge_t* vae = (vae_training_bridge_t*)a->brain->vae_training_bridge;
        vae_training_step_result_t vae_result = {0};
        vae_training_step(vae, a->features, a->num_features,
                          a->features, a->num_features, &vae_result);
        r.executed = true;
        r.success = true;
        r.loss = vae_result.loss.total_loss;
        r.aux_value = vae_result.loss.total_loss;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 4. FEP-Parietal */
static void task_fep_parietal(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_FEP_PARIETAL, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->parietal && a->features && a->target) {
        fep_parietal_bridge_t* fep_bridge = parietal_get_fep_bridge(a->brain->parietal);
        if (fep_bridge) {
            const float* obs_ptr = a->features;
            const float* tgt_ptr = a->target;
            fep_parietal_train_model(fep_bridge, &obs_ptr, &tgt_ptr, 1);
            r.executed = true;
            r.success = true;
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 5. Physics NN */
static void task_physics_nn(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_PHYSICS_NN, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->parietal && a->features && a->target &&
        a->num_features >= 4 && a->target_size >= 4) {
        uint32_t phys_dim = (a->num_features < 32) ? a->num_features : 32;
        float phys_state[32] = {0};
        float phys_deriv[32] = {0};
        for (uint32_t i = 0; i < phys_dim; i++) {
            phys_state[i] = a->features[i];
            phys_deriv[i] = a->target[i];
        }
        const float* state_ptr = phys_state;
        const float* deriv_ptr = phys_deriv;
        parietal_train_physics_nn(a->brain->parietal,
                                  &state_ptr, &deriv_ptr, 1, 1);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 6. Predictive Hierarchy */
static void task_pred_hierarchy(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_PRED_HIERARCHY, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->pred_hierarchy && a->brain->pred_hierarchy_enabled) {
        predictive_hierarchy_t* ph = (predictive_hierarchy_t*)a->brain->pred_hierarchy;
        float pred_loss = 0.0f;
        if (ph->bottom && a->num_features >= ph->bottom->dim) {
            pred_hier_learn_step(ph, a->features, &pred_loss);
        }
        r.executed = true;
        r.success = true;
        r.loss = pred_loss;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 7. JEPA Predictor */
static void task_jepa(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_JEPA, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->jepa_predictor && a->brain->jepa_predictor_enabled &&
        a->features && a->target) {
        uint32_t latent_dim = (a->num_features < 256) ? a->num_features : 256;
        jepa_latent_t* context = jepa_latent_create_dim(latent_dim);
        jepa_latent_t* target_latent = jepa_latent_create_dim(latent_dim);

        if (context && target_latent) {
            jepa_latent_set_embedding(context, a->features,
                (a->num_features < latent_dim) ? a->num_features : latent_dim);
            jepa_latent_set_embedding(target_latent, a->target,
                (a->target_size < latent_dim) ? a->target_size : latent_dim);

            float jepa_loss = 0.0f;
            jepa_predictor_train_step(
                (jepa_predictor_t*)a->brain->jepa_predictor,
                context, target_latent, &jepa_loss);
            r.executed = true;
            r.success = true;
            r.loss = jepa_loss;
        }
        if (context) jepa_latent_destroy(context);
        if (target_latent) jepa_latent_destroy(target_latent);
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 8. Creative Training */
static void task_creative(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_CREATIVE, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->creative_training_bridge && a->brain->creative_enabled) {
        uint8_t rating = (a->loss < 0.1f) ? 5 :
                         (a->loss < 0.3f) ? 4 :
                         (a->loss < 0.5f) ? 3 :
                         (a->loss < 0.7f) ? 2 : 1;
        creative_training_submit_feedback(
            a->brain->creative_training_bridge,
            a->target, 0, rating, a->label);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 9. Self-Heal Engine */
static void task_self_heal(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_SELF_HEAL, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->self_heal_engine && a->brain->self_heal_enabled && a->features) {
        crash_features_t cf = {0};
        uint32_t cf_dim = (a->num_features < SELF_HEAL_FEATURE_DIM) ?
                           a->num_features : SELF_HEAL_FEATURE_DIM;
        cf.n_features = cf_dim;
        memcpy(cf.features, a->features, cf_dim * sizeof(float));
        float success = 1.0f - fminf(a->loss, 1.0f);
        self_heal_train_online(
            (self_heal_engine_t*)a->brain->self_heal_engine,
            &cf, FIX_PATTERN_UNKNOWN, success);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 10. Intuition System */
static void task_intuition(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_INTUITION, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->intuition_system && a->brain->intuition_system_enabled) {
        intuition_experience_t exp = {
            .id = a->brain->cognitive_train_counter,
            .hunch = NULL,
            .predicted_outcome = 0.0f,
            .actual_outcome = a->loss,
            .timestamp = (float)nimcp_time_get_us(),
            .was_successful = (a->loss < 0.3f)
        };
        const intuition_experience_t* exp_ptr = &exp;
        intuition_train_from_experience(a->brain->intuition_system,
                                         &exp_ptr, 1);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 11. FEP Orchestrator — returns metrics via result, coordinator writes them */
static void task_fep_orchestrator(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_FEP_ORCHESTRATOR, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    /* FEP orchestrator metrics are returned via result struct.
     * The coordinator writes them to brain->fep_orchestrator single-threaded.
     * loss = free_energy, aux_value = surprise */
    if (a->brain->fep_orchestrator && a->brain->fep_orchestrator_enabled) {
        r.executed = true;
        r.success = true;
        r.loss = a->loss;  /* free_energy = training loss */
        r.aux_value = -logf(fmaxf(1.0f - a->loss, 1e-7f));  /* surprise */
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 12. SNN FNO Populations */
static void task_snn_fno(void* arg)
{
    cog_task_arg_t* a = (cog_task_arg_t*)arg;
    cognitive_task_result_t r = { .module_id = COG_MODULE_SNN_FNO, .loss = NAN };
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->snn_fno_populations && a->brain->snn_fno_count > 0) {
        for (uint32_t fi = 0; fi < a->brain->snn_fno_count; fi++) {
            snn_fno_population_t* fno_pop =
                (snn_fno_population_t*)a->brain->snn_fno_populations[fi];
            if (fno_pop) {
                snn_fno_train(fno_pop, 1);
            }
        }
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/*=============================================================================
 * Task function table — indexed by cognitive_module_id_t
 *=============================================================================*/

static const nimcp_task_fn TASK_FNS[COG_MODULE_COUNT] = {
    task_grounded_lang,     /* 0 */
    task_knowledge,         /* 1 */
    task_vae,               /* 2 */
    task_fep_parietal,      /* 3 */
    task_physics_nn,        /* 4 */
    task_pred_hierarchy,    /* 5 */
    task_jepa,              /* 6 */
    task_creative,          /* 7 */
    task_self_heal,         /* 8 */
    task_intuition,         /* 9 */
    task_fep_orchestrator,  /* 10 */
    task_snn_fno            /* 11 */
};

/*=============================================================================
 * Coordinator: update cognitive_stats from batch results (single-threaded)
 *=============================================================================*/

static void dispatch_update_stats(brain_t brain, const cognitive_batch_result_t* batch)
{
    for (uint32_t i = 0; i < COG_MODULE_COUNT; i++) {
        const cognitive_task_result_t* r = &batch->results[i];
        if (!r->executed) continue;

        switch (r->module_id) {
        case COG_MODULE_GROUNDED_LANG:
            brain->cognitive_stats.grounded_lang_steps++;
            brain->cognitive_stats.grounded_lang_last_loss = r->loss;
            break;
        case COG_MODULE_KNOWLEDGE:
            brain->cognitive_stats.knowledge_steps++;
            brain->cognitive_stats.knowledge_last_loss = r->loss;
            break;
        case COG_MODULE_VAE:
            brain->cognitive_stats.vae_steps++;
            brain->cognitive_stats.vae_last_loss = r->loss;
            brain->last_vae_free_energy = r->aux_value;
            break;
        case COG_MODULE_FEP_PARIETAL:
            brain->cognitive_stats.fep_parietal_steps++;
            brain->cognitive_stats.fep_parietal_last_loss = r->loss;
            break;
        case COG_MODULE_PHYSICS_NN:
            brain->cognitive_stats.physics_nn_steps++;
            brain->cognitive_stats.physics_nn_last_loss = r->loss;
            break;
        case COG_MODULE_PRED_HIERARCHY:
            brain->cognitive_stats.pred_hierarchy_steps++;
            brain->cognitive_stats.pred_hierarchy_last_loss = r->loss;
            break;
        case COG_MODULE_JEPA:
            brain->cognitive_stats.jepa_steps++;
            brain->cognitive_stats.jepa_last_loss = r->loss;
            break;
        case COG_MODULE_CREATIVE:
            brain->cognitive_stats.creative_steps++;
            brain->cognitive_stats.creative_last_loss = r->loss;
            break;
        case COG_MODULE_SELF_HEAL:
            brain->cognitive_stats.self_heal_steps++;
            brain->cognitive_stats.self_heal_last_loss = r->loss;
            break;
        case COG_MODULE_INTUITION:
            brain->cognitive_stats.intuition_steps++;
            brain->cognitive_stats.intuition_last_loss = r->loss;
            break;
        case COG_MODULE_FEP_ORCHESTRATOR:
            brain->cognitive_stats.fep_orchestrator_steps++;
            brain->cognitive_stats.fep_orchestrator_last_loss = r->loss;
            if (brain->fep_orchestrator) {
                brain->fep_orchestrator->fep_metrics.free_energy = r->loss;
                brain->fep_orchestrator->fep_metrics.prediction_error = r->loss;
                brain->fep_orchestrator->fep_metrics.surprise = r->aux_value;
            }
            break;
        case COG_MODULE_SNN_FNO:
            /* No per-step counter in cognitive_stats for FNO */
            break;
        default:
            break;
        }
    }
}

/*=============================================================================
 * Main Parallel Dispatch Function
 *=============================================================================*/

cognitive_batch_result_t brain_train_cognitive_parallel(
    brain_t brain,
    const float* features,
    uint32_t num_features,
    const float* target,
    uint32_t target_size,
    const char* label,
    float loss)
{
    cognitive_batch_result_t batch = {0};
    if (!brain) return batch;

    /* Interval gating: expensive subsystem training runs every N steps.
     * NOTE: This is the ONLY place interval gating happens. The sequential
     * fallback does NOT gate (it was already gated here). The wrapper in
     * brain_learning.c calls us directly without gating. */
    uint32_t interval = brain->cognitive_train_interval;
    if (interval == 0) interval = 5;
    brain->cognitive_train_counter++;
    if ((brain->cognitive_train_counter % interval) != 0) return batch;

    uint64_t t0 = nimcp_time_get_us();

    /* Fallback to sequential if no thread pool.
     * Interval gating already passed above — sequential runs unconditionally. */
    if (!brain->inference_pool) {
        brain_train_cognitive_subsystems_sequential(
            brain, features, num_features, target, target_size, label, loss);
        batch.total_elapsed_us = nimcp_time_get_us() - t0;
        brain->dispatch_metrics.total_sequential++;
        brain->dispatch_metrics.last_dispatch_us = batch.total_elapsed_us;
        return batch;
    }

    /* Lazy-init FEP orchestrator on main thread (NOT thread-safe, must not
     * be called from workers). This ensures it's valid before dispatch. */
    BRAIN_ENSURE_FEP_ORCHESTRATOR(brain);

    /* Allocate task arguments on heap (freed after barrier) */
    cog_task_arg_t* args = (cog_task_arg_t*)nimcp_calloc(
        COG_MODULE_COUNT, sizeof(cog_task_arg_t));
    if (!args) {
        brain_train_cognitive_subsystems_sequential(
            brain, features, num_features, target, target_size, label, loss);
        batch.total_elapsed_us = nimcp_time_get_us() - t0;
        return batch;
    }

    nimcp_promise_t promises[COG_MODULE_COUNT] = {0};
    nimcp_future_t  futures[COG_MODULE_COUNT]  = {0};
    uint32_t submitted = 0;

    /* Create promises and submit tasks */
    for (uint32_t i = 0; i < COG_MODULE_COUNT; i++) {
        promises[i] = nimcp_promise_create(sizeof(cognitive_task_result_t));
        if (!promises[i]) {
            /* Promise creation failed — skip this module entirely.
             * batch.results[i] is already zero-initialized (executed=false). */
            continue;
        }
        futures[i] = nimcp_promise_get_future(promises[i]);
        if (!futures[i]) {
            nimcp_promise_destroy(promises[i]);
            promises[i] = NULL;
            continue;
        }

        /* Populate task argument — immutable input for this actor */
        args[i].brain = brain;
        args[i].features = features;
        args[i].num_features = num_features;
        args[i].target = target;
        args[i].target_size = target_size;
        args[i].label = label;
        args[i].loss = loss;
        args[i].promise = promises[i];
        args[i].module_id = (cognitive_module_id_t)i;

        if (nimcp_pool_submit(brain->inference_pool,
                              TASK_FNS[i], &args[i]) == NIMCP_SUCCESS) {
            submitted++;
        } else {
            /* Pool full or error — complete promise as skipped */
            complete_skip(promises[i], (cognitive_module_id_t)i);
        }
    }

    /* Barrier: wait for all modules to complete.
     * nimcp_future_all does NOT handle NULL entries — filter them out. */
    nimcp_future_t valid_futures[COG_MODULE_COUNT];
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < COG_MODULE_COUNT; i++) {
        if (futures[i]) valid_futures[valid_count++] = futures[i];
    }

    if (valid_count > 0) {
        nimcp_future_t all_future = nimcp_future_all(valid_futures, valid_count);
        if (all_future) {
            /* Timeout prevents deadlock if a worker thread crashes.
             * 30s is generous — modules normally complete in <5s total. */
            if (!nimcp_future_wait_timeout(all_future, 30000)) {
                LOG_WARN("Cognitive parallel dispatch timed out after 30s "
                         "(%u/%u modules may be stuck)", valid_count - submitted, valid_count);
            }
            nimcp_future_destroy(all_future);
        } else {
            /* Fallback: wait individually with timeout */
            for (uint32_t i = 0; i < valid_count; i++) {
                nimcp_future_wait_timeout(valid_futures[i], 30000);
            }
        }
    }

    /* Coordinator: collect results single-threaded */
    for (uint32_t i = 0; i < COG_MODULE_COUNT; i++) {
        if (futures[i]) {
            /* Only get result if future completed (avoids THROW_TO_IMMUNE on timeout) */
            if (nimcp_future_is_ready(futures[i])) {
                nimcp_future_get(futures[i], &batch.results[i]);
                if (batch.results[i].executed) batch.num_executed++;
            }
            /* else: batch.results[i] stays zero-initialized (executed=false) */
            nimcp_future_destroy(futures[i]);
        }
        if (promises[i]) nimcp_promise_destroy(promises[i]);
    }

    nimcp_free(args);

    /* Update brain stats from collected results — single-threaded, no contention */
    dispatch_update_stats(brain, &batch);

    batch.total_elapsed_us = nimcp_time_get_us() - t0;

    /* Update dispatch metrics on brain for probe monitoring */
    brain->dispatch_metrics.last_dispatch_us = batch.total_elapsed_us;
    brain->dispatch_metrics.last_modules_executed = batch.num_executed;
    brain->dispatch_metrics.last_modules_submitted = submitted;
    brain->dispatch_metrics.total_dispatches++;
    brain->dispatch_metrics.cumulative_dispatch_us += batch.total_elapsed_us;
    brain->dispatch_metrics.pool_thread_count =
        brain->inference_pool ? (uint32_t)nimcp_pool_active(brain->inference_pool) : 0;

    /* Compute slowest and average module time */
    float slowest = 0.0f;
    float sum = 0.0f;
    uint32_t timed = 0;
    for (uint32_t i = 0; i < COG_MODULE_COUNT; i++) {
        if (batch.results[i].executed) {
            float us = (float)batch.results[i].elapsed_us;
            if (us > slowest) slowest = us;
            sum += us;
            timed++;
        }
    }
    brain->dispatch_metrics.slowest_module_us = slowest;
    brain->dispatch_metrics.avg_module_us = timed > 0 ? sum / timed : 0.0f;

    if (submitted > 0) {
        LOG_DEBUG("Cognitive parallel: %u/%d modules in %lu us (submitted=%u)",
                  batch.num_executed, COG_MODULE_COUNT,
                  (unsigned long)batch.total_elapsed_us, submitted);
    }

    return batch;
}

/*=============================================================================
 * PHASE 2-4: INFERENCE STAGE PARALLEL DISPATCH
 *
 * Actor pattern for brain_decide() cognitive stages.
 * Each stage receives a snapshot of the decision state (immutable),
 * returns its contribution via promise, coordinator merges single-threaded.
 *=============================================================================*/

/* Additional headers for inference stages */
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
/* Imagination engine — forward declarations to avoid type conflicts
 * from the heavy imagination_engine.h header */
typedef struct imagination_scenario imagination_scenario_t;
typedef struct imagination_engine* imagination_t;
extern imagination_scenario_t* imagination_begin_scenario(imagination_t engine, int mode, void* goal);
extern int imagination_step_scenario(imagination_t engine, imagination_scenario_t* scenario);
extern int imagination_end_scenario(imagination_t engine, imagination_scenario_t* scenario);
#define IMAGINATION_MODE_PROSPECTIVE 1
#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "security/nimcp_audit_log.h"
#include "security/lgss/nimcp_lgss.h"
#include "core/brain/nimcp_brain.h"  /* brain_decision_t */

/* Forward declarations for functions defined in brain_part_core.c */
extern action_t brain_decision_to_action(brain_decision_t* decision,
                                          uint32_t action_id, const char* name);

/* Task argument for inference stage actors */
typedef struct {
    brain_t              brain;
    decide_snapshot_t    snap;         /* immutable snapshot */
    const float*         features;
    uint32_t             num_features;
    nimcp_promise_t      promise;
    decide_stage_id_t    stage_id;
} decide_task_arg_t;

/* Helper: return a "not executed" result */
static void decide_skip(nimcp_promise_t promise, decide_stage_id_t id) {
    decide_stage_result_t r = {0};
    r.stage_id = id;
    nimcp_promise_complete(promise, &r);
}

/*=============================================================================
 * REASONING GROUP ACTORS (STAGE 4.1-4.3)
 *=============================================================================*/

/* 4.1: Reasoning Engine */
static void task_decide_reasoning(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_REASONING;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->reasoning_engine && a->brain->reasoning_engine_enabled &&
        a->snap.label[0]) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        reasoning_engine_connect_brain(a->brain->reasoning_engine, a->brain);
        int rc = reasoning_engine_reason(a->brain->reasoning_engine, a->snap.label, &chain);
        if (rc == 0 && chain.num_steps > 0) {
            float rw = a->brain->config.reasoning_blend_weight;
            /* Return the delta: new_conf - old_conf */
            float new_conf = a->snap.confidence * (1.0f - rw) + chain.overall_confidence * rw;
            r.confidence_delta = new_conf - a->snap.confidence;
            r.aux_metric = chain.overall_confidence;
            r.executed = true;
            r.success = true;
        }
        reasoning_chain_cleanup(&chain);
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 4.2a: Inner Dialogue */
static void task_decide_inner_dialogue(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_INNER_DIALOGUE;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->inner_dialogue && a->brain->inner_dialogue_enabled &&
        a->snap.label[0]) {
        if (a->snap.confidence > a->brain->config.dialogue_confidence_min &&
            a->snap.confidence < a->brain->config.dialogue_confidence_max) {
            inner_dialogue_result_t dialogue_result;
            memset(&dialogue_result, 0, sizeof(dialogue_result));
            int start_rc = inner_dialogue_engine_start(a->brain->inner_dialogue, a->snap.label);
            if (start_rc == 0) {
                int run_rc = inner_dialogue_engine_run(a->brain->inner_dialogue, &dialogue_result);
                if (run_rc == 0 && dialogue_result.has_conclusion) {
                    /* Return multiplicative factor */
                    r.confidence_is_multiplicative = true;
                    if (dialogue_result.final_agreement > 0.8f) {
                        r.confidence_delta = 1.1f; /* DIALOGUE_BOOST_FACTOR */
                    } else if (dialogue_result.final_agreement < 0.4f) {
                        r.confidence_delta = 0.8f; /* DIALOGUE_REDUCE_FACTOR */
                    } else {
                        r.confidence_delta = 1.0f; /* no change */
                    }
                    r.aux_metric = dialogue_result.final_agreement;
                    r.executed = true;
                    r.success = true;
                }
            }
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 4.2b: Imagination Engine */
static void task_decide_imagination(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_IMAGINATION;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->imagination && a->brain->imagination_enabled) {
        if (a->snap.confidence > a->brain->config.imagination_confidence_min) {
            imagination_scenario_t* scenario = imagination_begin_scenario(
                a->brain->imagination, IMAGINATION_MODE_PROSPECTIVE, NULL);
            if (scenario) {
                bool sim_ok = true;
                for (int s = 0; s < 3 && sim_ok; s++) { /* IMAGINATION_SIM_STEPS */
                    sim_ok = (imagination_step_scenario(a->brain->imagination, scenario) == 0);
                }
                r.executed = true;
                r.success = true;
                if (!sim_ok) {
                    r.confidence_is_multiplicative = true;
                    r.confidence_delta = 0.95f; /* IMAGINATION_FAIL_PENALTY */
                }
                imagination_end_scenario(a->brain->imagination, scenario);
            }
        }
        /* Option-1 (Slice A, 2026-05-19): removed snn_language_bridge_apply_stdp
         * call. The bridge is transport-only now; STDP on concept↔word
         * bindings lives in the SNN's projection synapses (Slice B). */
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 4.3: Recursive Cognition */
static void task_decide_recursive_cog(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_RECURSIVE_COG;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->rcog_engine && a->brain->rcog_engine_enabled && a->snap.label[0]) {
        if (a->snap.confidence < a->brain->config.rcog_confidence_max &&
            a->snap.confidence > 0.05f) { /* RCOG_CONFIDENCE_FLOOR */
            rcog_goal_t goal = rcog_engine_create_goal(a->snap.label, RCOG_GOAL_ANALYSIS);
            rcog_process_result_t rcog_result;
            memset(&rcog_result, 0, sizeof(rcog_result));
            int rc = rcog_engine_process(a->brain->rcog_engine, &goal, &rcog_result);
            if (rc == 0 && rcog_result.success) {
                float rcog_conf = rcog_result.answer.confidence;
                if (rcog_conf > a->snap.confidence) {
                    float new_conf = sqrtf(a->snap.confidence * rcog_conf);
                    r.confidence_delta = new_conf - a->snap.confidence;
                    r.executed = true;
                    r.success = true;
                }
            }
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/*=============================================================================
 * EVALUATIVE GROUP ACTORS (STAGE 7.8, 7.9, 8, 9)
 *=============================================================================*/

/* 7.8: Ethics + LGSS (SECURITY CRITICAL — can block) */
static void task_decide_ethics(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_ETHICS;
    uint64_t t0 = nimcp_time_get_us();

    /* Ethics evaluation */
    if (a->brain->ethics && a->brain->config.enable_ethics && a->features) {
        action_context_t ethics_action = {
            .features = (float*)a->features,
            .num_features = a->num_features,
            .predicted_harm = (a->snap.confidence < 0.5f) ? 0.5f : 0.0f
        };
        ethics_evaluation_t ethics_eval = ethics_engine_evaluate_action(
            a->brain->ethics, &ethics_action);

        if (!ethics_eval.allowed) {
            r.should_block = true;
            strncpy(r.label_suffix, " [BLOCKED-ETHICS]", sizeof(r.label_suffix) - 1);
            snprintf(r.explanation_append, sizeof(r.explanation_append),
                     " | ETHICS: %s", ethics_eval.explanation);
            r.executed = true;
            r.success = true;
        } else if (ethics_eval.golden_rule_score < 0.0f) {
            r.confidence_is_multiplicative = true;
            r.confidence_delta = 1.0f + ethics_eval.golden_rule_score;
            r.executed = true;
            r.success = true;
        }
    }

    /* LGSS evaluation (non-removable safety layer) */
    if (a->brain->lgss && a->brain->lgss_enabled && !r.should_block) {
        safety_action_context_t lgss_ctx;
        memset(&lgss_ctx, 0, sizeof(lgss_ctx));
        strncpy(lgss_ctx.string_fields[0].key, "operation", 63);
        strncpy(lgss_ctx.string_fields[0].value, "brain_decide_output", SAFETY_MAX_VALUE_LEN - 1);
        strncpy(lgss_ctx.string_fields[1].key, "label", 63);
        strncpy(lgss_ctx.string_fields[1].value,
            a->snap.label[0] ? a->snap.label : "(none)", SAFETY_MAX_VALUE_LEN - 1);
        lgss_ctx.num_string_fields = 2;
        strncpy(lgss_ctx.numeric_fields[0].key, "confidence", 63);
        lgss_ctx.numeric_fields[0].value = a->snap.confidence;
        lgss_ctx.num_numeric_fields = 1;
        lgss_ctx.domain_hint = SAFETY_DOMAIN_GOVERNANCE;
        lgss_ctx.has_domain_hint = true;

        safety_evaluation_t lgss_eval;
        int lgss_rc = lgss_evaluate(a->brain->lgss, &lgss_ctx, &lgss_eval);
        if (lgss_rc == 0 && lgss_eval.action == SAFETY_ACTION_DENY) {
            r.should_block = true;
            strncpy(r.label_suffix, " [BLOCKED-LGSS]", sizeof(r.label_suffix) - 1);
            r.executed = true;
            r.success = true;
        } else if (lgss_rc == 0 && lgss_eval.action == SAFETY_ACTION_ESCALATE) {
            r.confidence_is_multiplicative = true;
            r.confidence_delta = 0.3f;
            strncpy(r.label_suffix, " [LGSS-ESCALATE]", sizeof(r.label_suffix) - 1);
            r.executed = true;
            r.success = true;
        }
    }

    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 7.9: Epistemic Filtering */
static void task_decide_epistemic(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_EPISTEMIC;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->epistemic && a->snap.label[0]) {
        claim_evidence_t evidence;
        epistemic_evidence_init(&evidence);
        evidence.evidence_quality = EVIDENCE_MODERATE;
        evidence.plausibility = PLAUSIBLE_NEUTRAL;
        evidence.num_sources = 1;
        evidence.is_falsifiable = true;

        epistemic_assessment_t assessment;
        epistemic_assessment_init(&assessment);

        if (epistemic_assess_claim(a->brain->epistemic, a->snap.label,
                                    a->snap.confidence, &evidence, &assessment)) {
            float mult = 1.0f;
            if (assessment.epistemic_quality < 0.5f) {
                mult *= assessment.epistemic_quality;
            }
            if (assessment.num_biases_detected > 0) {
                strncpy(r.label_suffix, " [BIAS-DETECTED]", sizeof(r.label_suffix) - 1);
                float bias_penalty = assessment.num_biases_detected * 0.2f;
                mult *= fmaxf(0.2f, 1.0f - bias_penalty);
            }
            float conspiracy = epistemic_check_conspiracy_pattern(
                a->brain->epistemic, a->snap.label, &evidence);
            if (conspiracy > 0.7f) {
                strncpy(r.label_suffix, " [CONSPIRACY-LIKE]", sizeof(r.label_suffix) - 1);
                mult *= 0.1f;
            }
            if (mult < 1.0f) {
                r.confidence_is_multiplicative = true;
                r.confidence_delta = mult;
                r.executed = true;
                r.success = true;
            }
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 8: Mirror Neuron Integration */
static void task_decide_mirror(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_MIRROR_NEURON;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->config.enable_mirror_neurons && a->brain->mirror_neurons &&
        a->snap.output_vector && a->snap.output_size > 0) {
        /* Find argmax for action_id */
        uint32_t action_id = 0;
        float max_val = a->snap.output_vector[0];
        for (uint32_t i = 1; i < a->snap.output_size; i++) {
            if (a->snap.output_vector[i] > max_val) {
                max_val = a->snap.output_vector[i];
                action_id = i;
            }
        }
        char action_name[32];
        snprintf(action_name, sizeof(action_name), "action_%u", action_id);

        /* Use action_t from nimcp_mirror_neurons.h */
        action_t action = {0};
        action.action_id = action_id;
        strncpy(action.action_name, action_name, sizeof(action.action_name) - 1);
        action.confidence = a->snap.confidence;
        mirror_neurons_execute_action(a->brain->mirror_neurons, &action);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* 9: Theory of Mind */
static void task_decide_tom(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_THEORY_OF_MIND;
    uint64_t t0 = nimcp_time_get_us();

    if (a->brain->config.enable_theory_of_mind && a->brain->theory_of_mind &&
        a->features && a->snap.label[0]) {
        tom_update_self_model(a->brain->theory_of_mind, a->features,
                               a->num_features, a->snap.label, a->snap.confidence);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/*=============================================================================
 * C5/C6 ACTORS (cognitive inference + online learning)
 *=============================================================================*/

/* C5.1: Parietal step */
static void task_decide_parietal(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_PARIETAL;
    uint64_t t0 = nimcp_time_get_us();
    if (a->brain->parietal) {
        parietal_step(a->brain->parietal, 1000);
        r.executed = true;
        r.success = true;
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* C5.2: Predictive Hierarchy inference */
static void task_decide_pred_hierarchy(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_PRED_HIERARCHY;
    uint64_t t0 = nimcp_time_get_us();
    if (a->brain->pred_hierarchy && a->brain->pred_hierarchy_enabled && a->features) {
        predictive_hierarchy_t* ph = (predictive_hierarchy_t*)a->brain->pred_hierarchy;
        if (ph->bottom && a->num_features >= ph->bottom->dim) {
            float pred_loss = 0.0f;
            pred_hier_learn_step(ph, a->features, &pred_loss);
            if (pred_loss > 0.5f) {
                r.confidence_is_multiplicative = true;
                r.confidence_delta = 0.95f; /* slight reduction for high prediction error */
            }
            r.aux_metric = pred_loss;
            r.executed = true;
            r.success = true;
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* C5.3: VAE anomaly */
static void task_decide_vae_anomaly(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_VAE_ANOMALY;
    uint64_t t0 = nimcp_time_get_us();
    if (a->brain->vae_system && a->brain->vae_enabled) {
        float anomaly = a->brain->last_vae_anomaly_score;
        if (anomaly > 0.8f) {
            r.confidence_is_multiplicative = true;
            r.confidence_delta = 0.5f;
            r.executed = true;
            r.success = true;
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* C5.4: JEPA inference */
static void task_decide_jepa_inference(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_JEPA_INFERENCE;
    uint64_t t0 = nimcp_time_get_us();
    if (a->brain->jepa_predictor && a->brain->jepa_predictor_enabled &&
        a->features && a->snap.output_vector) {
        uint32_t ldim = (a->num_features < 256) ? a->num_features : 256;
        jepa_latent_t* ctx_l = jepa_latent_create_dim(ldim);
        jepa_latent_t* out_l = jepa_latent_create_dim(ldim);
        if (ctx_l && out_l) {
            jepa_latent_set_embedding(ctx_l, a->features,
                (a->num_features < ldim) ? a->num_features : ldim);
            uint32_t osz = (a->snap.output_size < ldim) ? a->snap.output_size : ldim;
            jepa_latent_set_embedding(out_l, a->snap.output_vector, osz);
            float sim = jepa_latent_cosine_similarity(ctx_l, out_l);
            if (sim > 0.5f) {
                r.confidence_delta = (sim - 0.5f) * 0.1f; /* additive boost */
                r.executed = true;
                r.success = true;
            }
        }
        if (ctx_l) jepa_latent_destroy(ctx_l);
        if (out_l) jepa_latent_destroy(out_l);
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/* C5.5: FEP active inference — simplified (full API needs fep_problem_state_t) */
static void task_decide_fep_active(void* arg) {
    decide_task_arg_t* a = (decide_task_arg_t*)arg;
    decide_stage_result_t r = {0};
    r.stage_id = DECIDE_STAGE_FEP_ACTIVE;
    uint64_t t0 = nimcp_time_get_us();
    if (a->brain->parietal && a->snap.output_vector) {
        fep_parietal_bridge_t* fep = parietal_get_fep_bridge(a->brain->parietal);
        if (fep) {
            /* FEP active inference requires fep_problem_state_t which is complex
             * to construct from a snapshot. For now, mark as executed if FEP bridge
             * exists — the actual inference runs in the sequential path. */
            r.executed = true;
            r.success = true;
        }
    }
    r.elapsed_us = nimcp_time_get_us() - t0;
    nimcp_promise_complete(a->promise, &r);
}

/*=============================================================================
 * GENERIC PARALLEL DISPATCH HELPER (used by all 3 dispatch functions)
 *=============================================================================*/

static decide_batch_result_t dispatch_decide_stages(
    brain_t brain,
    const decide_snapshot_t* snap,
    const float* features,
    uint32_t num_features,
    const nimcp_task_fn* task_fns,
    const decide_stage_id_t* stage_ids,
    uint32_t num_stages)
{
    decide_batch_result_t batch = {0};
    if (!brain || !snap || num_stages == 0) return batch;

    /* Fallback to sequential if no pool */
    if (!brain->inference_pool) return batch;

    uint64_t t0 = nimcp_time_get_us();

    decide_task_arg_t* args = (decide_task_arg_t*)nimcp_calloc(
        num_stages, sizeof(decide_task_arg_t));
    if (!args) return batch;

    nimcp_promise_t* promises = (nimcp_promise_t*)nimcp_calloc(num_stages, sizeof(nimcp_promise_t));
    nimcp_future_t* futures = (nimcp_future_t*)nimcp_calloc(num_stages, sizeof(nimcp_future_t));
    if (!promises || !futures) {
        nimcp_free(args); nimcp_free(promises); nimcp_free(futures);
        return batch;
    }

    uint32_t submitted = 0;
    for (uint32_t i = 0; i < num_stages; i++) {
        promises[i] = nimcp_promise_create(sizeof(decide_stage_result_t));
        if (!promises[i]) continue;
        futures[i] = nimcp_promise_get_future(promises[i]);
        if (!futures[i]) { nimcp_promise_destroy(promises[i]); promises[i] = NULL; continue; }

        args[i].brain = brain;
        args[i].snap = *snap;
        args[i].features = features;
        args[i].num_features = num_features;
        args[i].promise = promises[i];
        args[i].stage_id = stage_ids[i];

        if (nimcp_pool_submit(brain->inference_pool, task_fns[i], &args[i]) == NIMCP_SUCCESS) {
            submitted++;
        } else {
            decide_skip(promises[i], stage_ids[i]);
        }
    }

    /* Barrier with timeout */
    nimcp_future_t valid[DECIDE_STAGE_COUNT];
    uint32_t vc = 0;
    for (uint32_t i = 0; i < num_stages; i++) {
        if (futures[i]) valid[vc++] = futures[i];
    }
    if (vc > 0) {
        nimcp_future_t all = nimcp_future_all(valid, vc);
        if (all) {
            nimcp_future_wait_timeout(all, 30000);
            nimcp_future_destroy(all);
        } else {
            for (uint32_t i = 0; i < vc; i++) nimcp_future_wait_timeout(valid[i], 30000);
        }
    }

    /* Collect results */
    for (uint32_t i = 0; i < num_stages; i++) {
        if (futures[i] && nimcp_future_is_ready(futures[i])) {
            nimcp_future_get(futures[i], &batch.results[i]);
            if (batch.results[i].executed) batch.num_executed++;
        }
        if (futures[i]) nimcp_future_destroy(futures[i]);
        if (promises[i]) nimcp_promise_destroy(promises[i]);
    }

    nimcp_free(args);
    nimcp_free(promises);
    nimcp_free(futures);

    batch.total_elapsed_us = nimcp_time_get_us() - t0;
    return batch;
}

/*=============================================================================
 * PUBLIC: Reasoning parallel dispatch (STAGE 4.1-4.3)
 *=============================================================================*/

decide_batch_result_t brain_decide_reasoning_parallel(
    brain_t brain, const decide_snapshot_t* snap)
{
    static const nimcp_task_fn fns[] = {
        task_decide_reasoning, task_decide_inner_dialogue,
        task_decide_imagination, task_decide_recursive_cog
    };
    static const decide_stage_id_t ids[] = {
        DECIDE_STAGE_REASONING, DECIDE_STAGE_INNER_DIALOGUE,
        DECIDE_STAGE_IMAGINATION, DECIDE_STAGE_RECURSIVE_COG
    };
    return dispatch_decide_stages(brain, snap, NULL, 0, fns, ids, 4);
}

/*=============================================================================
 * PUBLIC: Evaluative parallel dispatch (STAGE 7.8, 7.9, 8, 9)
 *=============================================================================*/

decide_batch_result_t brain_decide_evaluative_parallel(
    brain_t brain, const decide_snapshot_t* snap,
    const float* features, uint32_t num_features)
{
    /* Lazy-init ethics on main thread before dispatch */
    BRAIN_ENSURE_ETHICS(brain);
    BRAIN_ENSURE_MIRROR_NEURONS(brain);
    BRAIN_ENSURE_THEORY_OF_MIND(brain);

    static const nimcp_task_fn fns[] = {
        task_decide_ethics, task_decide_epistemic,
        task_decide_mirror, task_decide_tom
    };
    static const decide_stage_id_t ids[] = {
        DECIDE_STAGE_ETHICS, DECIDE_STAGE_EPISTEMIC,
        DECIDE_STAGE_MIRROR_NEURON, DECIDE_STAGE_THEORY_OF_MIND
    };
    return dispatch_decide_stages(brain, snap, features, num_features, fns, ids, 4);
}

/*=============================================================================
 * PUBLIC: C5/C6 cognitive parallel dispatch
 *=============================================================================*/

decide_batch_result_t brain_decide_cognitive_parallel(
    brain_t brain, const decide_snapshot_t* snap,
    const float* features, uint32_t num_features)
{
    static const nimcp_task_fn fns[] = {
        task_decide_parietal, task_decide_pred_hierarchy,
        task_decide_vae_anomaly, task_decide_jepa_inference,
        task_decide_fep_active
    };
    static const decide_stage_id_t ids[] = {
        DECIDE_STAGE_PARIETAL, DECIDE_STAGE_PRED_HIERARCHY,
        DECIDE_STAGE_VAE_ANOMALY, DECIDE_STAGE_JEPA_INFERENCE,
        DECIDE_STAGE_FEP_ACTIVE
    };
    return dispatch_decide_stages(brain, snap, features, num_features, fns, ids, 5);
}

/*=============================================================================
 * COORDINATOR: Apply batch results to decision (single-threaded merge)
 *=============================================================================*/

void decide_batch_apply(brain_decision_t* decision, const decide_batch_result_t* batch)
{
    if (!decision || !batch) return;

    /* Pass 1: Apply blocking results (ethics/LGSS) — highest priority */
    for (uint32_t i = 0; i < DECIDE_STAGE_COUNT; i++) {
        const decide_stage_result_t* r = &batch->results[i];
        if (!r->executed || !r->should_block) continue;
        decision->confidence = 0.0f;
        if (r->label_suffix[0]) {
            strncat(decision->label, r->label_suffix,
                    sizeof(decision->label) - strlen(decision->label) - 1);
        }
        if (r->explanation_append[0]) {
            strncat(decision->explanation, r->explanation_append,
                    sizeof(decision->explanation) - strlen(decision->explanation) - 1);
        }
    }

    /* If blocked, skip remaining adjustments */
    if (decision->confidence == 0.0f) return;

    /* Pass 2: Apply multiplicative deltas */
    for (uint32_t i = 0; i < DECIDE_STAGE_COUNT; i++) {
        const decide_stage_result_t* r = &batch->results[i];
        if (!r->executed || r->should_block) continue;
        if (r->confidence_is_multiplicative && r->confidence_delta != 0.0f) {
            decision->confidence *= r->confidence_delta;
        }
    }

    /* Pass 3: Apply additive deltas */
    for (uint32_t i = 0; i < DECIDE_STAGE_COUNT; i++) {
        const decide_stage_result_t* r = &batch->results[i];
        if (!r->executed || r->should_block) continue;
        if (!r->confidence_is_multiplicative && r->confidence_delta != 0.0f) {
            decision->confidence += r->confidence_delta;
        }
        /* Append label suffixes from non-blocking stages */
        if (r->label_suffix[0] && !r->should_block) {
            strncat(decision->label, r->label_suffix,
                    sizeof(decision->label) - strlen(decision->label) - 1);
        }
    }

    /* Clamp confidence to [0, 1] */
    if (decision->confidence < 0.0f) decision->confidence = 0.0f;
    if (decision->confidence > 1.0f) decision->confidence = 1.0f;
}

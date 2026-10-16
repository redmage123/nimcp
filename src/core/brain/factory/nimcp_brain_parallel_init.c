//=============================================================================
// nimcp_brain_parallel_init.c - Wave-based parallel subsystem initialization
//=============================================================================
/**
 * @file nimcp_brain_parallel_init.c
 * @brief Parallelizes brain subsystem initialization using dependency waves
 *
 * WHAT: Replaces sequential 80+ init calls with wave-based parallel execution
 * WHY:  Sequential init takes 80-250s on large brains; many inits are independent
 * HOW:  Groups inits into ~28 dependency waves. Within each wave, tasks run in
 *       parallel via the thread pool. Between waves, nimcp_pool_wait() ensures
 *       ordering. Atomic error flag propagates failures.
 *
 * @version 1.0.0
 */

#include "core/brain/factory/nimcp_brain_parallel_init.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_bio_async.h"

// Emotional subsystem creation (inline inits in Wave 4)
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"

#include <stdatomic.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "PARALLEL_INIT"

//=============================================================================
// Types
//=============================================================================

/** Shared context for all init tasks within a brain creation */
typedef struct {
    brain_t brain;
    atomic_bool error_flag;
    atomic_int first_error_wave;
    const char* failed_subsystem;  // Name of first failure (best-effort, racy but OK for logging)
} parallel_init_ctx_t;

/** A single init task submitted to the thread pool */
typedef struct {
    parallel_init_ctx_t* ctx;
    bool (*init_fn)(brain_t);
    const char* name;
} init_task_t;

//=============================================================================
// External init function declarations (from factory init modules)
//=============================================================================

// Wave 0: Foundation
extern bool nimcp_brain_factory_init_gpu_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_gpu_inference(brain_t brain);

// Wave 1: Independent heavy subsystems
extern bool nimcp_brain_factory_init_glial_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_axon_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_dendrite_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_multimodal_subsystems(brain_t brain);
extern bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t brain);

// Wave 2: Neuromodulation
extern bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);
extern bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);
extern bool nimcp_brain_factory_init_neuromod_nuclei(brain_t brain);

// Wave 3: GPU substrate, CPU substrate+thalamic, symbolic, working memory
extern bool nimcp_brain_factory_init_substrate_gpu_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_substrate_thalamic_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);

// Biology cluster (epigenetics + neurogenesis + NVC) — register_driven via coord
extern bool nimcp_brain_factory_init_biology_subsystem(brain_t brain);

// Wave 8A: Cochlea + 15 consumer bridges — register_driven via coord
// Placed in Wave 14+ so brain->immune_system (Wave 11) and brain->fep_orchestrator
// (Wave 14) are both available. Null-tolerant for any missing dep.
extern bool nimcp_brain_factory_init_cochlea_subsystem(brain_t brain);

// Wave 8C: 4 HALF-STATUE physics bridges (ephaptic_bio_async, ephaptic_fft,
// hh_bio_async, thermo_bio_async) + their 3 upstream modules. Runs after
// bio_router (Wave 13) so the bio-async bridges can connect. Null-tolerant
// if bio_router is absent.
extern bool nimcp_brain_factory_init_physics_bridges_subsystem(brain_t brain);

// Chemistry cluster (pumps + buffers + pH + NO) — single BRAIN_CYCLE_CHEMISTRY at 10ms
extern bool nimcp_brain_factory_init_chemistry_subsystem(brain_t brain);

// Predictive-immune coupling — register_driven via coord, needs predictive_network + immune
extern bool nimcp_brain_factory_init_predictive_immune_subsystem(brain_t brain);

// Wave 8B/8C + Wave 4 meta_learning + Wave 5 intuitive_physics driven cycles —
// registers the 6 already-created tick drivers as driven coordinator cycles
// so state advances on the coordinator's threads in addition to the hot paths.
extern bool nimcp_brain_factory_init_region_cycles_subsystem(brain_t brain);

// Wave 5: Cognitive foundations
extern bool nimcp_brain_factory_init_attention_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_epistemic_subsystem(brain_t brain);

// Wave 6: Memory systems
extern bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_pr_memory_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_lnn_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_world_model_subsystem(brain_t brain);

// Wave 7: Executive/ToM
extern bool nimcp_brain_factory_init_executive_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_natural_explanations_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_meta_learning_subsystem(brain_t brain);

// Wave 8: Independent cognitive
extern bool nimcp_brain_factory_init_mental_health_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_trauma_resilience(brain_t brain);
extern bool nimcp_brain_factory_init_predictive_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_mirror_neurons(brain_t brain);
extern bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_salience_subsystem(brain_t brain);

// Wave 9: Ethics triad, introspection, connectivity, middleware
extern bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_middleware_controller_subsystem(brain_t brain);

// Wave 10: Self-awareness
extern bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);

// Wave 11: Security chain
extern bool nimcp_brain_factory_init_security_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_immune_subsystem(brain_t brain);

// Wave 12: Training pipeline
extern bool nimcp_brain_factory_init_homeostatic_plasticity_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_dendritic_computation_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_biological_predictive_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_training_subsystem(brain_t brain);

// Wave 14: FEP + medulla
extern bool nimcp_brain_factory_init_fep_orchestrator_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_medulla_subsystem(brain_t brain);

// Wave 15: Hypothalamus + entorhinal + sensory
extern bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_entorhinal_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_somatosensory_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_olfactory_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_gustatory_subsystem(brain_t brain);

// Wave 16: White matter, IC, cortical interneurons
extern bool nimcp_brain_factory_init_white_matter_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_inferior_colliculus_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_cortical_interneurons_subsystem(brain_t brain);

// Wave 17: Neuropeptide, spinal
extern bool nimcp_brain_factory_init_neuropeptide_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_spinal_cord_subsystem(brain_t brain);

// Wave 18: ECB, glymphatic
extern bool nimcp_brain_factory_init_endocannabinoid_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_glymphatic_subsystem(brain_t brain);

// Wave 19: Cross-cutting utilities
extern bool nimcp_brain_factory_init_fuzzy_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_creative_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_parietal_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_intuition_subsystem(brain_t brain);

// Wave 20: Dragonfly, fault tolerance, octopus, amygdala
extern bool nimcp_brain_factory_init_dragonfly_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_fault_tolerance_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_octopus_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_amygdala_subsystem(brain_t brain);
/* Phase 4n-q: JEPA brain-bridges wrapper. The init function is void-
 * returning; trampoline to the bool-returning TASK contract here. */
extern void nimcp_jepa_brain_bridges_init(brain_t brain);
static bool _jepa_bridges_init_task(brain_t brain) {
    nimcp_jepa_brain_bridges_init(brain);
    return true;
}
/* Round A/2: perception bridges activation. Trampoline matches pattern. */
extern bool nimcp_brain_factory_init_perception_bridges_subsystem(brain_t brain);

// Wave 21: Health + state
extern bool nimcp_brain_factory_init_health_agent_subsystem(brain_t brain);
extern bool brain_init_state_manager(brain_t brain);

// Wave 22: BG, directives, KG
extern bool nimcp_brain_factory_init_basal_ganglia_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_core_directives_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_kg_reader_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_internal_kg_subsystem(brain_t brain);

// Wave 32 (W5 KG retrofit): 6 network-type KG structural wirings (LNN, SNN,
// CNN, FNO, HNN, main ANN) + per-network brain-backpointer install. Runs
// after Wave 31 (region KG bridges), also null-tolerant if KG disabled.
extern bool nimcp_brain_factory_init_network_kg_wiring_subsystem(brain_t brain);

// Wave 31 (W3 KG retrofit): region KG structural wirings (occipital, somato,
// motor, broca, gustatory, olfactory, brainstem, raphe, parahippocampal,
// perirhinal). Depends on Wave 22 internal_kg.
extern bool nimcp_brain_factory_init_region_kg_bridges_subsystem(brain_t brain);

// Wave 23-26: Coordinators and bridges
extern bool nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_stdp_omni_bridge_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_stdp_pr_bridge_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_eligibility_pr_bridge_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_stdp_quantum_bridge_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_immune_bridge_coordinator_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_security_perception_bridge_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_swarm_module_registry_subsystem(brain_t brain);

// Wave 27: Cognitive engines (rcog, inner dialogue, reasoning, imagination)
extern bool nimcp_brain_factory_init_rcog_engine_subsystem(brain_t brain);
/* Slice B: cross-modal population binding registry. */
extern bool nimcp_brain_factory_init_concept_registry_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_inner_dialogue_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_reasoning_engine_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_imagination_subsystem(brain_t brain);
extern bool nimcp_brain_factory_init_collective_cognition_subsystem(brain_t brain);

// Wave 28: Cognitive training subsystems (JEPA, predictive hierarchy, self-heal)
extern bool nimcp_brain_factory_init_cognitive_training_subsystem(brain_t brain);

// Wave 29: Edge/robot integration (sensor hub, safety watchdog, swarm bridges)
extern bool nimcp_brain_factory_init_edge_subsystem(brain_t brain);

// Emotional + spike analysis + Shannon headers pulled in via nimcp_brain_internal.h
// Additional headers for inline init functions
#include "information/nimcp_shannon.h"
#include "middleware/brain_integration.h"

//=============================================================================
// Thread Pool Task Wrapper
//=============================================================================

static void parallel_init_task(void* arg) {
    init_task_t* task = (init_task_t*)arg;
    if (atomic_load(&task->ctx->error_flag)) return;  // Skip if prior failure

    bool ok = task->init_fn(task->ctx->brain);
    if (!ok) {
        bool expected = false;
        if (atomic_compare_exchange_strong(&task->ctx->error_flag, &expected, true)) {
            task->ctx->failed_subsystem = task->name;
        }
    }
}

//=============================================================================
// Wave Executor
//=============================================================================

/**
 * Submit tasks to pool, wait, check error flag.
 * Returns false if any task in this wave (or prior) failed.
 */
static bool execute_wave(nimcp_thread_pool_t* pool, parallel_init_ctx_t* ctx,
                         init_task_t* tasks, size_t count, int wave_id) {
    if (atomic_load(&ctx->error_flag)) return false;
    if (count == 0) return true;

    LOG_MODULE_DEBUG(LOG_MODULE, "Wave %d/27 (%zu tasks)...", wave_id, count);

    for (size_t i = 0; i < count; i++) {
        nimcp_pool_submit(pool, parallel_init_task, &tasks[i]);
    }
    nimcp_pool_wait(pool);

    if (atomic_load(&ctx->error_flag)) {
        atomic_store(&ctx->first_error_wave, wave_id);
        LOG_ERROR(LOG_MODULE, "Wave %d failed: subsystem '%s'",
                  wave_id, ctx->failed_subsystem ? ctx->failed_subsystem : "unknown");
        return false;
    }
    return true;
}

/**
 * Run a single init serially (for chain dependencies within a wave).
 * Checks + sets error flag.
 */
static bool run_serial(parallel_init_ctx_t* ctx, bool (*fn)(brain_t), const char* name) {
    if (atomic_load(&ctx->error_flag)) return false;

    LOG_MODULE_DEBUG(LOG_MODULE, "Init: %s...", name);

    if (!fn(ctx->brain)) {
        bool expected = false;
        if (atomic_compare_exchange_strong(&ctx->error_flag, &expected, true)) {
            ctx->failed_subsystem = name;
        }
        return false;
    }
    return true;
}

//=============================================================================
// Conditional Init Wrappers
//=============================================================================

static bool init_glial_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_glial_init) return true;
    return nimcp_brain_factory_init_glial_subsystem(brain);
}

static bool init_axon_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_axon_init) return true;
    return nimcp_brain_factory_init_axon_subsystem(brain);
}

static bool init_dendrite_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_dendrite_init) return true;
    return nimcp_brain_factory_init_dendrite_subsystem(brain);
}

static bool init_multimodal_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    if (brain->config.lazy_visual_init && brain->config.lazy_audio_init &&
        brain->config.lazy_speech_init) return true;
    // Original code: skip if ALL three lazy flags are set (inverted logic)
    if (brain->config.lazy_visual_init || brain->config.lazy_audio_init ||
        brain->config.lazy_speech_init) {
        // Original sequential code only inits if NONE are set
        // Replicate: if any is lazy, skip the multimodal bundle
        return true;
    }
    return nimcp_brain_factory_init_multimodal_subsystems(brain);
}

static bool init_neuromod_chain(brain_t brain) {
    // Neuromod → spatial_neuromod → nuclei (serial chain within wave)
    if (!(brain->config.lazy_init_mode || brain->config.lazy_neuromod_init)) {
        if (!nimcp_brain_factory_init_neuromodulator_system(brain)) return false;
        if (!nimcp_brain_factory_init_spatial_neuromod_system(brain)) return false;
    }
    if (!nimcp_brain_factory_init_neuromod_nuclei(brain)) return false;
    return true;
}

static bool init_symbolic_logic_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_symbolic_logic_subsystem(brain);
}

static bool init_working_memory_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_working_memory_init) return true;
    return nimcp_brain_factory_init_working_memory_subsystem(brain);
}

static bool init_cortical_columns_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_cortical_init) return true;
    return nimcp_brain_factory_init_cortical_columns_subsystem(brain);
}

static bool init_attention_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_attention_subsystem(brain);
}

static bool init_consolidation_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_consolidation_init) return true;
    return nimcp_brain_factory_init_consolidation_subsystem(brain);
}

static bool init_pr_memory_if_needed(brain_t brain) {
    if (!brain->config.enable_pr_memory || brain->config.lazy_init_mode ||
        brain->config.lazy_pr_memory_init) return true;
    return nimcp_brain_factory_init_pr_memory_subsystem(brain);
}

static bool init_chemistry_if_needed(brain_t brain) {
    /* Chemistry cluster: 4 modules driven by one BRAIN_CYCLE_CHEMISTRY tick.
     * Same no-op-on-absent-coordinator behavior as biology. */
    if (brain->config.lazy_init_mode) return true;
    (void)nimcp_brain_factory_init_chemistry_subsystem(brain);
    return true;
}

static bool init_biology_if_needed(brain_t brain) {
    /* Biology cluster: epigenetics, neurogenesis, neurovascular. All three
     * are driven periodically via the cycle coordinator. If the coordinator
     * is absent (enable_cycle_coordinator=false), the factory function
     * returns false and we report that via the wave result — it's not a
     * hard error (no consumer code crashes), just leaves the modules as
     * NULL like they were before this wave existed. */
    if (brain->config.lazy_init_mode) return true;
    /* Treat init failure as soft: the wave continues. */
    (void)nimcp_brain_factory_init_biology_subsystem(brain);
    return true;
}

static bool init_cochlea_if_needed(brain_t brain) {
    /* Wave 8A cochlea + 15 consumer bridges. Runs after Wave 14 so immune
     * (Wave 11) and fep_orchestrator (Wave 14) prereqs are live. Any
     * still-NULL dep (e.g. broca, rcog_engine which init later) just
     * leaves its bridge NULL — not a hard error. Soft failure. */
    if (brain->config.lazy_init_mode) return true;
    (void)nimcp_brain_factory_init_cochlea_subsystem(brain);
    return true;
}

static bool init_physics_bridges_if_needed(brain_t brain) {
    /* Wave 8C — 4 HALF-STATUE physics bridges (ephaptic_bio_async,
     * ephaptic_fft, hh_bio_async, thermo_bio_async) + their 3 upstream
     * modules. All deps (bio_router Wave 13) are live by Wave 15. Any
     * missing dep results in a skipped bridge — not a hard error. */
    if (brain->config.lazy_init_mode) return true;
    (void)nimcp_brain_factory_init_physics_bridges_subsystem(brain);
    return true;
}

static bool init_predictive_immune_if_needed(brain_t brain) {
    /* Predictive-immune coupling. Runs in Wave 12 (after Wave 8 predictive
     * and Wave 11 immune have created their prerequisites). If either
     * prereq is NULL, the factory skips cleanly. Treat failure as soft —
     * the module stays a statue rather than breaking the wave. */
    if (brain->config.lazy_init_mode) return true;
    (void)nimcp_brain_factory_init_predictive_immune_subsystem(brain);
    return true;
}

static bool init_region_cycles_if_needed(brain_t brain) {
    /* Register 6 tick drivers (neuromod/sensorimotor/language/physics_bridges
     * + meta_learning + intuitive_physics) as driven coordinator cycles.
     * Runs late (after all upstream modules are created) so the tick wrappers
     * find live subsystems. Soft-fail: if coord is absent or any single
     * register_driven rejects, the other drivers still register via the
     * hot-path hooks — no module is silently orphaned. */
    if (brain->config.lazy_init_mode) return true;
    (void)nimcp_brain_factory_init_region_cycles_subsystem(brain);
    return true;
}

static bool init_lnn_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.fast_training_mode) return true;
    return nimcp_brain_factory_init_lnn_subsystem(brain);
}

static bool init_world_model_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_world_model_init) return true;
    if (brain->config.enable_world_model || !brain->config.fast_training_mode) {
        brain->config.enable_world_model = true;
        return nimcp_brain_factory_init_world_model_subsystem(brain);
    }
    return true;
}

static bool init_executive_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_executive_init) return true;
    return nimcp_brain_factory_init_executive_subsystem(brain);
}

static bool init_tom_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_theory_of_mind_init) return true;
    return nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
}

static bool init_meta_learning_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_meta_learning_init) return true;
    return nimcp_brain_factory_init_meta_learning_subsystem(brain);
}

static bool init_mirror_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_mirror_neurons_init) return true;
    return nimcp_brain_factory_init_mirror_neurons(brain);
}

static bool init_curiosity_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_curiosity_subsystem(brain);
}

static bool init_salience_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_salience_subsystem(brain);
}

static bool init_ethics_triad(brain_t brain) {
    // Ethics → empathy → empathetic response (serial chain)
    if (brain->config.lazy_init_mode || brain->config.lazy_ethics_init ||
        brain->config.minimal_mode) return true;
    if (!nimcp_brain_factory_init_ethics_engine_subsystem(brain)) return false;
    if (!nimcp_brain_factory_init_empathy_network_subsystem(brain)) return false;
    if (!nimcp_brain_factory_init_empathetic_response_subsystem(brain)) return false;
    return true;
}

static bool init_introspection_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_introspection_subsystem(brain);
}

static bool init_global_workspace_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_global_workspace_init) return true;
    return nimcp_brain_factory_init_global_workspace_subsystem(brain);
}

static bool init_self_model_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_self_model_subsystem(brain);
}

static bool init_fep_orchestrator_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_fep_orchestrator_subsystem(brain);
}

static bool init_somatosensory_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_somatosensory_subsystem(brain);
}

static bool init_olfactory_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_olfactory_subsystem(brain);
}

static bool init_gustatory_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_gustatory_subsystem(brain);
}

static bool init_creative_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode || brain->config.lazy_creative_init) return true;
    return nimcp_brain_factory_init_creative_subsystem(brain);
}

static bool init_parietal_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_parietal_subsystem(brain);
}

static bool init_intuition_if_needed(brain_t brain) {
    if (brain->config.lazy_init_mode) return true;
    return nimcp_brain_factory_init_intuition_subsystem(brain);
}

static bool init_core_directives_if_needed(brain_t brain) {
    if (brain->config.minimal_mode) return true;
    return nimcp_brain_factory_init_core_directives_subsystem(brain);
}

//=============================================================================
// Inline Field Initialization (Wave 4 — too lightweight for thread pool)
//=============================================================================

static bool init_inline_fields(brain_t brain) {
    // Wellbeing monitoring
    brain->wellbeing_monitoring_enabled = true;
    brain->wellbeing_check_interval_ms = 0;
    brain->last_wellbeing_check_time = 0;
    memset(&brain->last_distress, 0, sizeof(distress_assessment_t));
    brain->last_distress.type = DISTRESS_NONE;
    brain->last_distress.severity = DISTRESS_SEVERITY_NORMAL;

    // Simulation time
    brain->current_time_us = 0;
    brain->last_glial_update_us = 0;
    brain->glial_update_counter = 0;

    // Default input modality: text only
    brain->active_modalities = BRAIN_MODALITY_TEXT;

    /* 40-watt brain: Only allocate spike analysis for SNN networks */
    if (brain->snn_network) {
        brain->enable_spike_analysis = true;
        brain->enable_population_coding = true;
        brain->spike_feature_extractor = brain_create_spike_feature_extractor(1000, true, true);
        brain->population_analyzer = brain_create_population_analyzer();
    } else {
        brain->enable_spike_analysis = false;
        brain->enable_population_coding = false;
        brain->spike_feature_extractor = NULL;
        brain->population_analyzer = NULL;
    }
    brain->quantum_annealer = NULL;

    // Shannon info theory
    brain->shannon_config = shannon_default_config();
    brain->enable_shannon_monitoring = false;
    memset(&brain->last_shannon_metrics, 0, sizeof(shannon_network_metrics_t));

    // Quantum-Shannon diffusion
    brain->quantum_shannon_diffusion = NULL;
    brain->enable_quantum_shannon_diffusion = false;
    brain->quantum_shannon_mixing_ratio = 0.2F;
    brain->quantum_shannon_evolution_steps = 100;
    memset(&brain->last_quantum_shannon_metrics, 0, sizeof(shannon_diffusion_metrics_t));

    // Cross-modal info flow
    brain->cross_modal_graph = NULL;
    brain->enable_cross_modal_monitoring = false;
    memset(&brain->last_cross_modal_metrics, 0, sizeof(multi_modal_integration_t));
    brain->cross_modal_bottleneck_threshold = 0.5F;
    brain->cross_modal_sample_count = 50;

    /* 40-watt brain: Emotional systems are optional */
    if (!brain->config.minimal_mode) {
        brain->shadow_emotions = shadow_system_create(8);
        if (!brain->shadow_emotions) { LOG_WARN(LOG_MODULE, "Shadow emotions creation failed"); }
        brain->bias_detection = bias_system_create(8);
        if (!brain->bias_detection) { LOG_WARN(LOG_MODULE, "Bias detection creation failed"); }
        brain->grief_system = grief_system_create();
        if (!brain->grief_system) { LOG_WARN(LOG_MODULE, "Grief system creation failed"); }
        brain->joy_system = joy_system_create();
        if (!brain->joy_system) { LOG_WARN(LOG_MODULE, "Joy system creation failed"); }
        brain->remorse_system = remorse_regret_system_create();
        if (!brain->remorse_system) { LOG_WARN(LOG_MODULE, "Remorse system creation failed"); }
        brain->social_bond_system = social_bond_system_create();
        if (!brain->social_bond_system) { LOG_WARN(LOG_MODULE, "Social bond system creation failed"); }
    }

    return true;  // Non-fatal failures logged above
}

//=============================================================================
// Bio-Async Init (Wave 13 — serial, global singleton)
//=============================================================================

static bool init_bio_async_serial(brain_t brain) {
    if (brain->config.minimal_mode) return true;

    if (!bio_router_is_initialized()) {
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 128;
        router_config.outbox_capacity = 128;
        router_config.max_message_size = 4096;
        router_config.worker_threads = 2;
        router_config.enable_logging = true;
        router_config.enable_statistics = true;
        router_config.routing_timeout_ms = 100.0F;

        nimcp_error_t router_err = bio_router_init(&router_config);
        if (router_err != NIMCP_SUCCESS) {
            LOG_WARN(LOG_MODULE, "Bio-router init failed! Bio-async disabled.");
            brain->bio_async_enabled = false;
            return true;  // Non-fatal
        }
    }

    if (bio_router_is_initialized()) {
        nimcp_error_t async_err = brain_bio_async_init(brain);
        if (async_err != NIMCP_SUCCESS) {
            LOG_WARN(LOG_MODULE, "Brain bio-async init failed! Async disabled.");
            brain->bio_async_enabled = false;
        } else {
            brain->bio_async_enabled = true;
        }
    }
    return true;
}

//=============================================================================
// Signal Handler (Wave 11 — serial, after immune)
//=============================================================================

static bool init_signal_handler(brain_t brain) {
    (void)brain;
    signal_handler_config_t sig_cfg = signal_handler_default_config();
    sig_cfg.enable_stack_trace = true;
    sig_cfg.enable_checkpoint_save = true;
    if (!signal_handler_install(&sig_cfg)) {
        LOG_WARN(LOG_MODULE, "Signal handler installation failed");
    }
    return true;  // Non-fatal
}

//=============================================================================
// Main Entry Point
//=============================================================================

#define MAX_WAVE_TASKS 8  // Max parallel tasks in any single wave

#define TASK(fn, nm) (init_task_t){ .ctx = &ctx, .init_fn = (fn), .name = (nm) }

bool nimcp_brain_parallel_init_subsystems(brain_t brain, const brain_config_t* config) {
    if (!brain || !config) return false;

    // Create thread pool for init
    uint32_t num_threads = config->init_threads;
    if (num_threads == 0) num_threads = 4;
    if (num_threads > 16) num_threads = 16;  // Cap for init (don't need 64 threads)

    nimcp_thread_pool_t* pool = nimcp_pool_create(num_threads);
    if (!pool) {
        LOG_WARN(LOG_MODULE, "Thread pool creation failed, falling back to serial");
        return false;  // Caller will use sequential fallback
    }

    parallel_init_ctx_t ctx = {
        .brain = brain,
        .error_flag = false,
        .first_error_wave = -1,
        .failed_subsystem = NULL
    };

    init_task_t tasks[MAX_WAVE_TASKS];
    size_t n;
    bool ok = true;

    bool fast_mode = (config->init_mode == BRAIN_INIT_FAST);
    LOG_INFO(LOG_MODULE, "Starting %s parallel subsystem init with %u threads",
             fast_mode ? "FAST" : "FULL", num_threads);

    // ========================================================================
    // WAVE 0 (serial): GPU context + inference + pool
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_gpu_subsystem, "gpu_context");
    if (ok) ok = run_serial(&ctx, nimcp_brain_factory_init_gpu_inference, "gpu_inference");
    if (ok) {
        brain->inference_pool = NULL;
        brain->frozen = false;
        if (!brain->config.force_serial_inference) {
            uint32_t pt = brain->config.inference_threads;
            if (pt == 0) pt = 4;
            brain->inference_pool = nimcp_pool_create(pt);
        }
    }
    if (!ok) goto cleanup;

    // ========================================================================
    // FAST MODE: Skip non-essential waves. Only init training-critical systems.
    // All skipped subsystems will be lazy-initialized on first access.
    // ========================================================================
    if (fast_mode) {
        // Wave 4 (inline): Lightweight field inits + emotional systems
        ok = run_serial(&ctx, init_inline_fields, "inline_fields");
        if (!ok) goto cleanup;

        // Wave 11 (serial chain): security → immune → signal_handler
        ok = run_serial(&ctx, nimcp_brain_factory_init_security_subsystem, "security");
        if (ok) ok = run_serial(&ctx, nimcp_brain_factory_init_immune_subsystem, "immune");
        if (ok) ok = run_serial(&ctx, init_signal_handler, "signal_handler");
        if (!ok) goto cleanup;

        // Wave 12: training pipeline + plasticity (parallel)
        n = 0;
        tasks[n++] = TASK(nimcp_brain_factory_init_homeostatic_plasticity_subsystem, "homeostatic_plasticity");
        tasks[n++] = TASK(nimcp_brain_factory_init_dendritic_computation_subsystem, "dendritic_computation");
        tasks[n++] = TASK(nimcp_brain_factory_init_biological_predictive_subsystem, "bio_predictive");
        tasks[n++] = TASK(nimcp_brain_factory_init_training_subsystem, "training");
        if (!execute_wave(pool, &ctx, tasks, n, 12)) goto cleanup;

        // Wave 13 (serial): bio-router + bio-async init
        ok = run_serial(&ctx, init_bio_async_serial, "bio_async");
        if (!ok) goto cleanup;

        // Wave 23-24 (serial): bio_async_orchestrator → plasticity_coordinator
        ok = run_serial(&ctx, nimcp_brain_factory_init_bio_async_orchestrator_subsystem, "bio_async_orchestrator");
        if (ok) ok = run_serial(&ctx, nimcp_brain_factory_init_plasticity_coordinator_subsystem, "plasticity_coordinator");
        if (!ok) goto cleanup;

        // Wave 25: STDP bridges (parallel)
        n = 0;
        tasks[n++] = TASK(nimcp_brain_factory_init_stdp_omni_bridge_subsystem, "stdp_omni_bridge");
        tasks[n++] = TASK(nimcp_brain_factory_init_stdp_pr_bridge_subsystem, "stdp_pr_bridge");
        tasks[n++] = TASK(nimcp_brain_factory_init_eligibility_pr_bridge_subsystem, "eligibility_pr_bridge");
        tasks[n++] = TASK(nimcp_brain_factory_init_stdp_quantum_bridge_subsystem, "stdp_quantum_bridge");
        if (!execute_wave(pool, &ctx, tasks, n, 25)) goto cleanup;

        // Sparse coding: K-WTA sparsity enforcement on output layer (lightweight)
        {
            sparse_coding_config_t sc_config;
            cortical_sparse_default_config(&sc_config);
            sc_config.sparsity_method = SPARSITY_METHOD_K_WTA;
            sc_config.target_sparsity = 0.05f;
            sc_config.num_columns = brain->config.num_outputs > 0 ? brain->config.num_outputs : 4096;
            sc_config.k_winners = (uint32_t)(sc_config.num_columns * sc_config.target_sparsity);
            if (sc_config.k_winners < 8) sc_config.k_winners = 8;
            sc_config.enable_homeostasis = true;
            sc_config.adaptation_rate = 0.002f;
            sc_config.enable_bio_async = false;
            sc_config.enable_lateral_inhibition = false;
            brain->sparse_coding_system = cortical_sparse_create(&sc_config);
            brain->enable_sparse_coding = (brain->sparse_coding_system != NULL);
        }

        LOG_MODULE_DEBUG(LOG_MODULE, "FAST init complete (6 waves)");
        LOG_INFO(LOG_MODULE, "FAST parallel init complete (6 waves, skipped ~20 non-essential)");
        goto cleanup;
    }

    // ========================================================================
    // WAVE 1: glial, axon, dendrite, multimodal, pink_noise
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_glial_if_needed, "glial");
    tasks[n++] = TASK(init_axon_if_needed, "axon");
    tasks[n++] = TASK(init_dendrite_if_needed, "dendrite");
    tasks[n++] = TASK(init_multimodal_if_needed, "multimodal");
    tasks[n++] = TASK(nimcp_brain_factory_init_pink_noise_subsystem, "pink_noise");
    if (!execute_wave(pool, &ctx, tasks, n, 1)) goto cleanup;

    // ========================================================================
    // WAVE 2 (serial chain): neuromod → spatial_neuromod → nuclei
    // ========================================================================
    ok = run_serial(&ctx, init_neuromod_chain, "neuromod_chain");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 3: substrate_gpu, substrate_thalamic, symbolic_logic, symbolic_reasoning, working_memory
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_substrate_gpu_subsystem, "substrate_gpu");
    tasks[n++] = TASK(nimcp_brain_factory_init_substrate_thalamic_subsystem, "substrate_thalamic");
    tasks[n++] = TASK(init_symbolic_logic_if_needed, "symbolic_logic");
    tasks[n++] = TASK(nimcp_brain_factory_init_symbolic_reasoning_subsystem, "symbolic_reasoning");
    tasks[n++] = TASK(init_working_memory_if_needed, "working_memory");
    if (!execute_wave(pool, &ctx, tasks, n, 3)) goto cleanup;

    // ========================================================================
    // WAVE 4 (inline): Lightweight field inits + emotional systems
    // ========================================================================
    ok = run_serial(&ctx, init_inline_fields, "inline_fields");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 5: attention, brain_regions, cortical_columns, epistemic
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_attention_if_needed, "attention");
    tasks[n++] = TASK(nimcp_brain_factory_init_brain_regions_subsystem, "brain_regions");
    tasks[n++] = TASK(init_cortical_columns_if_needed, "cortical_columns");
    tasks[n++] = TASK(nimcp_brain_factory_init_epistemic_subsystem, "epistemic");
    if (!execute_wave(pool, &ctx, tasks, n, 5)) goto cleanup;

    // ========================================================================
    // WAVE 6: consolidation, pr_memory, lnn, world_model, biology
    //
    // NOTE: biology (epigenetics + neurogenesis + NVC) also uses
    // register_driven like pr_memory. Grouped here since both depend on the
    // coordinator being available and share "per-neuron state evolution"
    // semantics. Graceful no-op if coordinator hasn't been created yet.
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_consolidation_if_needed, "consolidation");
    tasks[n++] = TASK(init_pr_memory_if_needed, "pr_memory");
    tasks[n++] = TASK(init_lnn_if_needed, "lnn");
    tasks[n++] = TASK(init_world_model_if_needed, "world_model");
    tasks[n++] = TASK(init_biology_if_needed, "biology");
    tasks[n++] = TASK(init_chemistry_if_needed, "chemistry");
    if (!execute_wave(pool, &ctx, tasks, n, 6)) goto cleanup;

    // ========================================================================
    // WAVE 7: executive, ToM, natural_explanations, meta_learning
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_executive_if_needed, "executive");
    tasks[n++] = TASK(init_tom_if_needed, "theory_of_mind");
    tasks[n++] = TASK(nimcp_brain_factory_init_natural_explanations_subsystem, "natural_explanations");
    tasks[n++] = TASK(init_meta_learning_if_needed, "meta_learning");
    if (!execute_wave(pool, &ctx, tasks, n, 7)) goto cleanup;

    // ========================================================================
    // WAVE 8: mental_health, predictive, mirror, curiosity, salience
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_mental_health_subsystem, "mental_health");
    tasks[n++] = TASK(nimcp_brain_factory_init_trauma_resilience, "trauma_resilience");
    tasks[n++] = TASK(nimcp_brain_factory_init_predictive_subsystem, "predictive");
    tasks[n++] = TASK(init_mirror_if_needed, "mirror_neurons");
    tasks[n++] = TASK(init_curiosity_if_needed, "curiosity");
    if (!execute_wave(pool, &ctx, tasks, n, 8)) goto cleanup;

    // Salience in a separate mini-wave (exceeded MAX_WAVE_TASKS=8 with 5 tasks is fine,
    // but let's keep it in same wave — bump to second submit batch)
    n = 0;
    tasks[n++] = TASK(init_salience_if_needed, "salience");
    if (!execute_wave(pool, &ctx, tasks, n, 8)) goto cleanup;

    // ========================================================================
    // WAVE 9: ethics_triad, introspection, connectivity, middleware
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_ethics_triad, "ethics_triad");
    tasks[n++] = TASK(init_introspection_if_needed, "introspection");
    tasks[n++] = TASK(nimcp_brain_factory_init_connectivity_health_subsystem, "connectivity_health");
    tasks[n++] = TASK(nimcp_brain_factory_init_middleware_controller_subsystem, "middleware");
    if (!execute_wave(pool, &ctx, tasks, n, 9)) goto cleanup;

    // ========================================================================
    // WAVE 10: autobio_memory, self_model, global_workspace
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_autobiographical_memory_subsystem, "autobio_memory");
    tasks[n++] = TASK(init_self_model_if_needed, "self_model");
    tasks[n++] = TASK(init_global_workspace_if_needed, "global_workspace");
    if (!execute_wave(pool, &ctx, tasks, n, 10)) goto cleanup;

    // ========================================================================
    // WAVE 11 (serial chain): security → immune → signal_handler
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_security_subsystem, "security");
    if (ok) ok = run_serial(&ctx, nimcp_brain_factory_init_immune_subsystem, "immune");
    if (ok) ok = run_serial(&ctx, init_signal_handler, "signal_handler");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 12: homeostatic_plasticity, dendritic_computation, bio_predictive,
    //          training, predictive_immune
    //
    // predictive_immune placed here because it depends on predictive_network
    // (Wave 8) + immune_system (Wave 11) both being live. Wave 12 is the
    // earliest wave where both prereqs exist. Uses register_driven via the
    // cycle coordinator, same semantics as biology and pr_memory.
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_homeostatic_plasticity_subsystem, "homeostatic_plasticity");
    tasks[n++] = TASK(nimcp_brain_factory_init_dendritic_computation_subsystem, "dendritic_computation");
    tasks[n++] = TASK(nimcp_brain_factory_init_biological_predictive_subsystem, "bio_predictive");
    tasks[n++] = TASK(nimcp_brain_factory_init_training_subsystem, "training");
    tasks[n++] = TASK(init_predictive_immune_if_needed, "predictive_immune");
    if (!execute_wave(pool, &ctx, tasks, n, 12)) goto cleanup;

    // ========================================================================
    // WAVE 13 (serial): bio-router + bio-async init
    // ========================================================================
    ok = run_serial(&ctx, init_bio_async_serial, "bio_async");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 14: fep_orchestrator, medulla
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(init_fep_orchestrator_if_needed, "fep_orchestrator");
    tasks[n++] = TASK(nimcp_brain_factory_init_medulla_subsystem, "medulla");
    if (!execute_wave(pool, &ctx, tasks, n, 14)) goto cleanup;

    // ========================================================================
    // WAVE 15: hypothalamus + entorhinal (serial), then sensory (parallel)
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_hypothalamus_subsystem, "hypothalamus");
    if (!ok) goto cleanup;

    // Entorhinal (minimal adapter) runs adjacent to hypothalamus. Non-fatal
    // on failure — the tick driver null-guards on brain->entorhinal.
    ok = run_serial(&ctx, nimcp_brain_factory_init_entorhinal_subsystem, "entorhinal");
    if (!ok) {
        LOG_WARNING(LOG_MODULE, "Entorhinal init failed — continuing (non-fatal)");
        atomic_store(&ctx.error_flag, false);  // Clear flag; non-fatal
        ok = true;
    }

    n = 0;
    tasks[n++] = TASK(init_somatosensory_if_needed, "somatosensory");
    tasks[n++] = TASK(init_olfactory_if_needed, "olfactory");
    tasks[n++] = TASK(init_gustatory_if_needed, "gustatory");
    /* Wave 8A cochlea + 15 bridges — runs here because all prereqs (immune
     * Wave 11, fep Wave 14, medulla Wave 14, hypothalamus Wave 15 serial
     * above) are now live. Bridges with still-NULL deps (broca Wave later,
     * rcog/collective Wave 27) just stay NULL. */
    tasks[n++] = TASK(init_cochlea_if_needed, "cochlea");
    /* Wave 8C physics bridges — runs alongside cochlea in Wave 15 (same
     * prereq: bio_router Wave 13). Soft-fail on any missing dep. */
    tasks[n++] = TASK(init_physics_bridges_if_needed, "physics_bridges");
    if (!execute_wave(pool, &ctx, tasks, n, 15)) goto cleanup;

    // ========================================================================
    // WAVE 16: white_matter, inferior_colliculus, cortical_interneurons
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_white_matter_subsystem, "white_matter");
    tasks[n++] = TASK(nimcp_brain_factory_init_inferior_colliculus_subsystem, "inferior_colliculus");
    tasks[n++] = TASK(nimcp_brain_factory_init_cortical_interneurons_subsystem, "cortical_interneurons");
    if (!execute_wave(pool, &ctx, tasks, n, 16)) goto cleanup;

    // ========================================================================
    // WAVE 17: neuropeptide, spinal_cord
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_neuropeptide_subsystem, "neuropeptide");
    tasks[n++] = TASK(nimcp_brain_factory_init_spinal_cord_subsystem, "spinal_cord");
    if (!execute_wave(pool, &ctx, tasks, n, 17)) goto cleanup;

    // ========================================================================
    // WAVE 18: endocannabinoid, glymphatic
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_endocannabinoid_subsystem, "endocannabinoid");
    tasks[n++] = TASK(nimcp_brain_factory_init_glymphatic_subsystem, "glymphatic");
    if (!execute_wave(pool, &ctx, tasks, n, 18)) goto cleanup;

    // ========================================================================
    // WAVE 19: fuzzy, creative, parietal, intuition
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_fuzzy_subsystem, "fuzzy");
    tasks[n++] = TASK(init_creative_if_needed, "creative");
    tasks[n++] = TASK(init_parietal_if_needed, "parietal");
    tasks[n++] = TASK(init_intuition_if_needed, "intuition");
    if (!execute_wave(pool, &ctx, tasks, n, 19)) goto cleanup;

    // ========================================================================
    // WAVE 20: dragonfly, fault_tolerance
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_dragonfly_subsystem, "dragonfly");
    tasks[n++] = TASK(nimcp_brain_factory_init_fault_tolerance_subsystem, "fault_tolerance");
    tasks[n++] = TASK(nimcp_brain_factory_init_octopus_subsystem, "octopus");
    tasks[n++] = TASK(nimcp_brain_factory_init_amygdala_subsystem, "amygdala");
    /* Phase 4n-q: JEPA bridges. Trampoline declared at file scope. */
    tasks[n++] = TASK(_jepa_bridges_init_task, "jepa_bridges");
    /* Round A/2: perception bridges (FEP + immune + cortical + pr_predictive). */
    tasks[n++] = TASK(nimcp_brain_factory_init_perception_bridges_subsystem,
                      "perception_bridges");
    if (!execute_wave(pool, &ctx, tasks, n, 20)) goto cleanup;

    // ========================================================================
    // WAVE 21 (serial chain): health_agent → state_manager
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_health_agent_subsystem, "health_agent");
    if (ok) ok = run_serial(&ctx, brain_init_state_manager, "state_manager");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 22: basal_ganglia, core_directives, kg_reader, internal_kg
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_basal_ganglia_subsystem, "basal_ganglia");
    tasks[n++] = TASK(init_core_directives_if_needed, "core_directives");
    tasks[n++] = TASK(nimcp_brain_factory_init_kg_reader_subsystem, "kg_reader");
    tasks[n++] = TASK(nimcp_brain_factory_init_internal_kg_subsystem, "internal_kg");
    if (!execute_wave(pool, &ctx, tasks, n, 22)) goto cleanup;

    // ========================================================================
    // WAVE 23 (serial): bio_async_orchestrator
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_bio_async_orchestrator_subsystem, "bio_async_orchestrator");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 24 (serial): plasticity_coordinator
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_plasticity_coordinator_subsystem, "plasticity_coordinator");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 25: STDP bridges (all depend on plasticity_coordinator)
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_stdp_omni_bridge_subsystem, "stdp_omni_bridge");
    tasks[n++] = TASK(nimcp_brain_factory_init_stdp_pr_bridge_subsystem, "stdp_pr_bridge");
    tasks[n++] = TASK(nimcp_brain_factory_init_eligibility_pr_bridge_subsystem, "eligibility_pr_bridge");
    tasks[n++] = TASK(nimcp_brain_factory_init_stdp_quantum_bridge_subsystem, "stdp_quantum_bridge");
    if (!execute_wave(pool, &ctx, tasks, n, 25)) goto cleanup;

    // ========================================================================
    // WAVE 26: immune_bridge, cognitive_meta, security_perception, swarm
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_immune_bridge_coordinator_subsystem, "immune_bridge_coord");
    tasks[n++] = TASK(nimcp_brain_factory_init_cognitive_meta_controller_subsystem, "cognitive_meta_ctrl");
    tasks[n++] = TASK(nimcp_brain_factory_init_security_perception_bridge_subsystem, "security_perception");
    tasks[n++] = TASK(nimcp_brain_factory_init_swarm_module_registry_subsystem, "swarm_registry");
    if (!execute_wave(pool, &ctx, tasks, n, 26)) goto cleanup;

    // ========================================================================
    // WAVE 27: Cognitive engines (rcog, inner dialogue, reasoning, imagination, collective)
    // ========================================================================
    n = 0;
    tasks[n++] = TASK(nimcp_brain_factory_init_rcog_engine_subsystem, "rcog_engine");
    tasks[n++] = TASK(nimcp_brain_factory_init_inner_dialogue_subsystem, "inner_dialogue");
    tasks[n++] = TASK(nimcp_brain_factory_init_reasoning_engine_subsystem, "reasoning_engine");
    tasks[n++] = TASK(nimcp_brain_factory_init_imagination_subsystem, "imagination");
    tasks[n++] = TASK(nimcp_brain_factory_init_collective_cognition_subsystem, "collective_cognition");
    /* Slice B (Option 1 architectural rebuild) — concept registry.
     * Created in the cognitive_engines wave so it's available before any
     * brain_learn_vector call. NULL-tolerant downstream. */
    tasks[n++] = TASK(nimcp_brain_factory_init_concept_registry_subsystem, "concept_registry");
    if (!execute_wave(pool, &ctx, tasks, n, 27)) goto cleanup;

    // ========================================================================
    // WAVE 28: Cognitive training subsystems (JEPA, predictive hierarchy, self-heal)
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_cognitive_training_subsystem, "cognitive_training");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 29: Edge/robot integration (sensor hub, safety, swarm bridges)
    // DEPENDS ON: Dragonfly (wave 20), Swarm (wave 26), Portia (global)
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_edge_subsystem, "edge");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 30: Region/biology/chemistry driven cycles.
    //
    // Registers 6 already-created tick drivers as coordinator-driven cycles
    // (neuromod/sensorimotor/language/physics_bridges + meta_learning +
    // intuitive_physics). Runs last so every upstream module (medulla Wave 14,
    // LC/VTA/raphe/habenula Wave 14+, broca/wernicke, ephaptic/thermo/hh,
    // meta_learner Wave 7, intuitive_physics Wave 6) is live. Each register
    // is independently null-safe — a missing module leaves the cycle at
    // register_driven's null-tolerant pass. Hot-path hooks continue to
    // advance state even if register_driven fails for any cycle. */
    // ========================================================================
    ok = run_serial(&ctx, init_region_cycles_if_needed, "region_cycles");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 31 (W3 KG retrofit): Region KG structural wirings.
    // Registers structural sub-region nodes for 10 previously-unwired brain
    // regions (occipital/somatosensory/motor/broca/gustatory/olfactory/
    // brainstem/raphe/parahippocampal/perirhinal) into brain->internal_kg.
    // Each region's wiring_init is null-tolerant if internal_kg is disabled.
    // Must run after Wave 22 (internal_kg). See docs/claude/
    // kg-integration-audit-2026-04-24.md §2.1 + kg-node-naming-registry.md.
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_region_kg_bridges_subsystem,
                    "region_kg_bridges");
    if (!ok) goto cleanup;

    // ========================================================================
    // WAVE 32 (W5 KG retrofit): Network-level KG structural wirings.
    // Registers 6 network roots (net_lnn/snn/cnn/fno/hnn/main_ann) + sample
    // SNN population nodes + per-network brain-backpointer install (so the
    // file-scope static emit functions inside each network .c can
    // self-elevate and write event nodes on anomaly). Must run after
    // Wave 22 (internal_kg). See docs/claude/
    // kg-integration-audit-2026-04-24.md §2.12.
    // ========================================================================
    ok = run_serial(&ctx, nimcp_brain_factory_init_network_kg_wiring_subsystem,
                    "network_kg_wiring");
    if (!ok) goto cleanup;

    LOG_MODULE_DEBUG(LOG_MODULE, "Full init complete (32 waves)");
    LOG_INFO(LOG_MODULE, "Parallel subsystem init complete (32 waves + region_cycles + region_kg_bridges + network_kg_wiring)");

cleanup:
    nimcp_pool_destroy(pool);
    return !atomic_load(&ctx.error_flag);
}

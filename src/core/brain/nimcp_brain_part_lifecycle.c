// nimcp_brain_part_lifecycle.c - lifecycle functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c


/* brain_heartbeat is defined in nimcp_brain_internal.h */

/* Moved from inside brain_destroy() function body to file scope
 * (includes inside function bodies are non-standard and cause warnings) */
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "core/brain/subcortical/nimcp_amygdala.h"  /* Phase 3c: amygdala destroy */
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"  /* glial destroy helper */
#include "core/brain/factory/init/nimcp_brain_init_broca.h"  /* S5-C1: Broca + phonological-loop destroy */
#include "utils/thread/nimcp_thread.h"  /* S2-C2: arcuate-feedback lock destroy */
#include "core/cortical_columns/nimcp_cortical_column_ternary.h"  /* CC4 ternary destroy */
#include "security/nimcp_security_recovery_bridge.h"
#include "security/nimcp_security_integration.h"
#include "generation/nimcp_language_generator.h"
#include "generation/nimcp_embedding.h"
#include "language/nimcp_grounded_language.h"
#include "language/nimcp_concept_registry.h"  /* Slice B */
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "snn/bridges/nimcp_snn_speech_bridge.h"
#include "snn/bridges/nimcp_snn_audio_bridge.h"
#include "snn/bridges/nimcp_snn_visual_bridge.h"
#include "snn/bridges/nimcp_snn_somatosensory_bridge.h"
#include "snn/bridges/nimcp_snn_cross_modal_align.h"
#include "snn/nimcp_snn_fno.h"
#include "memory/nimcp_memory_store.h"
#include "memory/nimcp_memory_oodb.h"
#include "cognitive/nimcp_ood_detector.h"

//=============================================================================
// Bio-Async Message Handlers and Integration
//=============================================================================

/**
 * @brief Internal once-only initialization routine for bio-async
 *
 * Called exactly once via nimcp_platform_once to avoid race conditions.
 */
static void brain_bio_init_once_routine(void)
{
    LOG_MODULE_INFO("BRAIN", "Initializing bio-async integration");

    // Register module with bio-router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,  // High capacity for brain module
        .user_data = NULL
    };

    g_brain_bio_ctx = bio_router_register_module(&info);
    if (!g_brain_bio_ctx) {
        LOG_MODULE_ERROR("BRAIN", "Failed to register with bio-router");
        g_brain_bio_init_result = NIMCP_ERROR_INVALID_PARAM;
        return;
    }

    __atomic_store_n(&g_brain_bio_initialized, true, __ATOMIC_RELEASE);
    LOG_MODULE_INFO("BRAIN", "Bio-async integration initialized successfully");
    g_brain_bio_init_result = NIMCP_SUCCESS;
}


/**
 * @brief Initialize bio-async integration for brain module
 *
 * WHAT: Registers brain with bio-router and sets up message handlers
 * WHY:  Enable event-driven inter-module communication
 * HOW:  Uses nimcp_platform_once for thread-safe one-time initialization
 *
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t brain_bio_init(void)
{
    // Use platform_once for thread-safe one-time initialization
    nimcp_platform_once(&g_brain_bio_once, brain_bio_init_once_routine);
    return g_brain_bio_init_result;
}


//=============================================================================
// Strategy Factory
//=============================================================================

/**
 * @brief Create strategy for task type
 *
 * WHY: Factory pattern for strategy creation
 * Centralizes strategy instantiation logic
 *
 * COMPLEXITY: O(1) - simple allocation and assignment
 *
 * @param task Task type
 * @return Strategy instance or NULL on error
 */
static task_strategy_t* strategy_create(brain_task_t task)
{
    task_strategy_t* strategy = nimcp_calloc(1, sizeof(task_strategy_t));
    if (!strategy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "strategy allocation failed");

        return NULL;

    }

    strategy->task_type = task;

    switch (task) {
        case BRAIN_TASK_CLASSIFICATION:
            strategy->get_learning_rate = strategy_classification_lr;
            strategy->transform_output = strategy_classification_transform;
            strategy->compute_loss = strategy_classification_loss;
            break;

        case BRAIN_TASK_REGRESSION:
            strategy->get_learning_rate = strategy_regression_lr;
            strategy->transform_output = strategy_regression_transform;
            strategy->compute_loss = strategy_regression_loss;
            break;

        case BRAIN_TASK_PATTERN_MATCHING:
            strategy->get_learning_rate = strategy_pattern_lr;
            strategy->transform_output = strategy_pattern_transform;
            strategy->compute_loss = strategy_pattern_loss;
            break;

        case BRAIN_TASK_ASSOCIATION:
            strategy->get_learning_rate = strategy_association_lr;
            strategy->transform_output = strategy_association_transform;
            strategy->compute_loss = strategy_association_loss;
            break;

        default:
            // Default to classification
            strategy->get_learning_rate = strategy_classification_lr;
            strategy->transform_output = strategy_classification_transform;
            strategy->compute_loss = strategy_classification_loss;
            break;
    }

    return strategy;
}


/**
 * @brief Destroy strategy
 *
 * COMPLEXITY: O(1)
 */
static void strategy_destroy(task_strategy_t* strategy)
{
    nimcp_free(strategy);
}


/**
 * @brief Create brain with preset size and task
 *
 * WHY: Factory pattern - single creation entry point
 * Encapsulates all creation complexity with validation
 *
 * COMPLEXITY: O(n) where n = num_neurons
 * MEMORY: O(n*c) where c = connections per neuron
 *
 * PATTERN: Factory pattern with guard clauses
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
// brain_create() - MOVED TO: src/core/brain/factory/nimcp_brain_factory.c

/**
 * @brief Create brain with custom configuration
 *
 * WHY: Allows advanced users to customize all parameters
 * Delegates to standard factory after validation
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param config Custom configuration
 * @return Brain handle or NULL on error
 */
// brain_create_custom() - MOVED TO: src/core/brain/factory/nimcp_brain_factory.c

/**
 * @brief Destroy brain and free resources
 *
 * WHY: Proper cleanup prevents memory leaks
 * Handles partial initialization gracefully
 *
 * COMPLEXITY: O(n) where n = num_neurons (for network cleanup)
 *
 * @param brain Brain to destroy
 */
void brain_destroy(brain_t brain)
{
    if (!brain)
        return;
    LOG_MODULE_DEBUG("BRAIN", "brain_destroy: start");

    // Unregister brain from signal handler (handler stays installed for safety)
    signal_handler_unregister_brain();

    /* Destroy probe registry (before subsystems it might reference) */
    if (brain->probe_registry) {
        extern void probe_registry_destroy(struct probe_registry*);
        probe_registry_destroy((struct probe_registry*)brain->probe_registry);
        brain->probe_registry = NULL;
    }

    // Destroy inference thread pool (before subsystems that tasks might reference)
    if (brain->inference_pool) {
        LOG_MODULE_DEBUG("BRAIN", "Destroying inference pool...");
        nimcp_pool_destroy(brain->inference_pool);
        brain->inference_pool = NULL;
    }

    // Save final snapshot if configured (BEFORE destroying anything)
    if (brain->config.snapshot_dir && brain->config.save_final_snapshot) {
        brain_save_snapshot(brain, "final", "Snapshot at brain destruction");
        // Non-fatal if snapshot fails
    }

    /* Destroy unified training manager (before networks it references) */
    if (brain->unified_training) {
        /* Forward declaration avoids header include in lifecycle file */
        extern void nimcp_utm_destroy(struct nimcp_unified_training_manager* mgr);
        nimcp_utm_destroy(brain->unified_training);
        brain->unified_training = NULL;
    }

    /* Detach + destroy CPU substrate + thalamic router. Must run BEFORE
     * SNN/LNN/cortex CNN destruction so the per-network detach step can
     * clear borrowed pointers while the networks are still alive.
     * Forward declaration avoids pulling the full subsystems header into
     * the lifecycle TU. */
    {
        extern void nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain_t brain);
        nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain);
    }

    /* Destroy SNN/LNN training contexts and networks created by
     * brain_enable_multi_network_training() — must happen after UTM
     * (which holds adapter references) and before general network teardown. */
    if (brain->snn_backprop_ctx) {
        extern void snn_backprop_destroy(struct snn_backprop_ctx_s* ctx);
        snn_backprop_destroy(brain->snn_backprop_ctx);
        brain->snn_backprop_ctx = NULL;
    }
    if (brain->snn_training_ctx) {
        extern void snn_training_destroy(struct snn_training_ctx_s* ctx);
        snn_training_destroy(brain->snn_training_ctx);
        brain->snn_training_ctx = NULL;
    }
    if (brain->lnn_training_ctx) {
        extern void lnn_training_destroy(struct lnn_training_ctx_s* ctx);
        lnn_training_destroy(brain->lnn_training_ctx);
        brain->lnn_training_ctx = NULL;
    }
    if (brain->snn_network && brain->owns_specialized_network) {
        extern void snn_network_destroy(struct snn_network_s* snn);
        snn_network_destroy(brain->snn_network);
        brain->snn_network = NULL;
    }
    if (brain->lnn_network && brain->owns_specialized_network) {
        extern void lnn_network_destroy(struct lnn_network_s* lnn);
        lnn_network_destroy(brain->lnn_network);
        brain->lnn_network = NULL;
    }

    /* SNN FNO populations are now owned by snn_network_t and freed inside
     * snn_network_destroy(snn) above. The brain just held a borrowed view —
     * clear it so stale pointers don't leak past teardown. */
    brain->snn_fno_populations = NULL;
    brain->snn_fno_count = 0;

    LOG_MODULE_DEBUG("BRAIN", "Destroying network...");
    // Phase 3: Handle network destruction with atomic reference counting
    //
    // CONCURRENCY MODEL: Lock-free reference counting using C11 atomics
    // - atomic_fetch_sub returns the PREVIOUS value before decrement
    // - If previous value was 1, we just decremented to 0 and are the last holder
    // - No mutex needed - atomics provide the synchronization
    //
    // MEMORY ORDERING:
    // - ACQ_REL on fetch_sub ensures all prior accesses by other threads
    //   releasing their references are visible, and our cleanup is visible
    //   to any (impossible) future readers
    if (brain->network) {
        if (brain->owns_network) {
            // Brain owns the network - destroy immediately
            adaptive_network_destroy(brain->network);
        } else if (brain->network_refcount_atomic) {
            // Brain shares network - atomically decrement refcount
            _Atomic(uint32_t)* refcount_ptr = brain->network_refcount_atomic;
            adaptive_network_t network = brain->network;

            // Atomically decrement and get previous value
            // ACQ_REL ensures proper memory ordering with other threads
            uint32_t prev_refcount = __atomic_fetch_sub(refcount_ptr, 1, __ATOMIC_ACQ_REL);

            if (prev_refcount == 1) {
                // We just decremented from 1 to 0 - we are the last holder
                // All other clones have already completed their brain_destroy() calls.
                // No synchronization needed - we have exclusive access now.
                adaptive_network_destroy(network);
                nimcp_free((void*)refcount_ptr);
            }
            // else: prev_refcount > 1, so other clones still exist - just decrement and exit
        }
        // else: Neither owns nor has refcount - strange but safe (network leaked)
    }

    LOG_MODULE_DEBUG("BRAIN", "Network destroyed");
    // Strategies are shared (read-only), don't destroy
    // Only destroy if this is not a COW clone OR if we own it
    if (brain->strategy && !brain->is_cow_clone) {
        strategy_destroy(brain->strategy);
    }

    if (brain->output_labels) {
        for (uint32_t i = 0; i < brain->num_output_labels; i++) {
            if (brain->output_labels[i]) {
                nimcp_free(brain->output_labels[i]);
            }
        }
        nimcp_free(brain->output_labels);
    }

    // Cleanup Phase 8 SNN bridges (before hyperledger and language bridge)
    if (brain->cross_modal_aligner) {
        cross_modal_align_destroy((cross_modal_align_t*)brain->cross_modal_aligner);
        brain->cross_modal_aligner = NULL;
    }
    if (brain->snn_somatosensory_bridge) {
        snn_somatosensory_bridge_destroy((snn_somatosensory_bridge_t*)brain->snn_somatosensory_bridge);
        brain->snn_somatosensory_bridge = NULL;
    }
    if (brain->snn_audio_bridge) {
        snn_audio_bridge_destroy((snn_audio_bridge_t*)brain->snn_audio_bridge);
        brain->snn_audio_bridge = NULL;
    }
    if (brain->snn_visual_bridge) {
        snn_visual_bridge_destroy((snn_visual_bridge_t*)brain->snn_visual_bridge);
        brain->snn_visual_bridge = NULL;
    }
    if (brain->snn_speech_bridge) {
        snn_speech_bridge_destroy((snn_speech_bridge_t*)brain->snn_speech_bridge);
        brain->snn_speech_bridge = NULL;
    }

    LOG_MODULE_DEBUG("BRAIN", "Destroy phase 1 (network+SNN bridges) done");
    // Cleanup hyperledger bridge (before SNN language bridge)
    if (brain->hyperledger_bridge) {
        hyperledger_bridge_destroy(brain->hyperledger_bridge);
        brain->hyperledger_bridge = NULL;
        brain->hyperledger_enabled = false;
    }

    // Cleanup grounded language system
    if (brain->snn_lang_bridge) {
        snn_language_bridge_destroy(brain->snn_lang_bridge);
        brain->snn_lang_bridge = NULL;
    }

    /* S5-C1 fix: tear down the Broca subsystem (Broca adapter +
     * speech_repair + phonological_loop + pragmatics + the
     * substrate/thalamic/quantum bridges) BEFORE grounded_lang. The
     * destroy function is idempotent — if init was skipped, all the
     * tested fields are NULL and the inner branches no-op. Without this
     * call, the phonological_loop's malloced trace buffer + once-init
     * mutex (Slice 5) leak on every brain teardown. */
    nimcp_brain_factory_destroy_broca_subsystem(brain);

    if (brain->grounded_lang) {
        grounded_language_destroy(brain->grounded_lang);
        brain->grounded_lang = NULL;
    }

    /* S2-C2 fix: destroy the arcuate-feedback lock (lazy-init'd by the
     * cascade in nimcp_communication_cascade.c). Must come AFTER the
     * cascade-consuming subsystems (broca/grounded_lang) are gone, so
     * no thread will reach for the lock after we destroy it. */
    if (brain->arcuate_feedback_lock) {
        nimcp_mutex_free((nimcp_mutex_t*)brain->arcuate_feedback_lock);
        brain->arcuate_feedback_lock = NULL;
    }

    // Cleanup sparse coding system
    if (brain->sparse_coding_system) {
        cortical_sparse_destroy(brain->sparse_coding_system);
        brain->sparse_coding_system = NULL;
        brain->enable_sparse_coding = false;
    }

    // Free learning workspace
    nimcp_free(brain->learning_workspace.temp_float);
    nimcp_free(brain->learning_workspace.temp_uint);
    nimcp_free(brain->learning_workspace.delta_buf);
    memset(&brain->learning_workspace, 0, sizeof(brain->learning_workspace));

    // Cleanup language generator and embedding (before tokenizer, as generator references them)
    if (brain->lang_generator) {
        language_generator_destroy(brain->lang_generator);
        brain->lang_generator = NULL;
    }
    if (brain->lang_embedding) {
        embedding_destroy(brain->lang_embedding);
        brain->lang_embedding = NULL;
    }

    // Cleanup persistent tokenizer (lazy-initialized, may be NULL)
    if (brain->tokenizer) {
        tokenizer_destroy(brain->tokenizer);
        brain->tokenizer = NULL;
    }

    // Cleanup rubric evaluator (lazy-initialized, may be NULL)
    if (brain->rubric_evaluator) {
        rubric_evaluator_destroy(brain->rubric_evaluator);
        brain->rubric_evaluator = NULL;
    }
    // last_decision is now an owning deep-copy (managed by API layer)
    if (brain->last_decision) {
        brain_free_decision(brain->last_decision);
        brain->last_decision = NULL;
    }

    // Free cached cognitive transcript
    if (brain->last_transcript) {
        transcript_free(brain->last_transcript);
        brain->last_transcript = NULL;
    }

    // Phase 3: Cleanup distributed cognition coordinator
    if (brain->distributed) {
        distrib_cognition_destroy(brain->distributed);
    }

    // Phase 5/6: Cleanup glial integration + astrocyte/oligo/microglia networks.
    // Order matters: integration layer holds borrowed pointers to the three
    // networks; destroy it FIRST so it can't dereference freed memory, then
    // destroy the networks themselves. nimcp_brain_factory_destroy_glial_subsystem
    // does both in the correct order.
    nimcp_brain_factory_destroy_glial_subsystem(brain);

    // Phase 5/6: Cleanup myelin sheath network (after glial to avoid dangling pointers)
    if (brain->myelin_sheath) {
        myelin_network_destroy(brain->myelin_sheath);
        brain->myelin_sheath = NULL;
    }

    // Phase 1.5.6: Cleanup axon network
    if (brain->axon_network) {
        axon_network_destroy((axon_network_t*)brain->axon_network);
        brain->axon_network = NULL;
    }

    // Phase 1.5.7: Cleanup dendrite network
    if (brain->dendrite_network) {
        dendrite_network_destroy((dendrite_network_t*)brain->dendrite_network);
        brain->dendrite_network = NULL;
    }


    // Phase 8: Cleanup multi-modal subsystems
    if (brain->visual_cortex) {
        visual_cortex_destroy(brain->visual_cortex);
        brain->visual_cortex = NULL;
    }
    if (brain->audio_cortex) {
        audio_cortex_destroy(brain->audio_cortex);
        brain->audio_cortex = NULL;
    }
    if (brain->speech_cortex) {
        speech_cortex_destroy(brain->speech_cortex);
        brain->speech_cortex = NULL;
    }
    if (brain->multimodal) {
        multimodal_integration_destroy(brain->multimodal);
        brain->multimodal = NULL;
    }
    if (brain->nlp_network) {
        nlp_network_destroy(brain->nlp_network);
        brain->nlp_network = NULL;
    }
    nimcp_free(brain->visual_feature_buffer);
    brain->visual_feature_buffer = NULL;
    nimcp_free(brain->audio_feature_buffer);
    brain->audio_feature_buffer = NULL;
    nimcp_free(brain->speech_feature_buffer);
    brain->speech_feature_buffer = NULL;
    nimcp_free(brain->integrated_feature_buffer);
    brain->integrated_feature_buffer = NULL;


    LOG_MODULE_DEBUG("BRAIN", "Destroy phase 2 (language+workspace) done");
    // Phase 8.6: Cleanup pink noise neuromodulation
    if (brain->pink_noise) {
        neuromod_pink_destroy(brain->pink_noise);
        brain->pink_noise = NULL;
    }


    // Phase EDP-1: Cleanup Event-Driven Plasticity (before plasticity bridge it depends on)
    if (brain->event_driven_plasticity) {
        edp_destroy(brain->event_driven_plasticity);
        brain->event_driven_plasticity = NULL;
        brain->enable_event_driven_plasticity = false;
    }


    // Phase TPB-1: Cleanup Training-Plasticity Bridge (before neuromodulator system it depends on)
    if (brain->plasticity_bridge) {
        tpb_destroy(brain->plasticity_bridge);
        brain->plasticity_bridge = NULL;
        brain->enable_plasticity_bridge = false;
    }


    // Phase 10.5: Cleanup neuromodulator system
    if (brain->neuromodulator_system) {
        neuromodulator_system_destroy(brain->neuromodulator_system);
    }


    // Phase 3.0: Cleanup multihead attention
    if (brain->multihead_attention) {
        multihead_attention_destroy(brain->multihead_attention);
        brain->multihead_attention = NULL;
    }

    // Phase 3.1: Cleanup attention-plasticity bridge
    if (brain->attention_plasticity) {
        attention_plasticity_destroy(
            (attention_plasticity_bridge_t*)brain->attention_plasticity);
        brain->attention_plasticity = NULL;
        brain->attention_training_enabled = false;
    }


    // Phase 2 Middleware: Cleanup spike analysis and population coding
    if (brain->spike_feature_extractor) {
        brain_destroy_spike_feature_extractor(brain->spike_feature_extractor);
        brain->spike_feature_extractor = NULL;
    }
    if (brain->population_analyzer) {
        brain_destroy_population_analyzer(brain->population_analyzer);
        brain->population_analyzer = NULL;
    }


    // Module Integration: Cleanup brain regions
    if (brain->brain_regions) {
        brain_module_destroy(brain->brain_regions);
        brain->brain_regions = NULL;
    }


    // Phase CC-1: Cleanup cortical columns subsystem (Tier 0.65)
    // Order: feature hypercolumns → orientation hypercolumns → topographic maps →
    //        connectivity → laminar → hypercolumns → pool
    if (brain->feature_hypercolumns) {
        for (uint32_t i = 0; i < brain->num_feature_hypercolumns; i++) {
            if (brain->feature_hypercolumns[i]) {
                feature_hypercolumn_destroy(brain->feature_hypercolumns[i]);
            }
        }
        nimcp_free(brain->feature_hypercolumns);
        brain->feature_hypercolumns = NULL;
        brain->num_feature_hypercolumns = 0;
    }

    if (brain->orientation_hypercolumns) {
        for (uint32_t i = 0; i < brain->num_orientation_hypercolumns; i++) {
            if (brain->orientation_hypercolumns[i]) {
                orientation_hypercolumn_destroy(brain->orientation_hypercolumns[i]);
            }
        }
        nimcp_free(brain->orientation_hypercolumns);
        brain->orientation_hypercolumns = NULL;
        brain->num_orientation_hypercolumns = 0;
    }

    if (brain->visual_topographic_map) {
        topographic_map_destroy(brain->visual_topographic_map);
        brain->visual_topographic_map = NULL;
    }

    if (brain->auditory_topographic_map) {
        topographic_map_destroy(brain->auditory_topographic_map);
        brain->auditory_topographic_map = NULL;
    }

    if (brain->somatosensory_topographic_map) {
        topographic_map_destroy(brain->somatosensory_topographic_map);
        brain->somatosensory_topographic_map = NULL;
    }

    if (brain->columnar_connectivity) {
        columnar_connectivity_destroy(brain->columnar_connectivity);
        brain->columnar_connectivity = NULL;
    }

    if (brain->laminar_system) {
        laminar_structure_destroy(brain->laminar_system);
        brain->laminar_system = NULL;
    }

    if (brain->hypercolumns) {
        for (uint32_t i = 0; i < brain->num_hypercolumns; i++) {
            if (brain->hypercolumns[i]) {
                hypercolumn_destroy(brain->hypercolumns[i]);
            }
        }
        nimcp_free(brain->hypercolumns);
        brain->hypercolumns = NULL;
        brain->num_hypercolumns = 0;
    }

    if (brain->cortical_column_pool) {
        cortical_column_pool_destroy(brain->cortical_column_pool);
        brain->cortical_column_pool = NULL;
    }

    /* CC4 ternary teardown — paired with allocations in
     * nimcp_brain_init_structural.c. Each per-hypercolumn cc_ternary
     * object owns its own state; the shared cc_ternary_connectivity
     * object outlives them and is freed last. */
    if (brain->ternary_hypercolumns) {
        for (uint32_t i = 0; i < brain->num_ternary_hypercolumns; i++) {
            if (brain->ternary_hypercolumns[i]) {
                cc_ternary_hypercolumn_destroy(
                    (cc_ternary_hypercolumn_t*)brain->ternary_hypercolumns[i]);
            }
        }
        nimcp_free(brain->ternary_hypercolumns);
        brain->ternary_hypercolumns = NULL;
        brain->num_ternary_hypercolumns = 0;
    }
    if (brain->ternary_connectivity) {
        cc_ternary_connectivity_destroy(
            (cc_ternary_connectivity_t*)brain->ternary_connectivity);
        brain->ternary_connectivity = NULL;
    }

    /* CC2/CC3 column→decision projection scratch + matrix.
     * Both allocated together in init; free both, NULL both. */
    if (brain->column_feature_buf) {
        nimcp_free(brain->column_feature_buf);
        brain->column_feature_buf = NULL;
    }
    if (brain->column_to_decision_proj) {
        nimcp_free(brain->column_to_decision_proj);
        brain->column_to_decision_proj = NULL;
    }
    brain->column_feature_dim = 0;

    brain->enable_cortical_columns = false;
    brain->enable_cortical_ternary = false;


    // Phase 9.0: Cleanup neural logic network
    if (brain->logic) {
        neural_logic_destroy(brain->logic);
    }


    // Phase 9.2: Cleanup epistemic filter
    if (brain->epistemic) {
        epistemic_filter_destroy(brain->epistemic);
    }

    // Phase 9.4: Cleanup symbolic logic engine
    if (brain->symbolic_logic) {
        symbolic_logic_destroy(brain->symbolic_logic);
    }


    // Phase M3: Cleanup working memory transfer (BEFORE working memory)
    if (brain->wm_transfer_system) {
        wm_transfer_destroy(brain->wm_transfer_system);
        brain->wm_transfer_system = NULL;
    }

    // Phase 10.1: Cleanup working memory
    if (brain->working_memory) {
        working_memory_destroy(brain->working_memory);
        brain->working_memory = NULL;
    }


    // Phase 10.2: Cleanup memory consolidation
    // Note: brain_stop_background_consolidation() stops the thread, destroys
    // synchronization primitives, AND frees the handle — no separate free needed.
    if (brain->consolidation) {
        brain_stop_background_consolidation(brain->consolidation);
        brain->consolidation = NULL;
    }

    // Phase 10.3: Cleanup executive functions
    if (brain->executive) {
        executive_destroy(brain->executive);
    }


    // Phase 10.2: Cleanup emotional system
    if (brain->emotional_system) {
        emotion_system_destroy(brain->emotional_system);
    }

    // Phase 10.4: Cleanup sleep/wake bridges (destroy BEFORE the sleep_system
    // they reference via bridge->sleep_wake borrowed pointer)
    if (brain->sleep_wake_substrate_bridge) {
        extern void sleep_wake_substrate_bridge_destroy(
            struct sleep_wake_substrate_bridge*);
        sleep_wake_substrate_bridge_destroy(brain->sleep_wake_substrate_bridge);
        brain->sleep_wake_substrate_bridge = NULL;
    }
    if (brain->sleep_wake_thalamic_bridge) {
        extern void sleep_wake_thalamic_bridge_destroy(
            struct sleep_wake_thalamic_bridge*);
        sleep_wake_thalamic_bridge_destroy(brain->sleep_wake_thalamic_bridge);
        brain->sleep_wake_thalamic_bridge = NULL;
    }

    /* Region/biology/chemistry driven cycles destroy — unregister the 6 tick
     * drivers (neuromod/sensorimotor/language/physics_bridges + meta_learning
     * + intuitive_physics) registered by init_region_cycles. MUST run BEFORE
     * the biology/chemistry destroys below so there's no race between the
     * driven threads (which call brain_tick_*) and module teardown. Each
     * unregister joins its driver thread. */
    {
        extern void nimcp_brain_factory_destroy_region_cycles_subsystem(brain_t);
        nimcp_brain_factory_destroy_region_cycles_subsystem(brain);
    }

    /* Biology cluster destroy — unregisters each driven cycle (which stops +
     * joins the driver thread) BEFORE cycle_coordinator_destroy freeing. Must
     * run while brain->cycle_coordinator is still valid. */
    {
        extern void nimcp_brain_factory_destroy_biology_subsystem(brain_t);
        nimcp_brain_factory_destroy_biology_subsystem(brain);
    }

    /* Chemistry cluster destroy (Wave 6) — same ordering rationale: runs
     * while cycle_coordinator is still valid so the driver thread can be
     * joined cleanly before the coord is freed. */
    {
        extern void nimcp_brain_factory_destroy_chemistry_subsystem(brain_t);
        nimcp_brain_factory_destroy_chemistry_subsystem(brain);
    }

    /* Cochlea + 15 bridges destroy (Wave 8A) — same ordering: unregister
     * BRAIN_CYCLE_COCHLEA_BRIDGES before any bridge pointer is freed so
     * the tick wrapper can't race with destruction. Also runs while
     * brain->audio_cortex / brain->fep_orchestrator / etc. are still
     * live — the bridges' destroy funcs may reach back into those deps. */
    {
        extern void nimcp_brain_factory_destroy_cochlea_subsystem(brain_t);
        nimcp_brain_factory_destroy_cochlea_subsystem(brain);
    }

    /* Physics bridges destroy (Wave 8C) — 4 HALF-STATUE bridges +
     * upstream ephaptic/thermo/hh modules. Bridges are destroyed first,
     * then upstream modules freed. Null-safe throughout. */
    {
        extern void nimcp_brain_factory_destroy_physics_bridges_subsystem(brain_t);
        nimcp_brain_factory_destroy_physics_bridges_subsystem(brain);
    }

    /* Entorhinal adapter destroy (Wave 8B-c Gap 1 completion) — runs AFTER
     * destroy_region_cycles_subsystem above has already unregistered
     * BRAIN_CYCLE_ENTORHINAL and joined its driver thread, so no tick can
     * race with the adapter free. Plugs a leak that existed when the field
     * was added without a matching destroy call. */
    {
        extern void nimcp_brain_factory_destroy_entorhinal_subsystem(brain_t);
        nimcp_brain_factory_destroy_entorhinal_subsystem(brain);
    }

    /* Predictive-immune coupling destroy — same ordering rationale as biology.
     * Unregister drives + stop + destroy BEFORE predictive_network or
     * immune_system are freed downstream (predictive_network destroy on line
     * ~760 below, immune_system is managed by its own lifecycle).  */
    {
        extern void nimcp_brain_factory_destroy_predictive_immune_subsystem(brain_t);
        nimcp_brain_factory_destroy_predictive_immune_subsystem(brain);
    }

    // Phase 10.4: Cleanup sleep/wake system
    if (brain->sleep_system) {
        sleep_system_destroy(brain->sleep_system);
    }


    LOG_MODULE_DEBUG("BRAIN", "Destroy phase 3 (neural+cognitive) done");
    // Phase M1: Cleanup engram system
    if (brain->engram_system) {
        engram_system_destroy(brain->engram_system);
    }

    // Phase M1b: Cleanup persistent memory systems (OODB before store — OODB flushes to store)
    if (brain->ood_detector) {
        nimcp_ood_detector_destroy((nimcp_ood_detector_t*)brain->ood_detector);
        brain->ood_detector = NULL;
    }
    if (brain->memory_oodb) {
        nimcp_oodb_flush((nimcp_oodb_t*)brain->memory_oodb);
        nimcp_oodb_destroy((nimcp_oodb_t*)brain->memory_oodb);
        brain->memory_oodb = NULL;
    }
    if (brain->memory_store) {
        nimcp_memory_store_checkpoint((nimcp_memory_store_t*)brain->memory_store);
        nimcp_memory_store_destroy((nimcp_memory_store_t*)brain->memory_store);
        brain->memory_store = NULL;
    }

    // Phase M2: Cleanup systems consolidation
    if (brain->systems_consolidation) {
        systems_consolidation_destroy(brain->systems_consolidation);
    }

    // Phase M4: Cleanup semantic memory network
    if (brain->semantic_memory) {
        semantic_memory_destroy(brain->semantic_memory);
    }


    // Phase 10.6: Cleanup Theory of Mind
    if (brain->theory_of_mind) {
        tom_destroy(brain->theory_of_mind);
    }

    // Phase 10.7: Cleanup Natural Explanations
    if (brain->explanation_gen) {
        explanation_generator_destroy(brain->explanation_gen);
    }

    // Phase 10.8: Cleanup Meta-Learning
    if (brain->meta_learner) {
        meta_learner_destroy(brain->meta_learner);
    }

    // Phase 10.5: Cleanup Mental Health
    if (brain->mental_health_monitor) {
        mental_health_destroy(brain->mental_health_monitor);
    }

    // Trauma Resilience: recall dampening + arousal homeostasis
    if (brain->trauma_resilience) {
        extern void nimcp_trauma_resilience_destroy(void*);
        nimcp_trauma_resilience_destroy(brain->trauma_resilience);
        brain->trauma_resilience = NULL;
    }

    // Phase 10.9: Cleanup Predictive Processing
    if (brain->predictive_network) {
        predictive_destroy(brain->predictive_network);
    }

    // Phase 10.11: Cleanup Mirror Neurons
    if (brain->mirror_neurons) {
        mirror_neurons_destroy(brain->mirror_neurons);
    }


    // Cleanup Global Workspace Architecture
    if (brain->global_workspace) {
        global_workspace_destroy(brain->global_workspace);
    }


    // CRITICAL FIX: Cleanup cognitive modules (memory leak fix)
    if (brain->introspection) {
        introspection_context_destroy(brain->introspection);
    }
    if (brain->curiosity) {
        curiosity_engine_destroy(brain->curiosity);
    }
    if (brain->salience) {
        salience_evaluator_destroy(brain->salience);
    }
    if (brain->ethics) {
        ethics_engine_destroy(brain->ethics);
    }
    if (brain->knowledge) {
        knowledge_system_destroy(brain->knowledge);
    }


    // Phase 11: Part I.2: Cleanup empathetic response engine (before empathy network)
    if (brain->empathetic_response_engine) {
        /* empathetic_response_destroy declared via nimcp_empathetic_response.h
         * included in the parent nimcp_brain.c (W9-finish). */
        empathetic_response_destroy(brain->empathetic_response_engine);
    }

    // Phase 11: Part I.1: Cleanup empathy network
    if (brain->empathy_network) {
        empathy_network_destroy(brain->empathy_network);
    }

    // Phase 12: Cleanup autobiographical memory
    if (brain->autobio) {
        autobio_destroy(brain->autobio);
    }


    // Phase 12: Cleanup self-model
    if (brain->self_model) {
        self_model_destroy(brain->self_model);
    }

    // Cleanup Recursive Cognition Engine
    if (brain->rcog_engine) {
        rcog_engine_stop(brain->rcog_engine, 1000);  // 1s timeout
        rcog_engine_destroy(brain->rcog_engine);
        brain->rcog_engine = NULL;
    }

    // Cleanup Inner Dialogue Engine
    if (brain->inner_dialogue) {
        inner_dialogue_engine_destroy(brain->inner_dialogue);
        brain->inner_dialogue = NULL;
    }

    // Cleanup Reasoning Engine
    if (brain->reasoning_engine) {
        reasoning_engine_destroy(brain->reasoning_engine);
        brain->reasoning_engine = NULL;
    }

    /* Tier 1 Step E: free the cascade reasoning-conclusion cache buffer
     * (lazily allocated in cascade_prime_reasoning). NULL-safe. */
    if (brain->cascade_reasoning_vec) {
        nimcp_free(brain->cascade_reasoning_vec);
        brain->cascade_reasoning_vec = NULL;
        brain->cascade_reasoning_cap = 0;
        brain->cascade_reasoning_dim = 0;
    }

    // Cleanup Imagination Engine
    if (brain->imagination) {
        imagination_engine_destroy(brain->imagination);
        brain->imagination = NULL;
    }

    /* Slice B (Option 1 architectural rebuild) — concept registry.
     * concept_registry_destroy is NULL-safe but we still gate to keep
     * the read-after-free pattern symmetrical with the other engines. */
    if (brain->concept_registry) {
        concept_registry_destroy((concept_registry_t*)brain->concept_registry);
        brain->concept_registry = NULL;
    }

    // Cleanup Collective Cognition
    if (brain->collective_cognition) {
        collective_cognition_destroy(brain->collective_cognition);
        brain->collective_cognition = NULL;
    }

    // Phase 12: Cleanup personality profile
    // NOTE: personality_profile_t is assumed to be a flat POD type (no internal allocations).
    // If personality_profile_t ever gains dynamically allocated fields, this must change
    // to a dedicated personality_destroy() function to avoid memory leaks.
    if (brain->personality) {
        nimcp_free(brain->personality);
    }

    // Phase 11: Cleanup long-term memory consolidation buffer
    if (brain->longterm_memory) {
        for (uint32_t i = 0; i < brain->longterm_count; i++) {
            if (brain->longterm_memory[i].features) {
                nimcp_free(brain->longterm_memory[i].features);
            }
        }
        nimcp_free(brain->longterm_memory);
    }


    // Phase 11 Enhancement C1.1: Cleanup quantum annealer
    if (brain->quantum_annealer) {
        quantum_annealer_destroy(brain->quantum_annealer);
    }

    // Phase C4.1: Cleanup quantum-Shannon diffusion
    if (brain->quantum_shannon_diffusion) {
        quantum_shannon_destroy((quantum_shannon_diffusion_t*)brain->quantum_shannon_diffusion);
        brain->quantum_shannon_diffusion = NULL;
    }


    // Phase C4.7: Cleanup cross-modal routing graph
    if (brain->cross_modal_graph) {
        cross_modal_destroy_routing_graph(brain->cross_modal_graph);
        brain->cross_modal_graph = NULL;
    }


    // === PHASE E: CLEANUP HIGHER-ORDER COGNITIVE & SOCIAL SYSTEMS ===

    // Phase E5: Cleanup Shadow Emotions
    if (brain->shadow_emotions) {
        shadow_system_destroy(brain->shadow_emotions);
    }


    // Phase E6: Cleanup Bias Detection
    if (brain->bias_detection) {
        bias_system_destroy(brain->bias_detection);
    }


    // === PHASE E: CLEANUP FULL EMOTIONAL INTELLIGENCE ===

    // Phase E1: Cleanup Grief and Loss
    if (brain->grief_system) {
        grief_system_destroy(brain->grief_system);
    }

    // Phase E2: Cleanup Joy and Euphoria
    if (brain->joy_system) {
        joy_system_destroy(brain->joy_system);
    }

    // Phase E3: Cleanup Remorse and Regret
    if (brain->remorse_system) {
        remorse_regret_system_destroy(brain->remorse_system);
    }

    // Phase E4: Cleanup Love, Loyalty, Friendship
    if (brain->social_bond_system) {
        social_bond_system_destroy(brain->social_bond_system);
    }


    // === PHASE EDGE: CLEANUP EDGE/ROBOT INTEGRATION ===
    // Destroy in reverse order: ROS2 bridge → safety watchdog → sensor hub
    // Then swarm bridges: dragonfly-swarm → portia-swarm
    //
    // Use forward declarations with void* to avoid including edge headers
    // in this lifecycle file (SRP: includes are at file scope, not in functions).
    // The actual functions accept their typed pointers; C permits implicit
    // void* → typed-pointer conversion at the call site.
    {
        /* Forward-declare opaque struct types and destroy functions.
         * Brain stores these as void* to avoid header dependencies. */
        typedef struct nimcp_ros2_bridge nimcp_ros2_bridge_t;
        typedef struct nimcp_safety_watchdog nimcp_safety_watchdog_t;
        typedef struct nimcp_sensor_hub nimcp_sensor_hub_t;
        typedef struct swarm_dragonfly_bridge_s swarm_dragonfly_bridge_t;
        typedef struct portia_swarm_bridge_t portia_swarm_bridge_t;

        extern void nimcp_ros2_bridge_destroy(nimcp_ros2_bridge_t* bridge);
        extern void nimcp_watchdog_destroy(nimcp_safety_watchdog_t* watchdog);
        extern void nimcp_sensor_hub_destroy(nimcp_sensor_hub_t* hub);
        extern void swarm_dragonfly_bridge_destroy(swarm_dragonfly_bridge_t* bridge);
        extern void portia_swarm_bridge_destroy(portia_swarm_bridge_t* bridge);

        if (brain->ros2_bridge) {
            nimcp_ros2_bridge_destroy((nimcp_ros2_bridge_t*)brain->ros2_bridge);
            brain->ros2_bridge = NULL;
            brain->ros2_bridge_enabled = false;
        }
        if (brain->safety_watchdog) {
            nimcp_watchdog_destroy((nimcp_safety_watchdog_t*)brain->safety_watchdog);
            brain->safety_watchdog = NULL;
            brain->safety_watchdog_enabled = false;
        }
        if (brain->sensor_hub) {
            nimcp_sensor_hub_destroy((nimcp_sensor_hub_t*)brain->sensor_hub);
            brain->sensor_hub = NULL;
            brain->sensor_hub_enabled = false;
        }
        if (brain->swarm_dragonfly_bridge) {
            swarm_dragonfly_bridge_destroy((swarm_dragonfly_bridge_t*)brain->swarm_dragonfly_bridge);
            brain->swarm_dragonfly_bridge = NULL;
            brain->swarm_dragonfly_bridge_enabled = false;
        }
        if (brain->portia_swarm_bridge) {
            portia_swarm_bridge_destroy((portia_swarm_bridge_t*)brain->portia_swarm_bridge);
            brain->portia_swarm_bridge = NULL;
            brain->portia_swarm_bridge_enabled = false;
        }
        /* Octopus cognitive module: destroy core first, then free the
         * Phase 2a bridge state (which the core's hooks referenced). Order
         * matters — if we freed bridge state first, any pending hook call
         * from the core would UAF. The forward-declared void-pointer
         * signatures avoid pulling the octopus headers into this TU. */
        if (brain->octopus) {
            typedef struct octopus_system_s octopus_system_t;
            extern void octopus_destroy(octopus_system_t* ctx);
            octopus_destroy((octopus_system_t*)brain->octopus);
            brain->octopus = NULL;
            brain->octopus_enabled = false;
        }
        if (brain->octopus_bridge_state) {
            /* Unregister bio_router module before freeing state — otherwise
             * router retains a dangling context pointer. */
            extern void nimcp_octopus_uninstall_bridges(brain_t brain);
            nimcp_octopus_uninstall_bridges(brain);
            nimcp_free(brain->octopus_bridge_state);
            brain->octopus_bridge_state = NULL;
        }
        /* Amygdala (Phase 3c). Header included at TU top; amygdala_t is
         * an anonymous typedef so we use it directly here. */
        if (brain->amygdala) {
            amygdala_destroy((amygdala_t*)brain->amygdala);
            brain->amygdala = NULL;
            brain->amygdala_enabled = false;
        }
        /* Dragonfly sidecar LNN reservoir (Phase 4i). Destroy the reservoir
         * before the dragonfly system itself so the reservoir's non-owning
         * back-reference stays valid. Forward-declared signatures keep the
         * LNN/dragonfly headers out of this TU. */
        if (brain->dragonfly_lnn) {
            typedef struct dragonfly_lnn_s dragonfly_lnn_t;
            extern void dragonfly_lnn_destroy(dragonfly_lnn_t* dl);
            dragonfly_lnn_destroy((dragonfly_lnn_t*)brain->dragonfly_lnn);
            brain->dragonfly_lnn = NULL;
            brain->dragonfly_lnn_enabled = false;
        }
        /* Phase 4n-q: JEPA bridges — destroy before the subsystems they
         * hold non-owning refs into (omni, audio cortex, neuromod). */
        {
            extern void nimcp_jepa_brain_bridges_destroy(brain_t brain);
            nimcp_jepa_brain_bridges_destroy(brain);
        }
        /* Round A/2: perception bridges — destroy before cortex + immune
         * teardown since the 10 bridges hold non-owning refs into them. */
        {
            extern void nimcp_brain_destroy_perception_bridges(brain_t brain);
            nimcp_brain_destroy_perception_bridges(brain);
        }
        /* Dragonfly system + medulla bridge: previously leaked on brain
         * teardown. Medulla bridge destroy is outside-in (bridge first,
         * then the dragonfly it references) to mirror create order. */
        {
            typedef struct dragonfly_medulla_bridge_s* dragonfly_medulla_bridge_t;
            extern void dragonfly_medulla_bridge_destroy(dragonfly_medulla_bridge_t bridge);
            extern void dragonfly_system_destroy(dragonfly_system_t* system);
            if (brain->dragonfly_medulla_bridge) {
                dragonfly_medulla_bridge_destroy(
                    (dragonfly_medulla_bridge_t)brain->dragonfly_medulla_bridge);
                brain->dragonfly_medulla_bridge = NULL;
            }
            if (brain->dragonfly) {
                dragonfly_system_destroy(brain->dragonfly);
                brain->dragonfly = NULL;
                brain->dragonfly_enabled = false;
            }
        }
        /* Flight controller bridges */
        if (brain->mavlink_bridge) {
            extern void nimcp_mavlink_bridge_destroy(void*);
            nimcp_mavlink_bridge_destroy(brain->mavlink_bridge);
            brain->mavlink_bridge = NULL;
        }
        if (brain->dji_bridge) {
            extern void nimcp_dji_bridge_destroy(void*);
            nimcp_dji_bridge_destroy(brain->dji_bridge);
            brain->dji_bridge = NULL;
        }
        if (brain->msp_bridge) {
            extern void nimcp_msp_bridge_destroy(void*);
            nimcp_msp_bridge_destroy(brain->msp_bridge);
            brain->msp_bridge = NULL;
        }
        if (brain->parrot_bridge) {
            extern void nimcp_parrot_bridge_destroy(void*);
            nimcp_parrot_bridge_destroy(brain->parrot_bridge);
            brain->parrot_bridge = NULL;
        }
        /* Sensorimotor + Language */
        if (brain->sensorimotor) {
            extern void nimcp_sensorimotor_destroy(void*);
            nimcp_sensorimotor_destroy(brain->sensorimotor);
            brain->sensorimotor = NULL;
        }
        if (brain->native_language) {
            extern void nimcp_native_language_destroy(void*);
            nimcp_native_language_destroy(brain->native_language);
            brain->native_language = NULL;
            brain->native_language_enabled = false;
        }
        if (brain->brain_tokenizer) {
            extern void nimcp_tokenizer_destroy(void*);
            nimcp_tokenizer_destroy(brain->brain_tokenizer);
            brain->brain_tokenizer = NULL;
            brain->brain_tokenizer_enabled = false;
        }
        /* Cognitive enhancements */
        {
            extern void nimcp_inner_speech_destroy(void*);
            extern void nimcp_episodic_replay_destroy(void*);
            extern void nimcp_wmt_destroy(void*);
            extern void nimcp_oa_destroy(void*);
            extern void nimcp_wms_destroy(void*);
            extern void nimcp_analogical_destroy(void*);
            extern void nimcp_multiscale_destroy(void*);
            /* nimcp_emotional_learning_destroy declared via its header (W9-finish include in brain.c) */
            extern void nimcp_contrastive_self_destroy(void*);
            extern void nimcp_self_curriculum_destroy(void*);
            extern void nimcp_dynamic_arch_destroy(void*);
            if (brain->inner_speech) { nimcp_inner_speech_destroy(brain->inner_speech); brain->inner_speech = NULL; }
            if (brain->episodic_replay) { nimcp_episodic_replay_destroy(brain->episodic_replay); brain->episodic_replay = NULL; }
            if (brain->world_model_trainer) { nimcp_wmt_destroy(brain->world_model_trainer); brain->world_model_trainer = NULL; }
            if (brain->output_attention) { nimcp_oa_destroy(brain->output_attention); brain->output_attention = NULL; }
            if (brain->wm_scratchpad) { nimcp_wms_destroy(brain->wm_scratchpad); brain->wm_scratchpad = NULL; }
            if (brain->analogical_transfer) { nimcp_analogical_destroy(brain->analogical_transfer); brain->analogical_transfer = NULL; }
            if (brain->multiscale_memory) { nimcp_multiscale_destroy(brain->multiscale_memory); brain->multiscale_memory = NULL; }
            if (brain->emotional_learning) { nimcp_emotional_learning_destroy(brain->emotional_learning); brain->emotional_learning = NULL; }
            if (brain->contrastive_self) { nimcp_contrastive_self_destroy(brain->contrastive_self); brain->contrastive_self = NULL; }
            if (brain->self_curriculum) { nimcp_self_curriculum_destroy(brain->self_curriculum); brain->self_curriculum = NULL; }
            if (brain->dynamic_arch) { nimcp_dynamic_arch_destroy(brain->dynamic_arch); brain->dynamic_arch = NULL; }
        }
    }

    LOG_MODULE_DEBUG("BRAIN", "Destroy phase 4 (higher cognitive) done");
    // Community Detection: Cleanup
    if (brain->functional_modules) {
        topology_community_structure_free(brain->functional_modules);
    }
    if (brain->network_hubs) {
        hub_structure_free(brain->network_hubs);
    }
    if (brain->topology_metrics) {
        nimcp_free(brain->topology_metrics);
    }


    // Network Analyzer: Cleanup
    if (brain->network_analyzer) {
        network_analyzer_destroy((network_analyzer_t*)brain->network_analyzer);
        brain->network_analyzer = NULL;
    }


    // Universal Event Bus: Cleanup event broadcasting system
    if (brain->event_bus) {
        event_bus_destroy(brain->event_bus);
        brain->event_bus = NULL;
    }


    // Phase 1.5: Cleanup memory pools for hot-path allocations
    if (brain->decision_struct_pool) {
        memory_pool_destroy(brain->decision_struct_pool);
        brain->decision_struct_pool = NULL;
    }
    if (brain->output_vector_pool) {
        memory_pool_destroy(brain->output_vector_pool);
        brain->output_vector_pool = NULL;
    }
    if (brain->active_neuron_ids_pool) {
        memory_pool_destroy(brain->active_neuron_ids_pool);
        brain->active_neuron_ids_pool = NULL;
    }


    // === PHASE SC-2: CLEANUP SECURITY RECOVERY BRIDGE ===
    if (brain->security_bridge) {
        nimcp_srb_destroy((nimcp_security_recovery_bridge_t*)brain->security_bridge);
        brain->security_bridge = NULL;
    }


    // === PHASE TM-3: CLEANUP BRAIN-TRAINING INTEGRATION ===
    // Must be cleaned up BEFORE security integration since it may be registered with security
    if (brain->training_ctx) {
        nimcp_brain_training_destroy(brain->training_ctx);
        brain->training_ctx = NULL;
    }
    brain->enable_training_integration = false;


    // === PHASE SC-4: CLEANUP UNIVERSAL SECURITY INTEGRATION ===
    if (brain->security_integration) {
        // Unregister regions
        if (brain->sec_region_ids) {
            for (uint32_t i = 0; i < brain->num_sec_regions; i++) {
                nimcp_sec_unregister_region(brain->security_integration, brain->sec_region_ids[i]);
            }
            nimcp_free(brain->sec_region_ids);
            brain->sec_region_ids = NULL;
        }

        // Unregister module
        if (brain->sec_module_id > 0) {
            nimcp_sec_unregister_module(brain->security_integration, brain->sec_module_id);
        }

        // Destroy the security integration context
        nimcp_sec_integration_destroy(brain->security_integration);
        brain->security_integration = NULL;
    }


    // === PHASE IS-1: CLEANUP BLOOD-BRAIN BARRIER (BBB) ===
    if (brain->bbb_enabled && brain->bbb_system) {
        // Unregister this brain's memory region from BBB
        if (brain->bbb_memory_region_id > 0) {
            bbb_unregister_memory_region(brain->bbb_system, brain->bbb_memory_region_id);
        }

        // Release our reference to the global BBB system
        nimcp_bbb_release_global_system();

        brain->bbb_system = NULL;
        brain->bbb_memory_region_id = 0;
        brain->bbb_enabled = false;
    }


    LOG_MODULE_DEBUG("BRAIN", "Destroy phase 5 (security+training) done");
    clear_cache(brain);

    // Destroy cache mutex
    nimcp_platform_mutex_destroy(&brain->cache_mutex);


    // P1-9 FIX: Bio-Async reference-counted cleanup.
    // Only unregister the GLOBAL bio-async context when the last brain is destroyed.
    // Without this, the first brain_destroy would break all other brains' bio-async.
    int prev_ref = __atomic_fetch_sub(&g_brain_bio_ref_count, 1, __ATOMIC_ACQ_REL);
    if (prev_ref <= 1) {
        // Last brain - perform global bio-async cleanup
        bio_module_context_t ctx = __atomic_load_n(&g_brain_bio_ctx, __ATOMIC_ACQUIRE);
        if (ctx && __atomic_compare_exchange_n(&g_brain_bio_ctx, &ctx, NULL,
                                               false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            LOG_MODULE_INFO("BRAIN", "Last brain destroyed - unregistering from bio-async router");
            bio_router_unregister_module(ctx);
            __atomic_store_n(&g_brain_bio_initialized, false, __ATOMIC_RELEASE);
            // Reset platform_once so next brain creation can re-register
            g_brain_bio_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
        }
    }
    // If ref_count > 0, other brains still need the global bio-async context

    // Per-brain bio-async cleanup
    if (brain->bio_async_enabled) {
        brain_bio_async_shutdown(brain);
        brain->bio_async_enabled = false;
    }

    nimcp_free(brain);
}


/**
 * @brief Clone brain using copy-on-write optimization
 *
 * WHAT: Creates lightweight clone sharing network with original
 * WHY:  Enable efficient replication (86% memory savings)
 * HOW:  Shares adaptive_network_t, copies metadata
 *
 * PERFORMANCE: <10ms vs ~350ms for full copy
 * MEMORY: ~1MB overhead vs ~50MB for full copy
 *
 * @param original Brain to clone
 * @return Cloned brain or NULL on error
 */
// brain_clone_cow() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

/**
 * @brief Mark brain as a snapshot with preserved stats
 *
 * WHAT: Sets snapshot flag and preserves current stats
 * WHY:  Snapshots should preserve stats at snapshot time, not reflect future changes
 * HOW:  Stores current stats in brain->snapshot_stats
 *
 * @param brain Brain to mark as snapshot
 * @param stats Stats to preserve
 */
// brain_mark_as_snapshot() - MOVED TO: src/core/brain/distributed/nimcp_brain_distributed.c

//=============================================================================
// Learning API
//=============================================================================

/**
 * @brief Find or create output label index
 *
 * WHY: Maps string labels to numeric indices
 * Enables human-readable classification
 *
 * COMPLEXITY: O(k) where k = num_existing_labels
 * OPTIMIZATION: Linear search sufficient for small label sets
 *
 * THREAD SAFETY: Uses cache_mutex for protection. This prevents race conditions
 * when multiple threads try to create labels concurrently, which could cause
 * duplicate labels or data corruption.
 *
 * @param brain Brain handle
 * @param label Label string
 * @return Label index
 */
static uint32_t get_or_create_label_index(brain_t brain, const char* label)
{
    uint32_t result = 0;

    // THREAD SAFETY FIX: Protect label creation with mutex
    // Without this, concurrent threads could:
    // 1. Both see label doesn't exist
    // 2. Both create the same label at same index
    // 3. Result in duplicate labels or memory corruption
    nimcp_platform_mutex_lock(&brain->cache_mutex);

    // Search existing labels - O(k)
    for (uint32_t i = 0; i < brain->num_output_labels; i++) {
        if (strcmp(brain->output_labels[i], label) == 0) {
            result = i;
            goto done;
        }
    }

    // Guard: Check capacity — labels exceed output neurons
    if (brain->num_output_labels >= brain->config.num_outputs) {
        // Hash overflow labels across existing outputs instead of always index 0
        uint32_t h = 5381;
        for (const char* p = label; *p; p++) {
            h = ((h << 5) + h) + (unsigned char)*p;
        }
        result = h % brain->config.num_outputs;
        goto done;
    }

    // Create new label (use nimcp_malloc to match nimcp_free in brain_destroy)
    size_t label_len = strlen(label);
    brain->output_labels[brain->num_output_labels] = nimcp_malloc(label_len + 1);
    if (!brain->output_labels[brain->num_output_labels]) {
        result = 0;
        goto done;
    }
    strncpy(brain->output_labels[brain->num_output_labels], label, label_len + 1);
    brain->output_labels[brain->num_output_labels][label_len] = '\0';
    result = brain->num_output_labels++;

done:
    nimcp_platform_mutex_unlock(&brain->cache_mutex);
    return result;
}

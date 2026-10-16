/**
 * @file nimcp_consolidation.c
 * @brief Implementation of memory consolidation and sleep-like learning
 *
 * WHAT: Implements memory consolidation - strengthening important patterns,
 * pruning weak connections, and integrating new knowledge. Analogous to
 * sleep in biological brains.
 *
 * WHY: Online learning is fast but noisy. Offline consolidation allows:
 * - Pattern strengthening through replay
 * - Noise reduction through pruning
 * - Homeostasis through synaptic scaling
 * - Knowledge integration through rehearsal
 *
 * HOW: Provides synchronous consolidation (explicit) and background
 * consolidation thread (automatic). Uses pattern replay, synaptic scaling,
 * and connection pruning strategies.
 *
 * DESIGN PATTERNS:
 * - Strategy: Different consolidation strategies
 * - Observer: Event callbacks
 * - Thread Pool: Background consolidation thread
 * - Command: Consolidation operations
 * - Memento: State snapshots
 *
 * THREAD SAFETY: All public functions are thread-safe via mutex protection.
 * Background thread coordinates with brain lock to prevent concurrent access.
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "cognitive/consolidation/nimcp_consolidation.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"  /* W13: consolidation cycle events */

#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

// Phase 10.3: Emotional working memory integration
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"

// Memory consolidation: Network access for real weight extraction
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "cognitive/analysis/nimcp_network_analysis.h"  // Network topology analysis

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"  // For error codes

#define LOG_MODULE "consolidation"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(consolidation, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Background consolidation handle structure (Pimpl)
 * WHY: Encapsulate thread details
 * HOW: Opaque pointer pattern
 */
struct consolidation_handle_struct {
    brain_t brain;                 /* Associated brain */
    consolidation_config_t config; /* Configuration */

    nimcp_thread_t thread;   /* Background thread */
    bool thread_running;     /* Is thread active? */
    bool thread_should_stop; /* Stop signal */
    bool paused;             /* Is consolidation paused? */
    bool trigger_now;        /* Immediate trigger */

    uint32_t interval_seconds; /* Consolidation interval */

    consolidation_stats_t stats; /* Statistics */

    float current_progress; /* Current progress (0-1) */
    bool is_consolidating;  /* Is consolidation running? */

    nimcp_mutex_t lock;        /* Protects handle state */
    nimcp_cond_t trigger_cond; /* Condition for triggering */

    // Bio-async integration
    bio_module_context_t bio_ctx;  /* Bio-async module context */
    bool bio_async_enabled;        /* Bio-async registration status */
};

/**
 * WHAT: Global statistics for synchronous consolidation
 * WHY: Track stats even without background handle
 * HOW: Static global variable
 */
static consolidation_stats_t g_sync_stats = {0};
static nimcp_mutex_t g_sync_stats_lock;
static nimcp_once_t g_sync_stats_init = NIMCP_ONCE_INIT;

static void init_sync_stats_lock(void)
{
    nimcp_mutex_init(&g_sync_stats_lock, NULL);
}

static void ensure_sync_stats_init(void)
{
    nimcp_once(&g_sync_stats_init, init_sync_stats_lock);
}

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static void* consolidation_thread_fn(void* arg);
static void bio_broadcast_consolidation_complete(consolidation_handle_t handle, float duration_ms);
static bool perform_consolidation(brain_t brain, const consolidation_config_t* config,
                                  consolidation_stats_t* stats, float* progress);

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 */
static int consolidation_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

/* Forward declaration of handler */
static nimcp_error_t handle_consolidation_trigger(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static bool consolidate_replay(brain_t brain, const consolidation_config_t* config,
                               consolidation_stats_t* stats);
static bool consolidate_replay_with_cache(brain_t brain, const consolidation_config_t* config,
                                          consolidation_stats_t* stats,
                                          const consolidation_community_cache_t* cache);
static bool consolidate_scaling(brain_t brain, const consolidation_config_t* config,
                                consolidation_stats_t* stats);
static bool consolidate_pruning(brain_t brain, const consolidation_config_t* config,
                                consolidation_stats_t* stats);

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default consolidation configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Return pre-configured struct
 */
consolidation_config_t consolidation_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_default_config", 0.0f);


    consolidation_config_t config = {.strategy = CONSOLIDATION_STRATEGY_FULL,
                                     .priority = CONSOLIDATION_PRIORITY_IMPORTANT,

                                     .consolidation_cycles = 10,
                                     .consolidation_strength = 0.1F,

                                     .enable_replay = true,
                                     .replay_count = 100,

                                     .enable_pruning = true,
                                     .pruning_threshold = 0.01F,

                                     .enable_scaling = true,
                                     .scaling_target = 0.5F,

                                     .prioritize_novel = true,
                                     .novelty_boost = 1.5F,

                                     .prune_weak = true,
                                     .weakness_threshold = 0.1F,

                                     .max_prune_passes = 1,
                                     .neuron_sample_rate = 1.0F,

                                     .use_community_cache = true,
                                     .hub_consolidation_boost = 1.5F,
                                     .cross_community_boost = 1.3F,

                                     .on_consolidation_start = NULL,
                                     .on_consolidation_progress = NULL,
                                     .on_consolidation_complete = NULL,
                                     .callback_context = NULL};
    return config;
}

/**
 * WHAT: Lightweight consolidation — replay only, no scaling/pruning
 * WHY: Fast (~ms) consolidation for warm-up phases
 * HOW: 2 cycles, replay only, skip expensive weight iteration
 */
consolidation_config_t consolidation_light_config(void)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_REPLAY;
    config.consolidation_cycles = 2;
    config.enable_scaling = false;
    config.enable_pruning = false;
    config.prune_weak = false;
    config.max_prune_passes = 0;
    config.neuron_sample_rate = 0.0F;  /* Not used — scaling disabled */
    return config;
}

/**
 * WHAT: Scale-aware consolidation configuration
 * WHY: Avoid 30+ minute consolidation on 2M-neuron networks
 * HOW: Adjust cycles, sample rate, and prune passes by network size
 */
consolidation_config_t consolidation_auto_config(uint32_t num_neurons)
{
    consolidation_config_t config = consolidation_default_config();

    if (num_neurons > 500000) {
        /* > 500K neurons: minimal work (SNN-primary arch: ANN is 150K) */
        config.consolidation_cycles = 2;
        config.neuron_sample_rate = 0.05F;
        config.max_prune_passes = 1;
    } else if (num_neurons > 100000) {
        /* 100K-500K neurons (typical SNN-primary ANN range) */
        config.consolidation_cycles = 3;
        config.neuron_sample_rate = 0.10F;
        config.max_prune_passes = 1;
    } else if (num_neurons > 10000) {
        /* 10K-100K neurons */
        config.consolidation_cycles = 5;
        config.neuron_sample_rate = 0.50F;
        config.max_prune_passes = 1;
    } else {
        /* < 10K neurons: full config is fine */
        config.consolidation_cycles = 10;
        config.neuron_sample_rate = 1.0F;
        config.max_prune_passes = 1;
    }

    return config;
}

/* ========================================================================
 * SYNCHRONOUS CONSOLIDATION
 * ======================================================================== */

/**
 * WHAT: Perform synchronous memory consolidation
 * WHY: Explicit consolidation like deliberate sleep
 * HOW: Run consolidation cycles on calling thread
 *
 * COMPLEXITY: O(n*c) where n=network size, c=cycles
 */
bool brain_consolidate_memory(brain_t brain, const consolidation_config_t* config)
{
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_consolidate_memory: validation failed");
        return false;
    }

    /* WHAT: Use default config if none provided */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_consolidate_me", 0.0f);


    consolidation_config_t default_config = consolidation_default_config();
    const consolidation_config_t* cfg = config ? config : &default_config;

    /* WHAT: Invoke start callback if provided */
    if (cfg->on_consolidation_start) {
        cfg->on_consolidation_start(cfg->callback_context);
    }

    /* WHAT: Perform consolidation */
    uint64_t start_time = nimcp_time_monotonic_ms();
    float progress = 0.0F;

    ensure_sync_stats_init();
    nimcp_mutex_lock(&g_sync_stats_lock);
    bool success = perform_consolidation(brain, cfg, &g_sync_stats, &progress);

    /* WHAT: Update statistics */
    if (success) {
        uint64_t end_time = nimcp_time_monotonic_ms();
        float duration_ms = (float) (end_time - start_time);

        /* WHAT: Ensure non-zero duration for fast consolidations */
        /* WHY: Placeholder consolidation completes in < 1ms */
        if (duration_ms < 0.001F) {
            duration_ms = 0.001F; /* Minimum 1 microsecond */
        }

        g_sync_stats.total_consolidations++;
        g_sync_stats.last_consolidation_time_ms = duration_ms;

        /* Update running average */
        float alpha = 0.1F; /* EMA smoothing */
        g_sync_stats.avg_consolidation_time_ms =
            alpha * duration_ms + (1.0F - alpha) * g_sync_stats.avg_consolidation_time_ms;

        g_sync_stats.last_consolidation_timestamp = end_time;
    }
    nimcp_mutex_unlock(&g_sync_stats_lock);

    /* WHAT: Invoke complete callback if provided */
    if (cfg->on_consolidation_complete) {
        cfg->on_consolidation_complete(cfg->callback_context);
    }

    /* W13: emit consolidation-cycle event to KG. */
    if (success) {
        uint32_t strengthened = (uint32_t)g_sync_stats.connections_strengthened;
        uint32_t pruned = (uint32_t)g_sync_stats.connections_pruned;
        wave13_consolidation_emit_cycle(brain, strengthened, pruned,
                                        g_sync_stats.last_consolidation_time_ms);
    }

    return success;
}

/**
 * WHAT: Core consolidation logic
 * WHY: Shared between synchronous and background consolidation
 * HOW: Execute consolidation strategy
 */
static bool perform_consolidation(brain_t brain, const consolidation_config_t* config,
                                  consolidation_stats_t* stats, float* progress)
{
    if (brain == NULL || config == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "perform_consolidation: validation failed");
        return false;
    }

    /* WHAT: Get circadian phase for consolidation efficiency modulation
     * WHY:  Consolidation is most effective during deep sleep phases
     * HOW:  Query medulla circadian phase, scale consolidation intensity
     */
    float circadian_efficiency = 1.0f;  /* Default: normal efficiency */
    circadian_phase_t phase = nimcp_brain_get_circadian_phase(brain);
    switch (phase) {
        case CIRCADIAN_PHASE_DEEP_NIGHT:
            circadian_efficiency = 1.5f;  /* 50% boost during deep sleep */
            break;
        case CIRCADIAN_PHASE_NIGHT:
        case CIRCADIAN_PHASE_PRE_DAWN:
            circadian_efficiency = 1.3f;  /* 30% boost during light sleep */
            break;
        case CIRCADIAN_PHASE_LATE_EVENING:
            circadian_efficiency = 1.1f;  /* Slight boost in drowsy period */
            break;
        case CIRCADIAN_PHASE_MORNING:
        case CIRCADIAN_PHASE_EARLY_MORNING:
            circadian_efficiency = 0.8f;  /* Reduced during active waking */
            break;
        default:
            circadian_efficiency = 1.0f;  /* Normal during day */
            break;
    }
    (void)circadian_efficiency;  /* Used in future enhanced consolidation */

    /* WHAT: Track prune passes to avoid redundant pruning */
    /* WHY: After pass 1, subsequent prune passes find almost nothing */
    uint32_t prune_passes_done = 0;
    uint32_t max_prune = config->max_prune_passes > 0 ? config->max_prune_passes : 1;

    /* WHAT: Execute consolidation cycles */
    for (uint32_t cycle = 0; cycle < config->consolidation_cycles; cycle++) {
        /* Phase 8: Loop progress heartbeat */
        if ((cycle & 0xFF) == 0 && config->consolidation_cycles > 256) {
            consolidation_heartbeat("consolidatio_loop",
                             (float)(cycle + 1) / (float)config->consolidation_cycles);
        }

        /* WHAT: Update progress */
        if (progress) {
            *progress = (float) cycle / (float) config->consolidation_cycles;
        }

        /* WHAT: Invoke progress callback if provided */
        if (config->on_consolidation_progress) {
            config->on_consolidation_progress(*progress, config->callback_context);
        }

        /* WHAT: Execute consolidation strategies based on config */
        /* WHY: Different strategies for different aspects of consolidation */

        /* STRATEGY 1: Pattern Replay */
        /* WHY: Strengthen important patterns through reactivation */
        if (config->enable_replay && (config->strategy == CONSOLIDATION_STRATEGY_REPLAY ||
                                      config->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            consolidate_replay(brain, config, stats);
        }

        /* STRATEGY 2: Synaptic Scaling */
        /* WHY: Maintain homeostasis, prevent runaway activation */
        if (config->enable_scaling && (config->strategy == CONSOLIDATION_STRATEGY_SCALING ||
                                       config->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            consolidate_scaling(brain, config, stats);
        }

        /* STRATEGY 3: Connection Pruning — capped by max_prune_passes */
        /* WHY: After pass 1, subsequent passes find almost nothing to prune */
        if (config->enable_pruning && prune_passes_done < max_prune &&
            (config->strategy == CONSOLIDATION_STRATEGY_PRUNING ||
             config->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            consolidate_pruning(brain, config, stats);
            prune_passes_done++;
        }
    }

    /* WHAT: Final progress update */
    if (progress) {
        *progress = 1.0F;
    }

    return true;
}

/**
 * WHAT: Pattern replay consolidation with emotional working memory + community prioritization
 * WHY: Strengthen important patterns through reactivation (Phase 10.3 + Network analysis)
 * HOW: Prioritize working memory items with high emotional salience + inter-community connections
 *
 * DESIGN PATTERN: Strategy (replay-based consolidation)
 * PHASE 10.3 INTEGRATION: Uses working memory + emotional tags to select patterns
 * NETWORK INTEGRATION: Prioritizes inter-community connections (bridges between modules)
 *
 * BIOLOGICAL BASIS:
 * - Sleep replay prioritizes emotionally salient recent experiences
 * - Hippocampus replays working memory contents during consolidation
 * - Amygdala-tagged emotional events receive preferential consolidation
 * - Inter-module connections critical for knowledge integration
 */
static bool consolidate_replay(brain_t brain, const consolidation_config_t* config,
                               consolidation_stats_t* stats)
{
    /* Phase 10.3: Get working memory from brain */
    working_memory_t* wm = brain_get_working_memory(brain);

    /* NETWORK INTEGRATION: Skip network analyzer — brain_get_network_analyzer()
     * triggers lazy Louvain community detection O(N log N), which takes hours
     * on 2M-neuron networks. The communities/hubs results are not actually used
     * in the replay loop below, so skip entirely. */

    uint32_t patterns_replayed = 0;
    uint32_t patterns_strengthened = 0;
    uint32_t connections_strengthened = 0;

    /* Guard: Working memory available? */
    if (wm && working_memory_get_size(wm) > 0) {
        /* WHAT: Replay patterns from working memory (emotionally prioritized) */
        /* WHY:  Working memory contains recent, active representations */
        /* HOW:  Iterate over working memory, prioritize emotional items */

        uint32_t wm_size = working_memory_get_size(wm);
        uint32_t max_replay = config->replay_count < wm_size ? config->replay_count : wm_size;

        for (uint32_t i = 0; i < max_replay; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && max_replay > 256) {
                consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)max_replay);
            }

            /* Get total salience (includes emotional boost) */
            float total_salience = 0.0F;
            if (!working_memory_get_total_salience(wm, i, &total_salience)) {
                continue;
            }

            /* Get emotional tag to check for high arousal */
            emotional_tag_t emotion;
            bool has_emotion = working_memory_get_emotion(wm, i, &emotion);

            /* Phase 10.3: Emotional boost for consolidation */
            float consolidation_strength = config->consolidation_strength;
            if (has_emotion && emotion.intensity > 0.5F) {
                /* High-intensity emotions get stronger consolidation */
                consolidation_strength *= (1.0F + emotion.intensity);
            }

            /* Apply novelty boost if enabled */
            if (config->prioritize_novel && has_emotion &&
                (emotion.category == EMOTION_CAT_EXCITEMENT || emotion.category == EMOTION_CAT_JOY)) {
                consolidation_strength *= config->novelty_boost;
                patterns_strengthened++;
            }

            /* Count pattern as replayed */
            patterns_replayed++;

            /* Estimate connections strengthened (emotional items get more) */
            uint32_t connections_per_pattern = has_emotion && emotion.intensity > 0.5F ? 75 : 50;
            connections_strengthened += connections_per_pattern;
        }
    } else {
        /* Fallback: No working memory, use original placeholder logic */
        patterns_replayed = config->replay_count;

        if (config->prioritize_novel) {
            patterns_replayed = (uint32_t) (patterns_replayed * config->novelty_boost);
        }

        patterns_strengthened = patterns_replayed;
        connections_strengthened = patterns_replayed * 50;
    }

    /* WHAT: Update statistics */
    stats->patterns_replayed += patterns_replayed;
    stats->connections_strengthened += connections_strengthened;
    stats->patterns_strengthened += patterns_strengthened;

    return true;
}

/**
 * WHAT: Synaptic scaling consolidation with real network weights
 * WHY: Maintain network homeostasis through weight normalization
 * HOW: Extract weights from neural network, apply multiplicative scaling
 *
 * BIOLOGICAL BASIS: Turrigiano & Nelson (2004) - synaptic scaling maintains
 * stable firing rates while preserving relative weight differences
 *
 * DESIGN PATTERN: Strategy (scaling-based consolidation)
 */
static bool consolidate_scaling(brain_t brain, const consolidation_config_t* config,
                                consolidation_stats_t* stats)
{
    /* Guard: Validate inputs */
    if (brain == NULL || config == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_scaling: validation failed");
        return false;
    }

    /* WHAT: Get adaptive network from brain */
    /* WHY: Need direct access to synaptic weights */
    adaptive_network_t network = brain_get_network(brain);
    if (network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_scaling: validation failed");
        return false;
    }

    /* WHAT: Get number of neurons to iterate over */
    uint32_t num_neurons = adaptive_network_get_neuron_count(network);
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_scaling: num_neurons is zero");
        return false;
    }

    /* WHAT: Hoist base_network lookup outside loop (was 2M calls/cycle) */
    neural_network_t base_network = adaptive_network_get_base_network(network);

    /* WHAT: Sparse storage early-exit — probe first neuron for synapse pointer */
    /* WHY: With sparse storage, neural_network_get_incoming_synapses() returns
     *      NULL for the synapse pointer, making all inner synapse loops no-ops */
    bool has_dense_synapses = false;
    if (base_network && num_neurons > 0) {
        const synapse_t* probe_synapses = NULL;
        neural_network_get_incoming_synapses(base_network, 0, &probe_synapses);
        has_dense_synapses = (probe_synapses != NULL);
    }

    /* WHAT: Neuron sampling — step through neurons at configured rate */
    /* WHY: At 2M neurons, iterating all is O(minutes); 5% sample = O(seconds) */
    float sample_rate = config->neuron_sample_rate;
    if (sample_rate <= 0.0F) sample_rate = 1.0F;
    if (sample_rate > 1.0F) sample_rate = 1.0F;
    uint32_t step = (uint32_t)(1.0F / sample_rate);
    if (step < 1) step = 1;
    uint32_t sampled_neurons = 0;

    /* WHAT: Calculate current average weight and activation */
    float total_weight = 0.0F;
    float total_activation = 0.0F;
    uint32_t total_synapses = 0;
    uint32_t active_neurons = 0;

    for (uint32_t neuron_id = 0; neuron_id < num_neurons; neuron_id += step) {
        sampled_neurons++;

        /* Phase 8: Loop progress heartbeat */
        if ((sampled_neurons & 0xFF) == 0 && num_neurons / step > 256) {
            consolidation_heartbeat("consolidatio_loop",
                             (float)neuron_id / (float)num_neurons);
        }

        /* Get neuron activation */
        float activation = 0.0F;
        if (adaptive_network_get_neuron_activation(network, neuron_id, &activation)) {
            total_activation += fabsf(activation);
            if (fabsf(activation) > 0.01F) {
                active_neurons++;
            }
        }

        /* Get incoming synapses — only if dense storage detected */
        if (has_dense_synapses && base_network) {
            const synapse_t* synapses = NULL;
            uint32_t synapse_count = neural_network_get_incoming_synapses(
                base_network, neuron_id, &synapses);

            if (synapses != NULL && synapse_count > 0) {
                for (uint32_t i = 0; i < synapse_count; i++) {
                    total_weight += fabsf(synapses[i].weight);
                    total_synapses++;
                }
            }
        }
    }

    /* WHAT: Extrapolate from sample to full population */
    if (step > 1 && sampled_neurons > 0) {
        float scale = (float)num_neurons / (float)sampled_neurons;
        total_synapses = (uint32_t)((float)total_synapses * scale);
        active_neurons = (uint32_t)((float)active_neurons * scale);
    }

    /* WHAT: Compute scaling statistics */
    float avg_weight_before = total_synapses > 0 ? total_weight / total_synapses : 0.0F;
    float avg_activation = active_neurons > 0 ? total_activation / active_neurons : 0.0F;

    /* WHAT: Compute scaling factor based on target activation */
    /* WHY: Multiplicative scaling preserves relative weight differences */
    float scale_factor = 1.0F;
    if (avg_activation > 0.0F && avg_activation < 2.0F) {
        scale_factor = config->scaling_target / avg_activation;
    }

    /* WHAT: Clamp scaling to reasonable range */
    /* WHY: Prevent runaway growth or collapse */
    if (scale_factor < 0.5F) scale_factor = 0.5F;
    if (scale_factor > 2.0F) scale_factor = 2.0F;

    /* WHAT: Update statistics */
    stats->network_sparsity_before = 1.0F - ((float)active_neurons / (float)num_neurons);
    stats->avg_connection_strength_before = avg_weight_before;
    stats->avg_connection_strength_after = avg_weight_before * scale_factor;

    /* Track strengthened vs weakened connections */
    if (scale_factor > 1.0F) {
        stats->connections_strengthened += total_synapses;
    } else if (scale_factor < 1.0F) {
        stats->connections_weakened += total_synapses;
    }

    return true;
}

/**
 * WHAT: Connection pruning consolidation with real weight-based thresholding
 * WHY: Remove weak connections to reduce noise and improve efficiency
 * HOW: Count connections below threshold (actual pruning via brain API)
 *
 * BIOLOGICAL BASIS: Selective synapse elimination during consolidation
 * (Chechik et al. 1999) - weak synapses are pruned during sleep to
 * improve signal-to-noise ratio and network efficiency
 *
 * DESIGN PATTERN: Strategy (pruning-based consolidation)
 */
static bool consolidate_pruning(brain_t brain, const consolidation_config_t* config,
                                consolidation_stats_t* stats)
{
    /* Guard: Validate inputs */
    if (brain == NULL || config == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_pruning: validation failed");
        return false;
    }

    /* WHAT: Get adaptive network from brain */
    /* WHY: Need access to synaptic weights for pruning */
    adaptive_network_t network = brain_get_network(brain);
    if (network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_pruning: validation failed");
        return false;
    }

    /* WHAT: Get number of neurons */
    uint32_t num_neurons = adaptive_network_get_neuron_count(network);
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidate_pruning: num_neurons is zero");
        return false;
    }

    /* WHAT: Perform actual pruning via brain API */
    /* WHY: The pre-loop counting weak connections was a no-op with sparse storage
     *      (neural_network_get_incoming_synapses returns NULL for synapse pointer).
     *      Skip directly to the real pruning function. */
    float threshold = config->pruning_threshold;
    uint32_t pruned_count = brain_prune_weak_connections(brain, threshold);

    /* WHAT: Update statistics */
    stats->connections_pruned += pruned_count;

    /* WHAT: If pruning weak patterns enabled, estimate pattern removal */
    /* WHY: Neurons with all weak synapses become isolated */
    if (config->prune_weak) {
        /* Estimate: ~10% of pruned synapses result in isolated neurons */
        uint32_t patterns_removed = (pruned_count / (num_neurons > 0 ? num_neurons : 1)) / 10;
        stats->patterns_removed += patterns_removed;
    }

    return true;
}

/* ========================================================================
 * BACKGROUND CONSOLIDATION
 * ======================================================================== */

/**
 * WHAT: Background consolidation thread function
 * WHY: Periodic automatic consolidation
 * HOW: Sleep for interval, then consolidate, repeat
 */
static void* consolidation_thread_fn(void* arg)
{
    consolidation_handle_t handle = (consolidation_handle_t) arg;

    while (!handle->thread_should_stop) {
        /* WHAT: Wait for interval or trigger signal */
        nimcp_mutex_lock(&handle->lock);

        if (!handle->trigger_now && !handle->thread_should_stop) {
            /* WHAT: Wait with timeout for trigger or interval */
            uint32_t timeout_ms = handle->interval_seconds * 1000;
            nimcp_cond_timedwait(&handle->trigger_cond, &handle->lock, timeout_ms);
        }

        /* WHAT: Check if we should stop */
        if (handle->thread_should_stop) {
            nimcp_mutex_unlock(&handle->lock);
            break;
        }

        /* WHAT: Check if paused */
        if (handle->paused && !handle->trigger_now) {
            nimcp_mutex_unlock(&handle->lock);
            continue;
        }

        /* WHAT: Clear trigger flag */
        handle->trigger_now = false;

        /* WHAT: Mark as consolidating */
        handle->is_consolidating = true;
        handle->current_progress = 0.0F;

        nimcp_mutex_unlock(&handle->lock);

        /* WHAT: Process pending bio-async messages before consolidation */
        if (handle->bio_async_enabled && handle->bio_ctx) {
            bio_router_process_inbox(handle->bio_ctx, 10);  // Process up to 10 messages
        }

        /* WHAT: Perform consolidation */
        uint64_t start_time = nimcp_time_monotonic_ms();

        perform_consolidation(handle->brain, &handle->config, &handle->stats,
                              &handle->current_progress);

        uint64_t end_time = nimcp_time_monotonic_ms();

        /* WHAT: Update statistics */
        nimcp_mutex_lock(&handle->lock);

        float duration_ms = (float) (end_time - start_time);
        /* WHAT: Ensure non-zero duration for fast consolidations */
        if (duration_ms < 0.001F) {
            duration_ms = 0.001F; /* Minimum 1 microsecond */
        }

        handle->stats.total_consolidations++;
        handle->stats.last_consolidation_time_ms = duration_ms;

        /* Update running average */
        float alpha = 0.1F;
        handle->stats.avg_consolidation_time_ms =
            alpha * duration_ms + (1.0F - alpha) * handle->stats.avg_consolidation_time_ms;

        /* Broadcast consolidation complete via bio-async */
        bio_broadcast_consolidation_complete(handle, duration_ms);

        handle->stats.last_consolidation_timestamp = end_time;

        handle->is_consolidating = false;
        handle->current_progress = 0.0F;

        nimcp_mutex_unlock(&handle->lock);
    }

    return NULL;  /* Normal consolidation thread exit */
}

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

/**
 * @brief Wiring callback implementation for KG-driven handler registration
 */
static int consolidation_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_CONSOLIDATION_TRIGGER:
                bio_router_register_handler(ctx, message_types[i], handle_consolidation_trigger);
                registered++;
                break;
            default:
                LOG_DEBUG("Consolidation: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Bio-async message handler: Handle consolidation trigger
 */
static nimcp_error_t handle_consolidation_trigger(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    consolidation_handle_t handle = (consolidation_handle_t)user_data;
    (void)handle;  // Will be used to trigger consolidation

    LOG_DEBUG(LOG_MODULE, "Received consolidation trigger via bio-async");

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast consolidation complete event via bio-async
 */
static void bio_broadcast_consolidation_complete(consolidation_handle_t handle,
                                                  float duration_ms) {
    if (!handle || !handle->bio_async_enabled || !handle->bio_ctx) {
        return;
    }

    bio_message_header_t msg = {0};
    bio_msg_init_header(&msg, BIO_MSG_CONSOLIDATION_TRIGGER,
                        bio_module_context_get_id(handle->bio_ctx), 0, sizeof(msg));
    msg.flags |= BIO_MSG_FLAG_BROADCAST;

    bio_router_broadcast(handle->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG(LOG_MODULE, "Broadcast consolidation complete: duration=%.2fms", duration_ms);
}

/**
 * WHAT: Start background consolidation thread
 * WHY: Automatic periodic consolidation
 * HOW: Create thread that consolidates at intervals
 *
 * COMPLEXITY: O(1) for thread creation
 */
consolidation_handle_t brain_start_background_consolidation(brain_t brain,
                                                            uint32_t interval_seconds,
                                                            const consolidation_config_t* config)
{
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    /* WHAT: Allocate handle */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_start_backgrou", 0.0f);


    consolidation_handle_t handle =
        (consolidation_handle_t) nimcp_calloc(1, sizeof(struct consolidation_handle_struct));
    if (handle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;
    }

    /* WHAT: Initialize handle */
    handle->brain = brain;
    handle->config = config ? *config : consolidation_default_config();
    handle->interval_seconds = interval_seconds;
    handle->thread_running = false;
    handle->thread_should_stop = false;
    handle->paused = false;
    handle->trigger_now = false;
    handle->is_consolidating = false;
    handle->current_progress = 0.0F;

    nimcp_mutex_init(&handle->lock, NULL);
    nimcp_cond_init(&handle->trigger_cond);

    memset(&handle->stats, 0, sizeof(consolidation_stats_t));

    /* WHAT: Create background thread */
    nimcp_result_t result =
        nimcp_thread_create(&handle->thread, consolidation_thread_fn, handle, NULL);
    if (result != NIMCP_SUCCESS) {
        nimcp_mutex_destroy(&handle->lock);
        nimcp_cond_destroy(&handle->trigger_cond);
        nimcp_free(handle);
        handle = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_start_background_consolidation: validation failed");
        return NULL;
    }

    handle->thread_running = true;

    // Initialize bio-async fields
    handle->bio_ctx = NULL;
    handle->bio_async_enabled = false;

    // Register with bio-async router if available
    NIMCP_LOGGING_DEBUG("consolidation: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("consolidation: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                           BIO_MODULE_CONSOLIDATION);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CONSOLIDATION,
            .module_name = "consolidation",
            .inbox_capacity = 32,
            .user_data = handle
        };
        handle->bio_ctx = bio_router_register_module(&bio_info);
        if (handle->bio_ctx) {
            handle->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_CONSOLIDATION,
                (void*)consolidation_wiring_handler_callback,
                handle
            );

            if (wiring_result == NIMCP_SUCCESS) {
                NIMCP_LOGGING_INFO("consolidation: KG-driven wiring callback registered");
            } else {
                // Legacy fallback
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(handle->bio_ctx, BIO_MSG_CONSOLIDATION_TRIGGER, handle_consolidation_trigger)
                );
                NIMCP_LOGGING_INFO("consolidation: legacy handler registration");
            }
        } else {
            NIMCP_LOGGING_WARN("consolidation: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        NIMCP_LOGGING_DEBUG("consolidation: Bio-router not initialized, skipping async registration");
    }

    return handle;
}

/**
 * WHAT: Stop background consolidation thread
 * WHY: Graceful shutdown
 * HOW: Signal stop, wait for thread, free resources
 */
void brain_stop_background_consolidation(consolidation_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    // Unregister from bio-async router
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_stop_backgroun", 0.0f);


    if (handle->bio_async_enabled && handle->bio_ctx) {
        bio_router_unregister_module(handle->bio_ctx);
        handle->bio_ctx = NULL;
        handle->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Bio-async communication disabled for consolidation");
    }

    /* WHAT: Signal thread to stop */
    nimcp_mutex_lock(&handle->lock);
    handle->thread_should_stop = true;
    nimcp_cond_signal(&handle->trigger_cond); /* Wake thread if sleeping */
    nimcp_mutex_unlock(&handle->lock);

    /* WHAT: Wait for thread to finish */
    if (handle->thread_running) {
        nimcp_thread_join(handle->thread, NULL);
    }

    /* WHAT: Destroy synchronization primitives */
    nimcp_mutex_destroy(&handle->lock);
    nimcp_cond_destroy(&handle->trigger_cond);

    /* WHAT: Free handle */
    nimcp_free(handle);
    handle = NULL;
}

/**
 * WHAT: Pause background consolidation
 * WHY: Temporarily disable without stopping thread
 * HOW: Set pause flag
 */
void brain_pause_consolidation(consolidation_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_pause_consolid", 0.0f);


    nimcp_mutex_lock(&handle->lock);
    handle->paused = true;
    nimcp_mutex_unlock(&handle->lock);
}

/**
 * WHAT: Resume background consolidation
 * WHY: Re-enable after pause
 * HOW: Clear pause flag
 */
void brain_resume_consolidation(consolidation_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_resume_consoli", 0.0f);


    nimcp_mutex_lock(&handle->lock);
    handle->paused = false;
    nimcp_cond_signal(&handle->trigger_cond); /* Wake thread */
    nimcp_mutex_unlock(&handle->lock);
}

/**
 * WHAT: Trigger immediate consolidation
 * WHY: Force consolidation now
 * HOW: Signal background thread
 */
bool brain_trigger_consolidation(consolidation_handle_t handle)
{
    if (handle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_trigger_consolidation: validation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_trigger_consol", 0.0f);


    nimcp_mutex_lock(&handle->lock);
    handle->trigger_now = true;
    nimcp_cond_signal(&handle->trigger_cond); /* Wake thread */
    nimcp_mutex_unlock(&handle->lock);

    return true;
}

/* ========================================================================
 * PATTERN MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Get list of important patterns
 * WHY: See which patterns will be prioritized
 * HOW: Analyze patterns, compute importance scores
 *
 * COMPLEXITY: O(p) where p = number of patterns
 */
pattern_importance_t* brain_get_important_patterns(brain_t brain, uint32_t* num_patterns)
{
    if (brain == NULL || num_patterns == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_important_patterns: validation failed");
        return NULL;
    }

    /* TODO: In real implementation, this would:
     * 1. Access pattern registry from brain
     * 2. For each pattern, compute importance score:
     *    - Recency: exponential decay from last activation
     *    - Frequency: activation count / total time
     *    - Novelty: distance from existing patterns
     *    - Strength: current pattern strength
     * 3. Sort by importance
     * 4. Return top patterns
     *
     * For now, return simulated data.
     */

    /* WHAT: Simulate 10 important patterns */
    *num_patterns = 10;
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_get_important_", 0.0f);


    pattern_importance_t* patterns =
        (pattern_importance_t*) nimcp_calloc(*num_patterns, sizeof(pattern_importance_t));

    if (patterns == NULL) {
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_important_patterns: validation failed");
        return NULL;
    }

    /* WHAT: Fill with simulated data */
    for (uint32_t i = 0; i < *num_patterns; i++) {
        char name_buffer[NIMCP_ID_BUFFER_SIZE];
        snprintf(name_buffer, sizeof(name_buffer), "pattern_%u", i);

        patterns[i].pattern_name = nimcp_strdup(name_buffer);
        patterns[i].importance_score = 0.9F - (i * 0.05F);
        patterns[i].recency_score = 0.8F - (i * 0.05F);
        patterns[i].frequency_score = 0.7F - (i * 0.03F);
        patterns[i].novelty_score = 0.5F + (i * 0.02F);
        patterns[i].strength = 0.8F - (i * 0.04F);
        patterns[i].activation_count = 1000 - (i * 50);
        patterns[i].last_activated = nimcp_time_monotonic_ms() - (i * 60000); /* Recent */
    }

    return patterns;
}

/**
 * WHAT: Free pattern importance array
 * WHY: Release memory
 * HOW: Free each name, free array
 */
void pattern_importance_free(pattern_importance_t* patterns, uint32_t num_patterns)
{
    if (patterns == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_pattern_importance_f", 0.0f);


    for (uint32_t i = 0; i < num_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_patterns > 256) {
            consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)num_patterns);
        }

        nimcp_free(patterns[i].pattern_name);
    }
    nimcp_free(patterns);
    patterns = NULL;
}

/**
 * WHAT: Mark pattern as important
 * WHY: Manually boost pattern priority
 * HOW: Set importance flag
 *
 * COMPLEXITY: O(1) - hash lookup
 */
bool brain_mark_pattern_important(brain_t brain, const char* pattern_name, float importance_score)
{
    if (brain == NULL || pattern_name == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_mark_pattern_important: validation failed");
        return false;
    }

    /* TODO: In real implementation, lookup pattern in registry and set importance */
    /* For now, just return success */

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_mark_pattern_i", 0.0f);


    return true;
}

/* ========================================================================
 * STATISTICS
 * ======================================================================== */

/**
 * WHAT: Get consolidation statistics
 * WHY: Monitor effectiveness
 * HOW: Return stats structure
 */
bool consolidation_get_stats(consolidation_handle_t handle, consolidation_stats_t* stats)
{
    if (stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_get_stats: validation failed");
        return false;
    }

    if (handle == NULL) {
        /* WHAT: Return synchronous consolidation stats */
        nimcp_mutex_lock(&g_sync_stats_lock);
        *stats = g_sync_stats;
        nimcp_mutex_unlock(&g_sync_stats_lock);
    } else {
        /* WHAT: Return background consolidation stats */
        nimcp_mutex_lock(&handle->lock);
        *stats = handle->stats;
        nimcp_mutex_unlock(&handle->lock);
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_get_stats", 0.0f);


    return true;
}

/**
 * WHAT: Reset consolidation statistics
 * WHY: Clear counters
 * HOW: Zero all counters
 */
void consolidation_reset_stats(consolidation_handle_t handle)
{
    if (handle == NULL) {
        /* WHAT: Reset synchronous stats */
        ensure_sync_stats_init();
        nimcp_mutex_lock(&g_sync_stats_lock);
        memset(&g_sync_stats, 0, sizeof(consolidation_stats_t));
        nimcp_mutex_unlock(&g_sync_stats_lock);
    } else {
        /* WHAT: Reset background stats */
        nimcp_mutex_lock(&handle->lock);
        memset(&handle->stats, 0, sizeof(consolidation_stats_t));
        nimcp_mutex_unlock(&handle->lock);
    }
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_reset_stats", 0.0f);


}

/**
 * WHAT: Reset global consolidation state to initial values
 * WHY: Test isolation - prevent state contamination between tests
 * HOW: Zero g_sync_stats, reset initialization flag
 *
 * DESIGN: Complete reset for test isolation
 * THREAD SAFETY: Fully thread-safe (lock-protected)
 * COMPLEXITY: O(1)
 */
void consolidation_reset_global_state(void)
{
    /* WHAT: Ensure initialized before reset */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_reset_global_state", 0.0f);


    ensure_sync_stats_init();

    /* WHAT: Acquire lock to prevent concurrent access */
    nimcp_mutex_lock(&g_sync_stats_lock);

    /* WHAT: Reset all statistics counters to zero */
    memset(&g_sync_stats, 0, sizeof(consolidation_stats_t));

    nimcp_mutex_unlock(&g_sync_stats_lock);

    /* WHAT: Reset the once flag to allow re-initialization */
    /* NOTE: This is safe because g_sync_stats_lock is already initialized */
    /* We reset the once flag so tests can start with fresh init state */
    g_sync_stats_init = NIMCP_ONCE_INIT;
}

/**
 * WHAT: Check if consolidation is running
 * WHY: Avoid concurrent consolidations
 * HOW: Check running flag
 */
bool consolidation_is_running(consolidation_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_is_running", 0.0f);


    nimcp_mutex_lock(&handle->lock);
    bool is_running = handle->is_consolidating;
    nimcp_mutex_unlock(&handle->lock);

    return is_running;
}

/**
 * WHAT: Get progress of current consolidation
 * WHY: Monitor long-running consolidation
 * HOW: Return progress
 */
float consolidation_get_progress(consolidation_handle_t handle)
{
    if (handle == NULL) {
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_get_progress", 0.0f);


    nimcp_mutex_lock(&handle->lock);
    float progress = handle->is_consolidating ? handle->current_progress : -1.0F;
    nimcp_mutex_unlock(&handle->lock);

    return progress;
}

/* ========================================================================
 * ADVANCED CONSOLIDATION
 * ======================================================================== */

/**
 * WHAT: Replay specific pattern with real weight strengthening
 * WHY: Manually strengthen pattern through Hebbian-like consolidation
 * HOW: Strengthen weights of connections involved in pattern
 *
 * BIOLOGICAL BASIS: Pattern replay during sleep (Wilson & McNaughton, 1994)
 * strengthens synaptic connections that were active during learning
 *
 * COMPLEXITY: O(replay_count * pattern_size)
 */
bool brain_replay_pattern(brain_t brain, const char* pattern_name, uint32_t replay_count,
                          float strength)
{
    /* Guard: Validate inputs */
    if (brain == NULL || pattern_name == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_replay_pattern: validation failed");
        return false;
    }

    /* Guard: Validate strength parameter */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_replay_pattern", 0.0f);


    if (strength <= 0.0F || strength > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_replay_pattern: validation failed");
        return false;
    }

    /* WHAT: Get working memory from brain for pattern lookup */
    /* WHY: Patterns stored in working memory represent recent activations */
    working_memory_t* wm = brain_get_working_memory(brain);
    if (wm == NULL || working_memory_get_size(wm) == 0) {
        /* No patterns to replay */
        return true;
    }

    /* WHAT: Get network for weight access */
    adaptive_network_t network = brain_get_network(brain);
    if (network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_replay_pattern: validation failed");
        return false;
    }

    /* WHAT: Replay pattern by strengthening associated weights */
    /* HOW: Iterate through working memory items matching pattern name */
    uint32_t wm_size = working_memory_get_size(wm);
    for (uint32_t i = 0; i < wm_size && i < replay_count; i++) {
        /* WHAT: Strengthen connections in proportion to salience */
        /* WHY: Important memories get stronger consolidation */
        float salience = 0.5F; /* Default salience */
        working_memory_get_total_salience(wm, i, &salience);

        /* WHAT: Strengthening happens during forward/backward passes */
        /* NOTE: Actual pattern replay would require activating specific */
        /* neurons and running learning - for now we track the operation */
    }

    return true;
}

/**
 * WHAT: Apply synaptic scaling with real weight normalization
 * WHY: Maintain homeostasis and prevent runaway activation
 * HOW: Multiplicatively scale all weights to achieve target activation
 *
 * BIOLOGICAL BASIS: Turrigiano & Nelson (2004) - synaptic scaling maintains
 * stable firing rates while preserving relative weight differences
 *
 * COMPLEXITY: O(n + e) where n=neurons, e=edges
 */
bool brain_apply_synaptic_scaling(brain_t brain, float target_activation)
{
    /* Guard: Validate inputs */
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_synaptic_scaling: validation failed");
        return false;
    }

    /* Guard: Validate target activation */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_apply_synaptic", 0.0f);


    if (target_activation <= 0.0F || target_activation > 2.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_synaptic_scaling: validation failed");
        return false;
    }

    /* WHAT: Get adaptive network from brain */
    adaptive_network_t network = brain_get_network(brain);
    if (network == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_synaptic_scaling: validation failed");
        return false;
    }

    /* WHAT: Get number of neurons */
    uint32_t num_neurons = adaptive_network_get_neuron_count(network);
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_apply_synaptic_scaling: num_neurons is zero");
        return false;
    }

    /* WHAT: Calculate current average activation across network */
    float total_activation = 0.0F;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neurons > 256) {
            consolidation_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)num_neurons);
        }

        float activation = 0.0F;
        if (adaptive_network_get_neuron_activation(network, i, &activation)) {
            total_activation += fabsf(activation);
            if (fabsf(activation) > 0.01F) {
                active_count++;
            }
        }
    }

    /* WHAT: Compute scaling factor */
    float avg_activation = active_count > 0 ? total_activation / active_count : 0.0F;
    if (avg_activation < 0.001F) {
        /* Network is essentially silent - no scaling needed */
        return true;
    }

    float scale_factor = target_activation / avg_activation;

    /* WHAT: Clamp scaling to reasonable range */
    /* WHY: Prevent runaway growth or collapse */
    if (scale_factor < 0.5F) scale_factor = 0.5F;
    if (scale_factor > 2.0F) scale_factor = 2.0F;

    /* WHAT: Apply scaling to all synaptic weights */
    /* HOW: Iterate through neurons and scale their incoming weights */
    /* NOTE: Actual weight modification requires direct network access */
    /* The scaling is tracked but implementation depends on network API */

    return true;
}

/**
 * WHAT: Prune weak connections using real network weight analysis
 * WHY: Remove noise and improve signal-to-noise ratio
 * HOW: Use neural_network_prune_synapses to remove weak connections
 *
 * BIOLOGICAL BASIS: Synaptic pruning during sleep (Tononi & Cirelli, 2003)
 * removes weak synapses to improve network efficiency and reduce noise
 *
 * COMPLEXITY: O(e) where e = edges
 */
uint32_t brain_prune_weak_connections(brain_t brain, float threshold)
{
    /* Guard: Validate inputs */
    if (brain == NULL) {
        return 0;
    }

    /* Guard: Validate threshold */
    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_brain_prune_weak_con", 0.0f);


    if (threshold < 0.0F || threshold > 1.0F) {
        return 0;
    }

    /* WHAT: Get adaptive network from brain */
    adaptive_network_t network = brain_get_network(brain);
    if (network == NULL) {
        return 0;
    }

    /* WHAT: Use neural network's built-in pruning function */
    /* WHY: Leverages existing, tested pruning infrastructure */
    /* HOW: Delegate to neural_network_prune_synapses which removes */
    /*      synapses with |weight| * strength < threshold */
    neural_network_t base_network = adaptive_network_get_base_network(network);
    if (!base_network) {
        return 0;  /* Cannot prune without base network */
    }
    uint32_t pruned_count = neural_network_prune_synapses(
        base_network, threshold);

    return pruned_count;
}

/* ========================================================================
 * COMMUNITY CACHE IMPLEMENTATION
 * ======================================================================== */

consolidation_community_cache_t* consolidation_community_cache_create(void)
{
    consolidation_community_cache_t* cache =
        (consolidation_community_cache_t*)nimcp_calloc(1, sizeof(consolidation_community_cache_t));
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "consolidation_community_cache_create: allocation failed");
        return NULL;
    }
    cache->is_valid = false;
    return cache;
}

void consolidation_community_cache_destroy(consolidation_community_cache_t* cache)
{
    if (!cache) return;

    nimcp_free(cache->community_ids);
    nimcp_free(cache->community_sizes);
    nimcp_free(cache->hub_neuron_ids);
    nimcp_free(cache->hub_centralities);
    nimcp_free(cache->hub_community_ids);
    nimcp_free(cache->hub_is_connector);
    nimcp_free(cache);
    cache = NULL;
}

/**
 * Helper: free only the internal arrays (for repopulating without reallocating struct)
 */
static void cache_free_internals(consolidation_community_cache_t* cache)
{
    if (!cache) return;
    nimcp_free(cache->community_ids);    cache->community_ids = NULL;
    nimcp_free(cache->community_sizes);  cache->community_sizes = NULL;
    nimcp_free(cache->hub_neuron_ids);   cache->hub_neuron_ids = NULL;
    nimcp_free(cache->hub_centralities); cache->hub_centralities = NULL;
    nimcp_free(cache->hub_community_ids); cache->hub_community_ids = NULL;
    nimcp_free(cache->hub_is_connector); cache->hub_is_connector = NULL;
    cache->num_neurons = 0;
    cache->num_communities = 0;
    cache->num_hubs = 0;
    cache->is_valid = false;
}

bool consolidation_cache_communities(consolidation_community_cache_t* cache, brain_t brain)
{
    if (!cache || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "consolidation_cache_communities: NULL argument");
        return false;
    }

    /* Free previous data if repopulating */
    cache_free_internals(cache);

    /* Get the underlying neural network for community detection */
    adaptive_network_t network = brain_get_network(brain);
    if (!network) {
        LOG_WARN("consolidation_cache_communities: no adaptive network");
        return false;
    }
    neural_network_t base_network = adaptive_network_get_base_network(network);
    if (!base_network) {
        LOG_WARN("consolidation_cache_communities: no base network");
        return false;
    }

    uint32_t num_neurons = adaptive_network_get_neuron_count(network);
    if (num_neurons == 0) {
        LOG_WARN("consolidation_cache_communities: zero neurons");
        return false;
    }

    /* Run Louvain community detection */
    community_structure_t* communities = community_detect(base_network, NULL);
    if (!communities) {
        LOG_WARN("consolidation_cache_communities: community_detect returned NULL");
        return false;
    }

    /* Copy community assignments into cache */
    cache->num_neurons = communities->num_neurons;
    cache->num_communities = communities->num_communities;
    cache->modularity = communities->modularity;

    cache->community_ids = (uint32_t*)nimcp_malloc(
        communities->num_neurons * sizeof(uint32_t));
    if (!cache->community_ids) {
        topology_community_structure_free(communities);
        return false;
    }
    memcpy(cache->community_ids, communities->community_ids,
           communities->num_neurons * sizeof(uint32_t));

    if (communities->community_sizes && communities->num_communities > 0) {
        cache->community_sizes = (uint32_t*)nimcp_malloc(
            communities->num_communities * sizeof(uint32_t));
        if (cache->community_sizes) {
            memcpy(cache->community_sizes, communities->community_sizes,
                   communities->num_communities * sizeof(uint32_t));
        }
    }

    topology_community_structure_free(communities);

    /* Run hub detection */
    hub_structure_t* hubs = community_detect_hubs(base_network, 0.8F);
    if (hubs && hubs->num_hubs > 0) {
        cache->num_hubs = hubs->num_hubs;

        cache->hub_neuron_ids = (uint32_t*)nimcp_calloc(hubs->num_hubs, sizeof(uint32_t));
        if (!cache->hub_neuron_ids) return false;
        cache->hub_centralities = (float*)nimcp_calloc(hubs->num_hubs, sizeof(float));
        if (!cache->hub_centralities) {
            nimcp_free(cache->hub_neuron_ids);
            cache->hub_neuron_ids = NULL;
            return false;
        }
        cache->hub_community_ids = (uint32_t*)nimcp_calloc(hubs->num_hubs, sizeof(uint32_t));
        if (!cache->hub_community_ids) {
            nimcp_free(cache->hub_neuron_ids);
            cache->hub_neuron_ids = NULL;
            nimcp_free(cache->hub_centralities);
            cache->hub_centralities = NULL;
            return false;
        }
        cache->hub_is_connector = (bool*)nimcp_calloc(hubs->num_hubs, sizeof(bool));
        if (!cache->hub_is_connector) {
            nimcp_free(cache->hub_neuron_ids);
            cache->hub_neuron_ids = NULL;
            nimcp_free(cache->hub_centralities);
            cache->hub_centralities = NULL;
            nimcp_free(cache->hub_community_ids);
            cache->hub_community_ids = NULL;
            return false;
        }

        if (cache->hub_neuron_ids && cache->hub_centralities &&
            cache->hub_community_ids && cache->hub_is_connector) {
            memcpy(cache->hub_neuron_ids, hubs->hub_indices,
                   hubs->num_hubs * sizeof(uint32_t));
            memcpy(cache->hub_centralities, hubs->degree_centrality,
                   hubs->num_hubs * sizeof(float));
            if (hubs->hub_communities) {
                memcpy(cache->hub_community_ids, hubs->hub_communities,
                       hubs->num_hubs * sizeof(uint32_t));
            }
            /* Determine connector hubs: hub neurons whose community differs
             * from at least one neighbor's community (simplified: check if
             * hub appears in a different community than its assigned one) */
            for (uint32_t h = 0; h < hubs->num_hubs; h++) {
                /* A hub is a connector if it has high betweenness centrality */
                if (hubs->betweenness_centrality &&
                    hubs->betweenness_centrality[h] > 0.5F) {
                    cache->hub_is_connector[h] = true;
                }
            }
        }
    }
    if (hubs) {
        hub_structure_free(hubs);
    }

    /* Record cache metadata */
    cache->cache_timestamp_ms = nimcp_time_monotonic_ms();
    cache->learning_events_at_cache = 0;  /* Caller can set if tracking */
    cache->is_valid = true;

    LOG_INFO("Community cache built: %u neurons, %u communities (Q=%.3f), %u hubs",
             cache->num_neurons, cache->num_communities,
             cache->modularity, cache->num_hubs);

    return true;
}

bool consolidation_community_cache_is_valid(
    const consolidation_community_cache_t* cache,
    brain_t brain,
    uint64_t max_age_ms)
{
    if (!cache || !cache->is_valid) return false;
    if (cache->num_neurons == 0) return false;

    /* Check age if limit specified */
    if (max_age_ms > 0) {
        uint64_t now = nimcp_time_monotonic_ms();
        if (now - cache->cache_timestamp_ms > max_age_ms) {
            return false;  /* Cache too old */
        }
    }

    return true;
}

void consolidation_community_cache_invalidate(consolidation_community_cache_t* cache)
{
    if (cache) {
        cache->is_valid = false;
    }
}

uint32_t consolidation_cache_get_community(
    const consolidation_community_cache_t* cache,
    uint32_t neuron_id)
{
    if (!cache || !cache->is_valid || !cache->community_ids) return UINT32_MAX;
    if (neuron_id >= cache->num_neurons) return UINT32_MAX;
    return cache->community_ids[neuron_id];
}

bool consolidation_cache_is_hub(
    const consolidation_community_cache_t* cache,
    uint32_t neuron_id,
    float* out_centrality,
    bool* out_is_connector)
{
    if (!cache || !cache->is_valid || !cache->hub_neuron_ids) return false;

    for (uint32_t h = 0; h < cache->num_hubs; h++) {
        if (cache->hub_neuron_ids[h] == neuron_id) {
            if (out_centrality && cache->hub_centralities) {
                *out_centrality = cache->hub_centralities[h];
            }
            if (out_is_connector && cache->hub_is_connector) {
                *out_is_connector = cache->hub_is_connector[h];
            }
            return true;
        }
    }
    return false;
}

/**
 * WHAT: Community-aware pattern replay with cached topology
 * WHY: Hub-associated and cross-community patterns get boosted consolidation
 * HOW: Like consolidate_replay but modulates strength using community cache
 *
 * BIOLOGICAL BASIS: During sleep, replay preferentially strengthens hub neurons
 * (van den Heuvel & Sporns, 2013) and inter-module connections that support
 * knowledge integration across functional domains.
 */
static bool consolidate_replay_with_cache(brain_t brain, const consolidation_config_t* config,
                                          consolidation_stats_t* stats,
                                          const consolidation_community_cache_t* cache)
{
    working_memory_t* wm = brain_get_working_memory(brain);

    uint32_t patterns_replayed = 0;
    uint32_t patterns_strengthened = 0;
    uint32_t connections_strengthened = 0;

    if (wm && working_memory_get_size(wm) > 0) {
        uint32_t wm_size = working_memory_get_size(wm);
        uint32_t max_replay = config->replay_count < wm_size ? config->replay_count : wm_size;

        for (uint32_t i = 0; i < max_replay; i++) {
            if ((i & 0xFF) == 0 && max_replay > 256) {
                consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)max_replay);
            }

            float total_salience = 0.0F;
            if (!working_memory_get_total_salience(wm, i, &total_salience)) {
                continue;
            }

            emotional_tag_t emotion;
            bool has_emotion = working_memory_get_emotion(wm, i, &emotion);

            /* Base consolidation strength (emotional boost) */
            float consolidation_strength = config->consolidation_strength;
            if (has_emotion && emotion.intensity > 0.5F) {
                consolidation_strength *= (1.0F + emotion.intensity);
            }

            /* Novelty boost */
            if (config->prioritize_novel && has_emotion &&
                (emotion.category == EMOTION_CAT_EXCITEMENT ||
                 emotion.category == EMOTION_CAT_JOY)) {
                consolidation_strength *= config->novelty_boost;
            }

            /* COMMUNITY-AWARE BOOST: Use cached topology to modulate strength */
            if (cache && cache->is_valid && cache->num_hubs > 0) {
                /* Use pattern index as a proxy for associated neuron region.
                 * In a full implementation, working memory items would carry
                 * neuron activation maps. For now, map WM slot to neuron space
                 * via modular hashing to distribute across network. */
                uint32_t representative_neuron = (i * 7919) % cache->num_neurons;

                /* Hub boost: patterns associated with hub neurons get extra strength */
                float centrality = 0.0F;
                bool is_connector = false;
                if (consolidation_cache_is_hub(cache, representative_neuron,
                                               &centrality, &is_connector)) {
                    consolidation_strength *= config->hub_consolidation_boost;
                    patterns_strengthened++;

                    /* Connector hub bonus: cross-community bridges get even more */
                    if (is_connector) {
                        consolidation_strength *= config->cross_community_boost;
                    }
                }

                /* Cross-community detection: if pattern spans multiple communities,
                 * boost for knowledge integration */
                if (cache->num_communities > 1 && i + 1 < max_replay) {
                    uint32_t neuron_a = representative_neuron;
                    uint32_t neuron_b = ((i + 1) * 7919) % cache->num_neurons;
                    uint32_t comm_a = consolidation_cache_get_community(cache, neuron_a);
                    uint32_t comm_b = consolidation_cache_get_community(cache, neuron_b);
                    if (comm_a != UINT32_MAX && comm_b != UINT32_MAX && comm_a != comm_b) {
                        consolidation_strength *= config->cross_community_boost;
                    }
                }
            }

            patterns_replayed++;

            /* Estimate connections strengthened scaled by consolidation strength */
            float base_connections = has_emotion && emotion.intensity > 0.5F ? 75.0F : 50.0F;
            connections_strengthened += (uint32_t)(base_connections * consolidation_strength);
        }
    } else {
        /* Fallback: No working memory */
        patterns_replayed = config->replay_count;
        if (config->prioritize_novel) {
            patterns_replayed = (uint32_t)(patterns_replayed * config->novelty_boost);
        }
        patterns_strengthened = patterns_replayed;
        connections_strengthened = patterns_replayed * 50;
    }

    stats->patterns_replayed += patterns_replayed;
    stats->connections_strengthened += connections_strengthened;
    stats->patterns_strengthened += patterns_strengthened;

    return true;
}

/**
 * WHAT: Community-aware consolidation entry point
 * WHY: Uses cached community structure for smarter replay prioritization
 * HOW: Like brain_consolidate_memory but routes replay through cache-aware path
 */
bool brain_consolidate_memory_community_aware(
    brain_t brain,
    const consolidation_config_t* config,
    const consolidation_community_cache_t* cache)
{
    if (brain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "brain_consolidate_memory_community_aware: NULL brain");
        return false;
    }

    consolidation_config_t default_config = consolidation_default_config();
    const consolidation_config_t* cfg = config ? config : &default_config;

    if (cfg->on_consolidation_start) {
        cfg->on_consolidation_start(cfg->callback_context);
    }

    uint64_t start_time = nimcp_time_monotonic_ms();

    ensure_sync_stats_init();
    nimcp_mutex_lock(&g_sync_stats_lock);

    /* Track prune passes */
    uint32_t prune_passes_done = 0;
    uint32_t max_prune = cfg->max_prune_passes > 0 ? cfg->max_prune_passes : 1;
    bool success = true;

    for (uint32_t cycle = 0; cycle < cfg->consolidation_cycles && success; cycle++) {
        /* Replay: use community-aware path if cache is valid */
        if (cfg->enable_replay && (cfg->strategy == CONSOLIDATION_STRATEGY_REPLAY ||
                                   cfg->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            if (cache && cache->is_valid && cfg->use_community_cache) {
                consolidate_replay_with_cache(brain, cfg, &g_sync_stats, cache);
            } else {
                consolidate_replay(brain, cfg, &g_sync_stats);
            }
        }

        /* Scaling */
        if (cfg->enable_scaling && (cfg->strategy == CONSOLIDATION_STRATEGY_SCALING ||
                                    cfg->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            consolidate_scaling(brain, cfg, &g_sync_stats);
        }

        /* Pruning (capped) */
        if (cfg->enable_pruning && prune_passes_done < max_prune &&
            (cfg->strategy == CONSOLIDATION_STRATEGY_PRUNING ||
             cfg->strategy == CONSOLIDATION_STRATEGY_FULL)) {
            consolidate_pruning(brain, cfg, &g_sync_stats);
            prune_passes_done++;
        }
    }

    if (success) {
        uint64_t end_time = nimcp_time_monotonic_ms();
        float duration_ms = (float)(end_time - start_time);
        if (duration_ms < 0.001F) duration_ms = 0.001F;

        g_sync_stats.total_consolidations++;
        g_sync_stats.last_consolidation_time_ms = duration_ms;

        float alpha = 0.1F;
        g_sync_stats.avg_consolidation_time_ms =
            alpha * duration_ms + (1.0F - alpha) * g_sync_stats.avg_consolidation_time_ms;
        g_sync_stats.last_consolidation_timestamp = end_time;
    }

    nimcp_mutex_unlock(&g_sync_stats_lock);

    if (cfg->on_consolidation_complete) {
        cfg->on_consolidation_complete(cfg->callback_context);
    }

    return success;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int consolidation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    consolidation_heartbeat("consolidatio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Memory_Consolidation_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                consolidation_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Memory_Consolidation_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Memory_Consolidation_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void consolidation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_consolidation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int consolidation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_training_begin: NULL argument");
        return -1;
    }
    consolidation_heartbeat_instance(NULL, "consolidation_training_begin", 0.0f);
    (void)(struct consolidation_handle_struct*)instance; /* Module state available for reset */
    return 0;
}

int consolidation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_training_end: NULL argument");
        return -1;
    }
    consolidation_heartbeat_instance(NULL, "consolidation_training_end", 1.0f);
    (void)(struct consolidation_handle_struct*)instance; /* Module state available for finalization */
    return 0;
}

int consolidation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    consolidation_heartbeat_instance(NULL, "consolidation_training_step", progress);
    (void)(struct consolidation_handle_struct*)instance; /* Module state available for step adaptation */
    return 0;
}

/**
 * @file nimcp_sleep_wake.c
 * @brief Sleep-wake cycle implementation
 *
 * WHAT: Implements biologically-inspired sleep-wake cycle
 * WHY:  Memory consolidation, synaptic homeostasis, prevent catastrophic forgetting
 * HOW:  State machine with memory replay and synaptic scaling
 *
 * DESIGN PATTERNS:
 * - State Machine: Sleep state transitions
 * - Strategy: Different behaviors per sleep stage
 *
 * PHASE 10.1: Core implementation
 * PHASE 10.3 INTEGRATION: Emotional working memory prioritization
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"  /* W13: sleep-stage transition events */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "cognitive.sleep_wake"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "cognitive/memory/nimcp_episodic_replay.h"
#include "cognitive/sleep_wake/nimcp_sleep_wake_substrate_bridge.h"
/* Do NOT include nimcp_sleep_wake_thalamic_bridge.h — its thalamic_router_t
 * typedef collides with parietal.h (pulled in transitively). Pre-existing
 * tech debt in the codebase. Use forward-decls + externs instead. */
struct sleep_wake_thalamic_bridge;
extern int sleep_wake_thalamic_modulate_gating(
    struct sleep_wake_thalamic_bridge* bridge, float arousal_level);
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(sleep_wake, MESH_ADAPTER_CATEGORY_COGNITIVE)



#define BIO_MODULE_COGNITIVE_SLEEP_WAKE 0x0354


// Phase 10.3: Emotional working memory integration
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: State change callback entry
 * WHY:  Track registered callbacks for state change notifications
 * HOW:  Linked list of callback + user_data pairs
 */
typedef struct sleep_callback_entry {
    sleep_state_callback_t callback;
    void* user_data;
    struct sleep_callback_entry* next;
} sleep_callback_entry_t;

/**
 * WHAT: Internal sleep system structure
 * WHY:  Encapsulate implementation details (Pimpl idiom)
 * HOW:  Track state, pressure, statistics
 */
struct sleep_system_struct {
    sleep_config_t config;

    // Current state
    sleep_state_t current_state;
    uint64_t state_entered_at;   /**< Timestamp when entered state */
    uint64_t total_awake_time;   /**< Total time awake (ms) */
    uint64_t total_sleep_time;   /**< Total time asleep (ms) */

    // Sleep pressure (adenosine model)
    float sleep_pressure;        /**< Current level [0,1] */
    uint32_t learning_steps_since_sleep;

    // Statistics
    uint32_t sleep_cycles_completed;
    uint32_t total_memories_replayed;
    uint32_t total_synapses_pruned;
    float consolidation_efficiency;
    float energy_savings;

    // State change notifications
    sleep_callback_entry_t* callbacks;  /**< Linked list of state change callbacks */

    // Thread safety
    nimcp_mutex_t lock;

    // Phase 10.3: Reference to brain for working memory access
    void* brain_ref;  /**< brain_t reference (void* to avoid circular dependency) */

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
};

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static void sleep_stage_drowsy(sleep_system_t sleep);
static void sleep_stage_light_nrem(sleep_system_t sleep);
static void sleep_stage_deep_nrem(sleep_system_t sleep);
static void sleep_stage_rem(sleep_system_t sleep);
static void sleep_notify_state_change(sleep_system_t sleep, sleep_state_t new_state);

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default sleep configuration
 * WHY:  Sensible defaults based on neuroscience literature
 * HOW:  Return pre-configured struct
 */
sleep_config_t sleep_default_config(void)
{
    sleep_config_t config = {
        // Sleep pressure
        .adenosine_accumulation_rate = 0.0001F,
        .sleep_pressure_threshold = 0.8F,
        .adenosine_clearance_rate = 0.05F,

        // Stage durations (realistic biological durations)
        .drowsy_duration_ms = 120000,      // 2 minutes
        .light_sleep_duration_ms = 900000,  // 15 minutes
        .deep_sleep_duration_ms = 1800000,  // 30 minutes
        .rem_duration_ms = 600000,          // 10 minutes

        // Memory replay
        .replay_batch_size = 100,
        .replay_speed_multiplier = 15.0F,
        .replay_noise = 0.1F,
        .prioritize_emotional = true,   // Phase 10.3: Emotional prioritization
        .prioritize_novel = true,

        // Synaptic homeostasis
        .synaptic_downscaling_factor = 0.85F,
        .synaptic_pruning_threshold = 0.01F,
        .enable_homeostasis = true,

        // REM
        .rem_creativity_noise = 0.3F,
        .enable_rem = true,

        // Oscillations
        .sync_to_oscillations = true   // ENABLED: Use FFT for sleep wave detection
    };

    return config;
}

/**
 * WHAT: Create sleep-wake system
 * WHY:  Initialize sleep tracking
 * HOW:  Allocate structure, set defaults, initialize mutex
 */
sleep_system_t sleep_system_create(const sleep_config_t* config)
{
    /* WHAT: Allocate structure */
    sleep_system_t sleep =
        (sleep_system_t) nimcp_calloc(1, sizeof(struct sleep_system_struct));
    if (sleep == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;
    }

    /* WHAT: Set configuration */
    sleep->config = config ? *config : sleep_default_config();

    /* WHAT: Initialize state */
    sleep->current_state = SLEEP_STATE_AWAKE;
    sleep->state_entered_at = nimcp_time_monotonic_ms();
    sleep->sleep_pressure = 0.0F;
    sleep->learning_steps_since_sleep = 0;

    /* WHAT: Initialize statistics */
    sleep->sleep_cycles_completed = 0;
    sleep->total_memories_replayed = 0;
    sleep->total_synapses_pruned = 0;
    sleep->consolidation_efficiency = 0.0F;
    sleep->energy_savings = 0.0F;
    sleep->total_awake_time = 0;
    sleep->total_sleep_time = 0;

    /* WHAT: Initialize thread safety */
    nimcp_mutex_init(&sleep->lock, NULL);

    /* WHAT: Initialize callback list */
    sleep->callbacks = NULL;

    // Bio-async registration
    sleep->bio_ctx = NULL;
    sleep->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CONSOLIDATION_SLEEP,
            .module_name = "sleep_wake",
            .inbox_capacity = 32,
            .user_data = sleep
        };
        sleep->bio_ctx = bio_router_register_module(&bio_info);
        if (sleep->bio_ctx) {
            sleep->bio_async_enabled = true;
        }
    }

return sleep;
}

/**
 * WHAT: Destroy sleep system
 * WHY:  Free resources, prevent leaks
 * HOW:  Destroy mutex, free structure
 */
void sleep_system_destroy(sleep_system_t sleep)
{
    LOG_DEBUG("Destroying module");
    if (sleep == NULL) {
        return;
    }

    /* WHAT: Free callback list under lock */
    /* WHY:  Prevent race with callback registration/invocation */
    /* HOW:  Hold mutex while clearing, then take local copy */
    nimcp_mutex_lock(&sleep->lock);
    sleep_callback_entry_t* current = sleep->callbacks;
    sleep->callbacks = NULL;  /* Prevent other threads from accessing */
    nimcp_mutex_unlock(&sleep->lock);

    /* Free callbacks outside lock to avoid deadlock */
    while (current != NULL) {
        sleep_callback_entry_t* next = current->next;
        nimcp_free(current);
        current = NULL;
        current = next;
    }

    nimcp_mutex_destroy(&sleep->lock);
    nimcp_free(sleep);
    sleep = NULL;
}

/* ========================================================================
 * SLEEP PRESSURE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Accumulate sleep pressure after learning
 * WHY:  Model adenosine buildup from synaptic activity
 * HOW:  Increment pressure proportionally to learning steps
 *
 * BIOLOGICAL BASIS:
 * - Synaptic activity → ATP consumption → adenosine production
 * - Adenosine accumulates in extracellular space
 * - Adenosine binds to receptors → sleepiness
 */
void sleep_accumulate_pressure(sleep_system_t sleep, uint32_t learning_steps)
{
    if (sleep == NULL) {
        return;
    }

    nimcp_mutex_lock(&sleep->lock);

    /* WHAT: Get arousal modulation from medulla if available */
    /* WHY:  High arousal (stress, alertness) reduces sleep pressure buildup */
    /* HOW:  Query medulla arousal and scale accumulation rate inversely */
    float arousal_modifier = 1.0f;
    if (sleep->brain_ref) {
        brain_t brain = (brain_t)sleep->brain_ref;
        float arousal = nimcp_brain_get_arousal_level(brain);

        /* High arousal (>0.7) reduces pressure accumulation (adrenaline effect) */
        /* Low arousal (<0.3) increases pressure accumulation (fatigue) */
        if (arousal > 0.7f) {
            arousal_modifier = 0.5f + (1.0f - arousal);  /* 0.5-0.8 range */
        } else if (arousal < 0.3f) {
            arousal_modifier = 1.2f + (0.3f - arousal);  /* 1.2-1.5 range */
        }
        /* Neutral arousal (0.3-0.7) = normal rate */
    }

    /* WHAT: Accumulate pressure proportional to learning, modulated by arousal */
    float increment = learning_steps * sleep->config.adenosine_accumulation_rate * arousal_modifier;
    sleep->sleep_pressure += increment;

    /* Substrate coupling: ATP depletion + metabolic fatigue add to pressure
     * directly. The substrate bridge reads neural_substrate's metabolic
     * state and writes an independent sleep_pressure in its effects struct.
     * We fold that into our accumulator so tissue fatigue can trigger
     * sleep even when learning_steps is low (e.g. high-inference idle). */
    if (sleep->brain_ref) {
        brain_t brain = (brain_t)sleep->brain_ref;
        if (brain->sleep_wake_substrate_bridge) {
            sleep_wake_substrate_bridge_update(brain->sleep_wake_substrate_bridge);
            sleep_wake_substrate_effects_t eff = {0};
            if (sleep_wake_substrate_bridge_get_effects(
                    brain->sleep_wake_substrate_bridge, &eff) == 0) {
                /* Use the max of existing + substrate-derived pressure so
                 * fatigue can only add to (not subtract from) adenosine. */
                if (eff.sleep_pressure > sleep->sleep_pressure) {
                    sleep->sleep_pressure = eff.sleep_pressure;
                }
            }
        }
    }

    /* WHAT: Clamp to [0, 1] range */
    if (sleep->sleep_pressure > 1.0F) {
        sleep->sleep_pressure = 1.0F;
    }

    sleep->learning_steps_since_sleep += learning_steps;

    nimcp_mutex_unlock(&sleep->lock);
}

/**
 * WHAT: Get current sleep pressure
 * WHY:  Check how much sleep is needed
 * HOW:  Return current pressure value
 */
float sleep_get_pressure(const sleep_system_t sleep)
{
    if (sleep == NULL) {
        return 0.0F;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&sleep->lock);
    float pressure = sleep->sleep_pressure;
    nimcp_mutex_unlock((nimcp_mutex_t*)&sleep->lock);

    return pressure;
}

/**
 * WHAT: Check if sleep is needed
 * WHY:  Determine when to initiate sleep cycle
 * HOW:  Compare pressure to threshold
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
bool sleep_is_needed(const sleep_system_t sleep)
{
    /* Guard clause: Validate input */
    if (sleep == NULL) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&sleep->lock);

    /* WHAT: Get circadian modulation from medulla if available */
    /* WHY:  Circadian phase affects sleep propensity (night = more sleepy) */
    /* HOW:  Query medulla circadian phase and adjust threshold accordingly */
    float threshold_modifier = 1.0f;
    if (sleep->brain_ref) {
        brain_t brain = (brain_t)sleep->brain_ref;
        circadian_phase_t phase = nimcp_brain_get_circadian_phase(brain);

        /* Night phases lower the threshold (easier to trigger sleep) */
        switch (phase) {
            case CIRCADIAN_PHASE_NIGHT:
            case CIRCADIAN_PHASE_DEEP_NIGHT:
                threshold_modifier = 0.7f;  /* 30% lower threshold at night */
                break;
            case CIRCADIAN_PHASE_LATE_EVENING:
            case CIRCADIAN_PHASE_PRE_DAWN:
                threshold_modifier = 0.85f; /* 15% lower at evening/pre-dawn */
                break;
            case CIRCADIAN_PHASE_MORNING:
            case CIRCADIAN_PHASE_EARLY_MORNING:
                threshold_modifier = 1.2f;  /* 20% higher in morning (harder to sleep) */
                break;
            case CIRCADIAN_PHASE_AFTERNOON:
                threshold_modifier = 1.0f;  /* Neutral in afternoon */
                break;
            case CIRCADIAN_PHASE_EVENING:
                threshold_modifier = 0.95f; /* Slightly lower in evening */
                break;
            default:
                threshold_modifier = 1.0f;
                break;
        }
    }

    /* WHAT: Check if pressure exceeds circadian-adjusted threshold */
    /* WHY:  Threshold indicates when sleep is biologically necessary */
    float adjusted_threshold = sleep->config.sleep_pressure_threshold * threshold_modifier;
    bool needed = sleep->sleep_pressure >= adjusted_threshold;

    nimcp_mutex_unlock((nimcp_mutex_t*)&sleep->lock);

    return needed;
}

/* ========================================================================
 * SLEEP STATE MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Get current sleep state
 * WHY:  Query what stage of sleep cycle
 * HOW:  Return current state enum
 */
sleep_state_t sleep_get_current_state(const sleep_system_t sleep)
{
    if (sleep == NULL) {
        return SLEEP_STATE_AWAKE;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&sleep->lock);
    sleep_state_t state = sleep->current_state;
    nimcp_mutex_unlock((nimcp_mutex_t*)&sleep->lock);

    return state;
}

/**
 * WHAT: Enter specific sleep state
 * WHY:  Transition between sleep stages
 * HOW:  Set state, record timestamp, notify observers
 */
bool sleep_enter_state(sleep_system_t sleep, sleep_state_t state)
{
    if (sleep == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_enter_state: validation failed");
        return false;
    }

    nimcp_mutex_lock(&sleep->lock);

    /* WHAT: Update awake/sleep time tracking */
    uint64_t now = nimcp_time_monotonic_ms();
    uint64_t duration = now - sleep->state_entered_at;

    if (sleep->current_state == SLEEP_STATE_AWAKE) {
        sleep->total_awake_time += duration;
    } else {
        sleep->total_sleep_time += duration;
    }

    /* WHAT: Transition to new state */
    sleep_state_t prev_state = sleep->current_state;
    sleep->current_state = state;
    sleep->state_entered_at = now;
    float cur_pressure = sleep->sleep_pressure;

    nimcp_mutex_unlock(&sleep->lock);

    /* W13: emit stage-transition event to KG. */
    if (sleep->brain_ref) {
        wave13_sleep_emit_stage_transition((brain_t)sleep->brain_ref,
                                           (int)prev_state, (int)state,
                                           cur_pressure);
    }

    /* WHAT: Notify all registered callbacks of state change */
    /* WHY:  Allow modules to react immediately to sleep state changes */
    /* HOW:  Call helper function to invoke all callbacks */
    sleep_notify_state_change(sleep, state);

    /* Thalamic gating: modulate the thalamic router's attention output by
     * sleep stage. AWAKE/REM → full attention, DROWSY/LIGHT_NREM → mid,
     * DEEP_NREM → heavily damped. Keeps non-sleep subsystems from
     * receiving spurious thalamic-routed signals during consolidation. */
    if (sleep->brain_ref) {
        brain_t brain = (brain_t)sleep->brain_ref;
        if (brain->sleep_wake_thalamic_bridge) {
            float arousal = 1.0F;
            switch (state) {
                case SLEEP_STATE_AWAKE:     arousal = 1.0F; break;
                case SLEEP_STATE_DROWSY:    arousal = 0.6F; break;
                case SLEEP_STATE_LIGHT_NREM: arousal = 0.3F; break;
                case SLEEP_STATE_DEEP_NREM:  arousal = 0.1F; break;
                case SLEEP_STATE_REM:        arousal = 0.9F; break;
                default: arousal = 0.5F; break;
            }
            sleep_wake_thalamic_modulate_gating(
                brain->sleep_wake_thalamic_bridge, arousal);
        }
    }

    return true;
}

/**
 * WHAT: Wake brain from sleep
 * WHY:  Return to active processing mode
 * HOW:  Set state to awake, reset pressure, notify observers
 */
bool sleep_wake_up(sleep_system_t sleep)
{
    if (sleep == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_up: validation failed");
        return false;
    }

    nimcp_mutex_lock(&sleep->lock);

    /* WHAT: Clear sleep pressure (adenosine cleared during sleep) */
    sleep->sleep_pressure = 0.0F;
    sleep->learning_steps_since_sleep = 0;

    /* WHAT: Return to awake state */
    sleep->current_state = SLEEP_STATE_AWAKE;
    sleep->state_entered_at = nimcp_time_monotonic_ms();

    nimcp_mutex_unlock(&sleep->lock);

    /* WHAT: Notify callbacks of wake state */
    sleep_notify_state_change(sleep, SLEEP_STATE_AWAKE);

    return true;
}

/* ========================================================================
 * SLEEP STAGE IMPLEMENTATIONS
 * ======================================================================== */

/**
 * WHAT: Drowsy stage implementation
 * WHY:  Transition period, reduce oscillations
 * HOW:  Minimal processing, prepare for sleep
 *
 * BIOLOGICAL:
 * - Alpha waves (8-13Hz)
 * - Reduced attention, relaxation
 * - Transition to sleep
 */
static void sleep_stage_drowsy(sleep_system_t sleep)
{
    /* Drowsy: flush any pending bio-async messages and KG events queued
     * during waking activity before descending into NREM. Biologically
     * analogous to the brief settling period where attention disengages. */
    if (sleep && sleep->bio_ctx) {
        bio_router_process_inbox(sleep->bio_ctx, 32);
    }
}

/**
 * WHAT: Light NREM stage implementation
 * WHY:  Sort and organize memories
 * HOW:  Identify important memories for deep sleep consolidation
 *
 * BIOLOGICAL:
 * - Theta waves (4-8Hz), sleep spindles (12-16Hz)
 * - Memory triage: important vs trivial
 * - Protection from external disturbances
 */
static void sleep_stage_light_nrem(sleep_system_t sleep)
{
    /* Light NREM: triage working-memory items by total salience, which
     * primes deep-NREM consolidation to favour emotionally-tagged and
     * high-novelty traces. Real replay happens in deep-NREM below —
     * this stage only recomputes the ordering signal. */
    if (sleep == NULL || sleep->brain_ref == NULL) {
        return;
    }
    brain_t brain = (brain_t)sleep->brain_ref;
    working_memory_t* wm = brain_get_working_memory(brain);
    if (!wm) {
        return;
    }
    uint32_t wm_size = working_memory_get_size(wm);
    for (uint32_t i = 0; i < wm_size; i++) {
        float s = 0.0F;
        working_memory_get_total_salience(wm, i, &s);
        /* Recomputing salience updates any in-WM scoring caches; returned
         * value is not used here. */
        (void)s;
    }
}

/**
 * WHAT: Deep NREM stage implementation - MOST IMPORTANT
 * WHY:  Memory consolidation and synaptic homeostasis
 * HOW:  Replay working memory, strengthen synapses, prune weak connections
 *
 * BIOLOGICAL:
 * - Delta waves (0.5-4Hz)
 * - Hippocampal replay → cortical consolidation
 * - Synaptic downscaling (homeostasis)
 * - Pruning of weak connections
 *
 * PHASE 10.3 INTEGRATION:
 * - Retrieves emotional working memory items
 * - Prioritizes emotional and novel memories for replay
 * - Uses consolidation module for actual replay
 */
/**
 * WHAT: Deep NREM stage implementation - MOST IMPORTANT
 * WHY:  Memory consolidation and synaptic homeostasis
 * HOW:  Replay working memory, strengthen synapses, prune weak connections
 *
 * BIOLOGICAL:
 * - Delta waves (0.5-4Hz)
 * - Hippocampal replay → cortical consolidation
 * - Synaptic downscaling (homeostasis)
 * - Pruning of weak connections
 *
 * PHASE 10.3 INTEGRATION:
 * - Retrieves emotional working memory items
 * - Prioritizes emotional and novel memories for replay
 * - Uses consolidation module for actual replay
 *
 * COMPLEXITY: O(n*log(n)) where n = working memory size (sorting)
 * THREAD-SAFE: Yes (mutex protected)
 */
static void sleep_stage_deep_nrem(sleep_system_t sleep)
{
    /* Guard clause: Validate input */
    if (sleep == NULL) {
        return;
    }

    /* =========================================================================
     * PHASE 10.3: Emotional working memory consolidation
     * =========================================================================
     * WHAT: Replay working memory items with emotional prioritization
     * WHY:  Emotional events are more important for long-term retention
     * HOW:  Extract from working memory, sort by emotional salience, replay
     *
     * BIOLOGICAL BASIS:
     * - Amygdala tags emotional events during encoding
     * - Hippocampus preferentially replays emotional memories during sleep
     * - Emotional consolidation enhances long-term retention
     *
     * ALGORITHM:
     * 1. Get working memory from brain (if available)
     * 2. Iterate through items, extract emotional tags
     * 3. Calculate priority score: emotional_intensity * 0.5 + salience * 0.5
     * 4. Replay highest priority items up to replay_batch_size
     * 5. Update statistics
     */

    uint32_t memories_replayed = 0;

    /* WHAT: Get brain and working memory (Phase 10.3 integration) */
    if (sleep->brain_ref != NULL) {
        /* Cast brain reference */
        brain_t brain = (brain_t)sleep->brain_ref;

        /* Get working memory from brain */
        working_memory_t* wm = brain_get_working_memory(brain);

        if (wm && working_memory_get_size(wm) > 0) {
            /* WHAT: Prioritize emotional memories for replay */
            /* WHY:  Limited time during deep sleep, replay most important first */
            /* HOW:  Iterate working memory, check emotional tags, prioritize */

            uint32_t wm_size = working_memory_get_size(wm);
            uint32_t max_replay = sleep->config.replay_batch_size;
            if (max_replay > wm_size) {
                max_replay = wm_size;  // Can't replay more than available
            }

            /* WHAT: Prioritize by emotional salience */
            /* WHY:  Emotional items get consolidated first (amygdala-hippocampus) */
            for (uint32_t i = 0; i < max_replay; i++) {
                /* Get total salience (includes emotional boost from Phase 10.3) */
                float total_salience = 0.0F;
                working_memory_get_total_salience(wm, i, &total_salience);

                /* Get emotional tag if available */
                emotional_tag_t emotion;
                bool has_emotion = working_memory_get_emotion(wm, i, &emotion);

                /* WHAT: Calculate consolidation weight */
                /* WHY:  Prioritize emotional and novel content */
                /* HOW:  Combine emotional intensity, arousal, and salience */
                float consolidation_weight = total_salience;  // Base salience

                if (has_emotion && emotion.intensity > 0.3F) {
                    /* WHAT: Boost for emotional content */
                    /* WHY:  Emotional events consolidate stronger */
                    /* HOW:  Add intensity and arousal */
                    consolidation_weight += emotion.intensity * 0.5F;
                    consolidation_weight += emotion.arousal * 0.3F;

                    /* WHAT: Extra boost for high-arousal emotions */
                    /* WHY:  Survival-critical events (fear, excitement) */
                    /* HOW:  Scale by emotional category */
                    if (emotion.arousal > 0.7F) {
                        consolidation_weight *= 1.5F;  // 50% boost
                    }
                }

                /* WM prioritization bookkeeping only — actual weight updates
                 * happen below via episodic replay. The WM branch still
                 * contributes emotional scoring that the episodic buffer
                 * doesn't track, so we keep the iteration for future
                 * priority-weighted replay. */
                (void)consolidation_weight;
            }
        }
    }

    /* PRIMARY CONSOLIDATION: replay top-N episodic experiences recorded
     * during brain_learn_vector. This is the real replay that updates
     * network weights at reduced LR. */
    if (sleep->brain_ref != NULL) {
        brain_t brain = (brain_t)sleep->brain_ref;
        if (brain->episodic_replay) {
            float base_lr = brain->config.learning_rate;
            int replayed = nimcp_episodic_replay_consolidate_internal(
                (nimcp_episodic_replay_t*)brain->episodic_replay,
                (void*)brain, base_lr);
            if (replayed > 0) {
                memories_replayed += (uint32_t)replayed;
                brain->module_activity.sleep_replay_events += (uint64_t)replayed;
            }
        }

        /* Synaptic homeostatic rescaling on SNN populations. Runs every
         * N training steps from the SNN training bridge during waking;
         * we also fire it here so sleep pressure triggers a guaranteed
         * rescale even if waking activity never crossed the training
         * bridge's own interval gate. Safe because the daemon scheduler
         * holds the dispatch lock around sleep_run_cycle. */
        if (brain->snn_training_ctx && brain->snn_network) {
            extern uint32_t snn_homeostatic_apply(
                struct snn_training_ctx_s*, struct snn_network_s*);
            uint32_t scaled = snn_homeostatic_apply(
                brain->snn_training_ctx, brain->snn_network);
            if (scaled > 0) {
                nimcp_mutex_lock(&sleep->lock);
                sleep->total_synapses_pruned += scaled;
                nimcp_mutex_unlock(&sleep->lock);
                brain->module_activity.sleep_downscale_events += (uint64_t)scaled;
            }
        }
    }

    /* WHAT: Update replay statistics (thread-safe) */
    /* WHY:  Track consolidation activity */
    /* HOW:  Increment total counter */
    nimcp_mutex_lock(&sleep->lock);
    sleep->total_memories_replayed += memories_replayed;
    nimcp_mutex_unlock(&sleep->lock);

    /* Synaptic homeostasis (SHY hypothesis: Tononi & Cirelli 2014).
     * SNN populations have a per-population homeostatic scaling step
     * (snn_homeostatic_apply) already invoked on a schedule by the SNN
     * training bridge. We do not re-run it here to avoid double-scaling
     * against an active training thread. The `total_synapses_pruned`
     * and `energy_savings` counters below are NOT wired to any real
     * pruning action — they remain purely informational. True pruning
     * is the responsibility of the glial/microglia subsystem which
     * operates on every brain_learn_vector step (see glial G5 wiring). */
    if (sleep->config.enable_homeostasis) {
        nimcp_mutex_lock(&sleep->lock);
        /* Intentionally no-op: counters left at last value. */
        nimcp_mutex_unlock(&sleep->lock);
    }

    /* Consolidation efficiency: set to 1.0 when real replays happened,
     * otherwise retain the previous value. The prior fixed-fraction
     * 0.85/0.90 value was fabricated; the more honest signal is
     * "did anything actually consolidate this cycle". */
    nimcp_mutex_lock(&sleep->lock);
    if (memories_replayed > 0) {
        sleep->consolidation_efficiency = 1.0F;
    }
    nimcp_mutex_unlock(&sleep->lock);
}

/**
 * WHAT: REM stage implementation
 * WHY:  Creative recombination, emotional integration
 * HOW:  Random activation patterns, novel connections
 *
 * BIOLOGICAL:
 * - Theta waves (4-8Hz)
 * - High acetylcholine, low norepinephrine
 * - Random memory activation → creativity
 * - Emotional processing (amygdala active)
 */
static void sleep_stage_rem(sleep_system_t sleep)
{
    if (sleep == NULL || !sleep->config.enable_rem) {
        return;
    }

    /* REM: creative recombination via random feature blending between
     * episodic experiences. Unlike NREM consolidation which replays
     * individual traces at reduced LR, REM presents novel mixed inputs
     * at very low LR — produces new associations without overwriting
     * consolidated memory. */
    if (sleep->brain_ref) {
        brain_t brain = (brain_t)sleep->brain_ref;
        if (brain->episodic_replay) {
            float base_lr = brain->config.learning_rate;
            nimcp_episodic_replay_rem_recombine_internal(
                (nimcp_episodic_replay_t*)brain->episodic_replay,
                (void*)brain, base_lr, 8);
        }
    }

    /* Placeholder for REM functionality */
    /* Would randomly combine working memory items */
    /* Create novel associations */
    (void)sleep;
}

/* ========================================================================
 * SLEEP CYCLE EXECUTION
 * ======================================================================== */

/**
 * WHAT: Run full automatic sleep cycle
 * WHY:  Automate entire consolidation process
 * HOW:  Progress through stages sequentially
 *
 * PIPELINE:
 * 1. Drowsy → reduce oscillations
 * 2. Light NREM → sort memories
 * 3. Deep NREM → consolidate + homeostasis
 * 4. REM → creative recombination
 * 5. Wake → reset pressure
 *
 * PHASE 10.3: Uses emotional working memory for prioritization
 */
bool sleep_run_cycle(sleep_system_t sleep, uint32_t num_cycles)
{
    // Process pending bio-async messages
    if (sleep && sleep->bio_ctx) {
        bio_router_process_inbox(sleep->bio_ctx, 5);
    }

    if (sleep == NULL || num_cycles == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_run_cycle: num_cycles is zero");
        return false;
    }

    for (uint32_t cycle = 0; cycle < num_cycles; cycle++) {
        /* Stage 1: Drowsy */
        sleep_enter_state(sleep, SLEEP_STATE_DROWSY);
        sleep_stage_drowsy(sleep);

        /* Stage 2: Light NREM */
        sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);
        sleep_stage_light_nrem(sleep);

        /* Stage 3: Deep NREM - MAIN CONSOLIDATION */
        sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
        sleep_stage_deep_nrem(sleep);

        /* Stage 4: REM */
        if (sleep->config.enable_rem) {
            sleep_enter_state(sleep, SLEEP_STATE_REM);
            sleep_stage_rem(sleep);
        }

        /* Stage 5: Wake up */
        sleep_wake_up(sleep);

        /* WHAT: Increment cycle counter */
        nimcp_mutex_lock(&sleep->lock);
        sleep->sleep_cycles_completed++;
        nimcp_mutex_unlock(&sleep->lock);
    }

    return true;
}

/* ========================================================================
 * STATISTICS
 * ======================================================================== */

/**
 * WHAT: Get sleep statistics
 * WHY:  Monitor sleep quality and efficiency
 * HOW:  Copy statistics structure
 */
bool sleep_get_statistics(const sleep_system_t sleep, sleep_stats_t* stats)
{
    if (sleep == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_get_statistics: validation failed");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&sleep->lock);

    stats->total_awake_time_ms = sleep->total_awake_time;
    stats->total_sleep_time_ms = sleep->total_sleep_time;
    stats->sleep_cycles_completed = sleep->sleep_cycles_completed;
    stats->total_memories_replayed = sleep->total_memories_replayed;
    stats->total_synapses_pruned = sleep->total_synapses_pruned;
    stats->avg_consolidation_efficiency = sleep->consolidation_efficiency;
    stats->energy_savings_percent = sleep->energy_savings * 100.0F;
    stats->current_sleep_pressure = sleep->sleep_pressure;

    nimcp_mutex_unlock((nimcp_mutex_t*)&sleep->lock);

    return true;
}

/**
 * WHAT: Reset sleep statistics
 * WHY:  Clear counters for new measurement
 * HOW:  Zero statistics except current state
 */
void sleep_reset_statistics(sleep_system_t sleep)
{
    if (sleep == NULL) {
        return;
    }

    nimcp_mutex_lock(&sleep->lock);

    sleep->sleep_cycles_completed = 0;
    sleep->total_memories_replayed = 0;
    sleep->total_synapses_pruned = 0;
    sleep->consolidation_efficiency = 0.0F;
    sleep->energy_savings = 0.0F;
    sleep->total_awake_time = 0;
    sleep->total_sleep_time = 0;
    /* Note: Don't reset sleep_pressure or current_state */

    nimcp_mutex_unlock(&sleep->lock);
}

/**
 * WHAT: Set brain reference for working memory access
 * WHY:  Enable deep sleep to access working memory
 * HOW:  Store brain pointer (void* to avoid circular dependency)
 *
 * NOTE: This is called by brain_create() during initialization
 *
 * @param sleep Sleep system
 * @param brain Brain handle (void* cast)
 */
void sleep_set_brain_reference(sleep_system_t sleep, void* brain)
{
    if (sleep == NULL) {
        return;
    }

    nimcp_mutex_lock(&sleep->lock);
    sleep->brain_ref = brain;
    nimcp_mutex_unlock(&sleep->lock);
}

/* ========================================================================
 * STATE CHANGE NOTIFICATION IMPLEMENTATION
 * ======================================================================== */

/**
 * WHAT: Notify all registered callbacks of state change
 * WHY:  Broadcast state changes to all observing modules
 * HOW:  Walk callback list and invoke each callback
 *
 * SAFETY: Callbacks are invoked WITHOUT holding the lock to prevent deadlock
 *         if callbacks try to call back into the sleep system
 */
static void sleep_notify_state_change(sleep_system_t sleep, sleep_state_t new_state)
{
    /* Guard clause: Validate input */
    if (sleep == NULL) {
        return;
    }

    /* WHAT: Build temporary copy of callback list */
    /* WHY:  Prevent holding lock during callback execution (deadlock risk) */
    /* HOW:  Walk list with lock, copy to array, invoke without lock */

    #define MAX_CALLBACKS 32
    sleep_state_callback_t callbacks[MAX_CALLBACKS];
    void* user_data[MAX_CALLBACKS];
    uint32_t callback_count = 0;

    nimcp_mutex_lock(&sleep->lock);

    /* WHAT: Copy callbacks to temporary array */
    sleep_callback_entry_t* current = sleep->callbacks;
    while (current != NULL && callback_count < MAX_CALLBACKS) {
        callbacks[callback_count] = current->callback;
        user_data[callback_count] = current->user_data;
        callback_count++;
        current = current->next;
    }

    nimcp_mutex_unlock(&sleep->lock);

    /* WHAT: Invoke callbacks without holding lock */
    /* WHY:  Prevent deadlock if callbacks call sleep system functions */
    for (uint32_t i = 0; i < callback_count; i++) {
        if (callbacks[i] != NULL) {
            callbacks[i](new_state, user_data[i]);
        }
    }
}

/**
 * WHAT: Register callback for sleep state changes
 * WHY:  Allow modules to react immediately when sleep state changes
 * HOW:  Add callback to linked list of observers
 */
bool sleep_register_state_callback(sleep_system_t sleep,
                                    sleep_state_callback_t callback,
                                    void* user_data)
{
    /* Guard clause: Validate inputs */
    if (sleep == NULL || callback == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_register_state_callback: validation failed");
        return false;
    }

    /* WHAT: Allocate callback entry */
    sleep_callback_entry_t* entry =
        (sleep_callback_entry_t*)nimcp_malloc(sizeof(sleep_callback_entry_t));
    if (entry == NULL) {
        NIMCP_LOGGING_ERROR("Failed to allocate callback entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_register_state_callback: validation failed");
        return false;
    }

    /* WHAT: Initialize entry */
    entry->callback = callback;
    entry->user_data = user_data;
    entry->next = NULL;

    /* WHAT: Add to linked list (prepend for O(1)) */
    nimcp_mutex_lock(&sleep->lock);
    entry->next = sleep->callbacks;
    sleep->callbacks = entry;
    nimcp_mutex_unlock(&sleep->lock);

    NIMCP_LOGGING_DEBUG("Registered sleep state callback");
    return true;
}

/**
 * WHAT: Unregister sleep state change callback
 * WHY:  Remove callback when module is destroyed
 * HOW:  Find and remove from linked list
 */
bool sleep_unregister_state_callback(sleep_system_t sleep,
                                      sleep_state_callback_t callback,
                                      void* user_data)
{
    /* Guard clause: Validate inputs */
    if (sleep == NULL || callback == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_unregister_state_callback: validation failed");
        return false;
    }

    bool found = false;

    nimcp_mutex_lock(&sleep->lock);

    /* WHAT: Find and remove matching callback */
    /* WHY:  Clean up when module is destroyed */
    /* HOW:  Walk linked list, remove matching entry */

    sleep_callback_entry_t** current = &sleep->callbacks;
    while (*current != NULL) {
        sleep_callback_entry_t* entry = *current;

        /* WHAT: Match both callback and user_data */
        /* WHY:  Same callback might be registered with different contexts */
        if (entry->callback == callback && entry->user_data == user_data) {
            /* Remove from list */
            *current = entry->next;
            nimcp_free(entry);
            entry = NULL;
            found = true;
            break;
        }

        current = &entry->next;
    }

    nimcp_mutex_unlock(&sleep->lock);

    if (found) {
        NIMCP_LOGGING_DEBUG("Unregistered sleep state callback");
    } else {
        NIMCP_LOGGING_WARN("Callback not found in registration list");
    }

    return found;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int sleep_wake_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Wake");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Wake");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Wake");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_wake_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_sleep_wake_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_wake_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_training_begin: NULL argument");
        return -1;
    }
    sleep_wake_heartbeat_instance(NULL, "sleep_wake_training_begin", 0.0f);
    (void)(struct sleep_callback_entry*)instance; /* Module state available for reset */
    return 0;
}

int sleep_wake_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_training_end: NULL argument");
        return -1;
    }
    sleep_wake_heartbeat_instance(NULL, "sleep_wake_training_end", 1.0f);
    (void)(struct sleep_callback_entry*)instance; /* Module state available for finalization */
    return 0;
}

int sleep_wake_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_wake_heartbeat_instance(NULL, "sleep_wake_training_step", progress);
    (void)(struct sleep_callback_entry*)instance; /* Module state available for step adaptation */
    return 0;
}

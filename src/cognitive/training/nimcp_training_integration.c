/**
 * @file nimcp_training_integration.c
 * @brief Training Integration Layer - Implementation
 *
 * WHAT: Simplified wrapper around basal ganglia, medulla, symbolic logic,
 *       reasoning chain, and information forager for the training pipeline
 * WHY:  The training pipeline needs clean, simple APIs to interact with
 *       brain subsystems without managing their individual complexities
 * HOW:  Each function guards against NULL brain and missing subsystems,
 *       returning sensible defaults when subsystems are unavailable
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include "cognitive/training/nimcp_training_integration.h"

#include "utils/math/nimcp_math_helpers.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"
#include "core/medulla/nimcp_medulla.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_forward_chaining.h"
#include "cognitive/reasoning/nimcp_backward_chaining.h"
#include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "core/medulla/nimcp_arousal_state.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "portia/nimcp_portia_tier_switch.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "training_integration"

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple FNV-1a hash for domain string to uint32_t context ID
 *
 * WHAT: Hash a domain string into a 32-bit integer
 * WHY:  BG habits use uint32_t trigger_context; we need to map string domains
 * HOW:  FNV-1a algorithm - fast, well-distributed for short strings
 */
static uint32_t hash_domain(const char* domain) {
    if (!domain) { return 0; }
    uint32_t hash = 2166136261u; /* FNV offset basis */
    while (*domain) {
        hash ^= (uint8_t)*domain++;
        hash *= 16777619u; /* FNV prime */
    }
    return hash;
}

/* Use centralized nimcp_clampf from nimcp_math_helpers.h */

/**
 * @brief Get the core basal ganglia from brain, or NULL if unavailable
 */
static basal_ganglia_t* get_bg_core(brain_t brain) {
    if (!brain) { return NULL; }
    if (!brain->basal_ganglia_enabled) { return NULL; }
    if (!brain->basal_ganglia) { return NULL; }
    return bg_enhanced_get_core(brain->basal_ganglia);
}

/*=============================================================================
 * REASONING ENGINE CACHE
 *
 * Since we cannot modify brain_internal.h, we use a simple static cache
 * that stores one reasoning engine at a time. The training pipeline
 * typically operates on a single brain, so this is sufficient.
 *===========================================================================*/

/*
 * Thread-local reasoning engine cache.
 *
 * WHY:  Athena uses 4 concurrent instructors that each call brain_ti_reason().
 *       Static globals without synchronization cause data races.
 * HOW:  _Thread_local gives each instructor thread its own engine instance.
 *       This is correct because each instructor works on its own brain or
 *       its own reasoning queries, and the engine is cheap to create per-thread.
 */
/* BUG-20 note: These thread-local caches are never automatically cleaned up.
 * brain_ti_destroy_reasoning() MUST be called explicitly before thread exit
 * to avoid leaking the reasoning engine. Callers (e.g., Athena instructors)
 * should call brain_ti_destroy_reasoning(brain) in their thread cleanup path. */
static _Thread_local reasoning_engine_t* g_cached_engine = NULL;
static _Thread_local brain_t g_cached_brain = NULL;
static _Thread_local uint32_t g_last_reasoning_steps = 0;

/*=============================================================================
 * BASAL GANGLIA TRAINING INTEGRATION
 *===========================================================================*/

int brain_ti_update_reward(brain_t brain, float actual_reward, float expected_reward) {
    if (!brain) { return -1; }
    if (!brain->basal_ganglia_enabled || !brain->basal_ganglia) {
        return -1;
    }

    /* Use enhanced BG reward processing which updates all subsystems */
    int rc = bg_enhanced_process_reward(brain->basal_ganglia,
                                        actual_reward, expected_reward);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: bg_enhanced_process_reward "
                           "failed (rc=%d)", rc);
        return -1;
    }

    return 0;
}

float brain_ti_get_conflict(brain_t brain) {
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return 0.0f; }
    return basal_ganglia_get_conflict(bg);
}

int brain_ti_get_mode(brain_t brain) {
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return 0; } /* BG_MODE_GOAL_DIRECTED */
    return (int)basal_ganglia_get_mode(bg);
}

float brain_ti_get_dopamine(brain_t brain) {
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return 0.5f; } /* BG_DOPAMINE_BASELINE */
    return basal_ganglia_get_dopamine(bg);
}

float brain_ti_get_rpe(brain_t brain) {
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return 0.0f; }
    return basal_ganglia_get_rpe(bg);
}

int brain_ti_register_habit(brain_t brain, const char* domain, uint32_t action_id) {
    if (!domain) { return -1; }
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return -1; }

    uint32_t context = hash_domain(domain);
    uint32_t habit_id = 0;
    int rc = basal_ganglia_register_habit(bg, context, action_id, &habit_id);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: register_habit failed for "
                           "domain='%s' action=%u (rc=%d)", domain, action_id, rc);
        return -1;
    }

    NIMCP_LOGGING_DEBUG("training_integration: registered habit %u for "
                        "domain='%s' (context=0x%08x, action=%u)",
                        habit_id, domain, context, action_id);
    return (int)habit_id;
}

int brain_ti_check_habit(brain_t brain, const char* domain) {
    if (!domain) { return -1; }
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return -1; }

    uint32_t context = hash_domain(domain);
    uint32_t action_id = 0;
    bool found = basal_ganglia_check_habit(bg, context, &action_id);
    if (!found) { return -1; }

    return (int)action_id;
}

int brain_ti_strengthen_habit(brain_t brain, int habit_id, bool success) {
    if (habit_id < 0) { return -1; }
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return -1; }

    int rc = basal_ganglia_strengthen_habit(bg, (uint32_t)habit_id, success);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: strengthen_habit failed for "
                           "habit_id=%d (rc=%d)", habit_id, rc);
        return -1;
    }

    return 0;
}

float brain_ti_get_habit_strength(brain_t brain, int habit_id) {
    if (habit_id < 0) { return -1.0f; }
    basal_ganglia_t* bg = get_bg_core(brain);
    if (!bg) { return -1.0f; }
    return basal_ganglia_get_habit_strength(bg, (uint32_t)habit_id);
}

/*=============================================================================
 * MEDULLA TRAINING INTEGRATION
 *===========================================================================*/

float brain_ti_get_arousal(brain_t brain) {
    if (!brain) { return 0.5f; }
    if (!brain->medulla_enabled || !brain->medulla) { return 0.5f; }
    float arousal = medulla_get_arousal_level(brain->medulla);
    /* medulla_get_arousal_level returns -1.0f on error */
    if (arousal < 0.0f) { return 0.5f; }
    return arousal;
}

int brain_ti_get_circadian_phase(brain_t brain) {
    if (!brain) { return 0; }
    if (!brain->medulla_enabled || !brain->medulla) { return 0; }
    return (int)medulla_get_circadian_phase(brain->medulla);
}

int brain_ti_boost_arousal(brain_t brain, float delta) {
    if (!brain) { return -1; }
    if (!brain->medulla_enabled || !brain->medulla) { return -1; }
    /* Route negative delta to reduce_arousal (defensive — callers should
     * use reduce_arousal directly, but this prevents error spam) */
    int rc;
    if (delta < 0.0f) {
        rc = medulla_reduce_arousal(brain->medulla, -delta);
    } else {
        rc = medulla_boost_arousal(brain->medulla, delta);
    }
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: boost_arousal failed "
                           "(delta=%.3f, rc=%d)", delta, rc);
        return -1;
    }
    return 0;
}

int brain_ti_reduce_arousal(brain_t brain, float delta) {
    if (!brain) { return -1; }
    if (!brain->medulla_enabled || !brain->medulla) { return -1; }
    int rc = medulla_reduce_arousal(brain->medulla, delta);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: reduce_arousal failed "
                           "(delta=%.3f, rc=%d)", delta, rc);
        return -1;
    }
    return 0;
}

float brain_ti_get_circadian_efficiency(brain_t brain) {
    if (!brain) { return 1.0f; }
    if (!brain->medulla_enabled || !brain->medulla) { return 1.0f; }

    circadian_phase_t phase = medulla_get_circadian_phase(brain->medulla);

    /*
     * Map circadian phase to learning efficiency multiplier.
     *
     * The medulla uses the circadian_phase_t enum from nimcp_medulla.h:
     *   CIRCADIAN_PHASE_EARLY_MORNING = 0  (06:00-09:00)
     *   CIRCADIAN_PHASE_MORNING       = 1  (09:00-12:00)
     *   CIRCADIAN_PHASE_AFTERNOON     = 2  (12:00-15:00)
     *   CIRCADIAN_PHASE_EVENING       = 3  (15:00-18:00)
     *   CIRCADIAN_PHASE_LATE_EVENING  = 4  (18:00-21:00)
     *   CIRCADIAN_PHASE_NIGHT         = 5  (21:00-24:00)
     *   CIRCADIAN_PHASE_DEEP_NIGHT    = 6  (00:00-03:00)
     *   CIRCADIAN_PHASE_PRE_DAWN      = 7  (03:00-06:00)
     */
    static const float phase_efficiency[] = {
        1.2f,   /* 0: EARLY_MORNING - rising, good */
        1.5f,   /* 1: MORNING      - peak alertness, best for learning */
        1.0f,   /* 2: AFTERNOON    - post-lunch dip recovery */
        1.2f,   /* 3: EVENING      - second peak */
        0.9f,   /* 4: LATE_EVENING - declining */
        0.85f,  /* 5: NIGHT        - low arousal */
        0.85f,  /* 6: DEEP_NIGHT   - minimal arousal */
        0.8f    /* 7: PRE_DAWN     - minimum efficiency */
    };

    int idx = (int)phase;
    /* BUG-22 fix: Replaced magic number 8 with computed array size */
    if (idx < 0 || idx >= (int)(sizeof(phase_efficiency) / sizeof(phase_efficiency[0]))) { return 1.0f; }
    return phase_efficiency[idx];
}

/*=============================================================================
 * SYMBOLIC LOGIC TRAINING INTEGRATION
 *===========================================================================*/

/**
 * @brief Check if symbolic logic engine is available on this brain
 */
static bool logic_available(brain_t brain) {
    if (!brain) { return false; }
    if (!brain->symbolic_logic) { return false; }
    return true;
}

bool brain_ti_add_fact(brain_t brain, const char* fact_str, float salience) {
    if (!logic_available(brain)) { return false; }
    if (!fact_str) { return false; }
    return brain_add_fact(brain, fact_str, salience);
}

bool brain_ti_add_rule(brain_t brain, const char* rule_str, float priority) {
    if (!logic_available(brain)) { return false; }
    if (!rule_str) { return false; }
    return brain_add_rule(brain, rule_str, priority);
}

int brain_ti_forward_chain(brain_t brain, uint32_t max_iterations) {
    if (!logic_available(brain)) { return 0; }
    if (max_iterations == 0) { max_iterations = 100; }

    forward_chain_result_t result;
    memset(&result, 0, sizeof(result));
    bool ok = brain_forward_chain(brain, max_iterations, &result);
    if (!ok) { return 0; }

    int derived = (int)result.num_new_facts;

    if (derived > 0) {
        NIMCP_LOGGING_DEBUG("training_integration: forward_chain derived %d "
                            "new facts in %u iterations (conf=%.3f)",
                            derived, result.iterations_performed,
                            result.confidence);
    }

    forward_chain_free_result(&result);
    return derived;
}

float brain_ti_backward_chain(brain_t brain, const char* goal_str) {
    if (!logic_available(brain)) { return -1.0f; }
    if (!goal_str) { return -1.0f; }

    backward_chain_result_t result;
    memset(&result, 0, sizeof(result));
    bool ok = brain_backward_chain(brain, goal_str, &result);

    if (!ok) {
        /* Could be parse error or genuine failure to prove */
        backward_chain_free_result(&result);
        return -1.0f;
    }

    float confidence = result.proven ? result.confidence : 0.0f;

    if (result.proven) {
        NIMCP_LOGGING_DEBUG("training_integration: backward_chain proved '%s' "
                            "with confidence=%.3f (%u steps, depth=%u)",
                            goal_str, result.confidence,
                            result.num_steps, result.depth_reached);
    }

    backward_chain_free_result(&result);
    return confidence;
}

int brain_ti_query_knowledge(brain_t brain, const char* query_str) {
    if (!logic_available(brain)) { return 0; }
    if (!query_str) { return 0; }

    kb_query_result_t result;
    memset(&result, 0, sizeof(result));
    bool ok = brain_query_knowledge(brain, query_str, &result);
    if (!ok) { return 0; }

    int matches = result.num_matches;
    kb_free_query_result(&result);
    return matches;
}

int brain_ti_get_logic_stats(brain_t brain, brain_ti_logic_stats_t* stats) {
    if (!stats) { return -1; }
    memset(stats, 0, sizeof(*stats));

    if (!logic_available(brain)) { return -1; }

    /* Get base logic engine stats directly from symbolic_logic_t */
    logic_stats_t ls;
    memset(&ls, 0, sizeof(ls));
    if (symbolic_logic_get_stats(brain->symbolic_logic, &ls)) {
        stats->total_facts = ls.facts_stored;
        stats->total_rules = ls.rules_applied;
    }

    /* Get forward chaining stats */
    uint32_t fc_iters = 0;
    uint32_t fc_derived = 0;
    uint64_t fc_time = 0;
    if (brain_get_forward_chain_stats(brain, &fc_iters, &fc_derived, &fc_time)) {
        stats->facts_derived = fc_derived;
    }

    /* Get backward chaining stats */
    uint32_t bc_attempted = 0;
    uint32_t bc_succeeded = 0;
    float bc_avg_depth = 0.0f;
    if (brain_get_backward_chain_stats(brain, &bc_attempted, &bc_succeeded,
                                       &bc_avg_depth)) {
        stats->proofs_completed = bc_succeeded;
        stats->proofs_failed = bc_attempted - bc_succeeded;
    }

    /* Use the knowledge base interface for accurate counts */
    stats->total_facts = brain_get_fact_count(brain);
    stats->total_rules = brain_get_rule_count(brain);

    return 0;
}

/*=============================================================================
 * REASONING CHAIN TRAINING INTEGRATION
 *===========================================================================*/

int brain_ti_init_reasoning(brain_t brain) {
    if (!brain) { return -1; }

    /* If already cached for this brain, reuse */
    if (g_cached_engine && g_cached_brain == brain) {
        return 0;
    }

    /* Destroy any existing cached engine for a different brain */
    if (g_cached_engine) {
        NIMCP_LOGGING_INFO("training_integration: replacing cached reasoning "
                           "engine (was for different brain)");
        reasoning_engine_destroy(g_cached_engine);
        g_cached_engine = NULL;
        g_cached_brain = NULL;
        g_last_reasoning_steps = 0;
    }

    /* Create new engine with defaults */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    g_cached_engine = reasoning_engine_create(&config);
    if (!g_cached_engine) {
        NIMCP_LOGGING_ERROR("training_integration: failed to create reasoning "
                            "engine");
        return -1;
    }

    /* Connect to brain */
    int rc = reasoning_engine_connect_brain(g_cached_engine, brain);
    if (rc != 0) {
        NIMCP_LOGGING_ERROR("training_integration: failed to connect reasoning "
                            "engine to brain (rc=%d)", rc);
        reasoning_engine_destroy(g_cached_engine);
        g_cached_engine = NULL;
        return -1;
    }

    g_cached_brain = brain;
    g_last_reasoning_steps = 0;

    NIMCP_LOGGING_INFO("training_integration: reasoning engine initialized "
                       "and connected to brain");
    return 0;
}

float brain_ti_reason(brain_t brain, const char* query) {
    if (!brain || !query) { return -1.0f; }

    /* Check cache */
    if (!g_cached_engine || g_cached_brain != brain) {
        NIMCP_LOGGING_WARN("training_integration: brain_ti_reason called "
                           "without brain_ti_init_reasoning");
        return -1.0f;
    }

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(g_cached_engine, query, &chain);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("training_integration: reasoning failed for "
                           "query='%.80s' (rc=%d)", query, rc);
        reasoning_chain_cleanup(&chain);
        g_last_reasoning_steps = 0;
        return -1.0f;
    }

    float confidence = reasoning_chain_get_confidence(&chain);
    g_last_reasoning_steps = reasoning_chain_get_num_steps(&chain);

    NIMCP_LOGGING_DEBUG("training_integration: reasoning completed for "
                        "'%.80s': confidence=%.3f, steps=%u",
                        query, confidence, g_last_reasoning_steps);

    reasoning_chain_cleanup(&chain);
    return confidence;
}

uint32_t brain_ti_get_reasoning_steps(brain_t brain) {
    if (!brain) { return 0; }
    if (g_cached_brain != brain) { return 0; }
    return g_last_reasoning_steps;
}

void brain_ti_destroy_reasoning(brain_t brain) {
    /* Clear cache if brain matches or if brain is NULL (force clear) */
    if (!brain || g_cached_brain == brain) {
        if (g_cached_engine) {
            NIMCP_LOGGING_INFO("training_integration: destroying cached "
                               "reasoning engine");
            reasoning_engine_destroy(g_cached_engine);
        }
        g_cached_engine = NULL;
        g_cached_brain = NULL;
        g_last_reasoning_steps = 0;
    }
}

/*=============================================================================
 * COMBINED TRAINING HELPERS
 *===========================================================================*/

float brain_ti_compute_adaptive_lr(brain_t brain, float base_lr) {
    if (!brain) { return base_lr; }

    /* Arousal factor: 0.5 + 0.5 * arousal  (0.5x at zero, 1.0x at full) */
    float arousal = brain_ti_get_arousal(brain);
    float arousal_factor = 0.5f + 0.5f * arousal;

    /* Circadian factor: phase-based efficiency multiplier */
    float circadian_factor = brain_ti_get_circadian_efficiency(brain);

    /* RPE bonus: clamp(rpe * 0.2, -0.2, 0.3) */
    float rpe = brain_ti_get_rpe(brain);
    float rpe_bonus = nimcp_clampf(rpe * 0.2f, -0.2f, 0.3f);

    float adaptive_lr = base_lr * arousal_factor * circadian_factor *
                        (1.0f + rpe_bonus);

    NIMCP_LOGGING_DEBUG("training_integration: adaptive_lr=%.6f "
                        "(base=%.6f, arousal=%.3f[x%.3f], "
                        "circadian=x%.3f, rpe=%.3f[bonus=%.3f])",
                        adaptive_lr, base_lr, arousal, arousal_factor,
                        circadian_factor, rpe, rpe_bonus);

    return adaptive_lr;
}

int brain_ti_post_batch_update(brain_t brain, float accuracy,
                               float expected_accuracy, const char* domain) {
    if (!brain) { return -1; }

    /* Step 1: Update BG reward with accuracy as reward signal */
    brain_ti_update_reward(brain, accuracy, expected_accuracy);

    /* Step 2: Check for habitual behavior in this domain */
    if (domain) {
        int action_id = brain_ti_check_habit(brain, domain);
        if (action_id >= 0) {
            /* Habit exists -- check its strength */
            /* We need the habit_id, not the action_id, for strength query.
             * Since we only have action_id from check_habit, we look up
             * the habit by iterating. For simplicity, we use the BG core
             * habit system directly. */
            basal_ganglia_t* bg = get_bg_core(brain);
            if (bg) {
                uint32_t context = hash_domain(domain);
                /* BUG-19 fix: Lock BG mutex before accessing internal fields
                 * (num_habits, habits[]) which may be modified concurrently */
                if (bg->mutex) {
                    nimcp_mutex_lock(bg->mutex);
                }
                /* Find the habit_id for this context */
                for (uint32_t i = 0; i < bg->num_habits; i++) {
                    if (bg->habits[i].trigger_context == context) {
                        uint32_t habit_id = bg->habits[i].habit_id;
                        /* W4-2 fix: Always unlock before goto to prevent deadlock.
                         * The old code had the unlock here but goto bg_done
                         * at line 612 would skip the unlock at line 616. */
                        if (bg->mutex) {
                            nimcp_mutex_unlock(bg->mutex);
                        }
                        /* HIGH-7: The unlock-before-use pattern is safe here because:
                         * 1. habit_id is a uint32_t identifier (not a pointer) — IDs
                         *    remain valid even if the habits array is reallocated.
                         * 2. basal_ganglia_get_habit_strength() and
                         *    basal_ganglia_strengthen_habit() both take (bg, habit_id)
                         *    and acquire their own internal locks as needed.
                         * 3. If the habit is removed concurrently, these functions
                         *    return error values (-1.0f or negative int) gracefully. */
                        float strength = basal_ganglia_get_habit_strength(
                            bg, habit_id);
                        if (strength > 0.7f) {
                            NIMCP_LOGGING_INFO("training_integration: domain "
                                               "'%s' in habitual mode "
                                               "(strength=%.3f, action=%d)",
                                               domain, strength, action_id);
                        }

                        /* Step 3: Strengthen habit if accuracy exceeds
                         * expected */
                        if (accuracy > expected_accuracy) {
                            basal_ganglia_strengthen_habit(
                                bg, habit_id, true);
                            NIMCP_LOGGING_DEBUG("training_integration: "
                                                "strengthened habit for "
                                                "domain='%s' (accuracy %.3f "
                                                "> expected %.3f)",
                                                domain, accuracy,
                                                expected_accuracy);
                        }
                        goto bg_done;
                    }
                }
                if (bg->mutex) {
                    nimcp_mutex_unlock(bg->mutex);
                }
                bg_done: ;
            }
        }
    }

    return 0;
}

/*=============================================================================
 * PORTIA-REASONING RESOURCE ADAPTATION
 *===========================================================================*/

bool brain_ti_should_skip_reasoning(void) {
    return reasoning_portia_should_skip();
}

int brain_ti_get_reasoning_degradation(void) {
    reasoning_budget_t budget = reasoning_portia_compute_budget();
    return (int)budget.source_degradation;
}

int brain_ti_get_reasoning_phases_disabled(void) {
    reasoning_budget_t budget = reasoning_portia_compute_budget();
    reasoning_engine_config_t config = reasoning_engine_default_config();
    return reasoning_portia_apply_budget(&config, &budget);
}

/*=============================================================================
 * HYPOTHALAMUS-REASONING MOTIVATIONAL MODULATION
 *===========================================================================*/

/**
 * @brief Cached hypothalamus modulation to avoid redundant computation
 *
 * WHY:  brain_ti_get_cognitive_capacity(), brain_ti_get_urgency_mode(), and
 *       brain_ti_get_stress_level() each independently called
 *       reasoning_hypo_compute_modulation(brain), tripling the cost.
 * HOW:  Cache per-brain result; invalidate when brain changes.
 *       Thread-local to remain safe under concurrent instructor access.
 */
static _Thread_local brain_t g_hypo_cached_brain = NULL;
static _Thread_local reasoning_hypo_modulation_t g_hypo_cached_mod;
static _Thread_local bool g_hypo_cache_valid = false;

static inline const reasoning_hypo_modulation_t* get_cached_hypo_modulation(brain_t brain) {
    if (!g_hypo_cache_valid || g_hypo_cached_brain != brain) {
        g_hypo_cached_mod = reasoning_hypo_compute_modulation(brain);
        g_hypo_cached_brain = brain;
        g_hypo_cache_valid = true;
    }
    return &g_hypo_cached_mod;
}

/**
 * @brief Invalidate cached hypothalamus modulation
 *
 * Call at the start of brain_ti_compute_modulation_state() so that each
 * full modulation cycle gets fresh hypothalamus data.
 */
static inline void invalidate_hypo_cache(void) {
    g_hypo_cache_valid = false;
}

float brain_ti_get_cognitive_capacity(brain_t brain) {
    const reasoning_hypo_modulation_t* mod = get_cached_hypo_modulation(brain);
    return mod->cognitive_capacity;
}

int brain_ti_get_urgency_mode(brain_t brain) {
    const reasoning_hypo_modulation_t* mod = get_cached_hypo_modulation(brain);
    return (int)mod->urgency_mode;
}

float brain_ti_get_stress_level(brain_t brain) {
    const reasoning_hypo_modulation_t* mod = get_cached_hypo_modulation(brain);
    return mod->stress_level;
}

/*=============================================================================
 * CONVERGENT REASONING INTEGRATION
 *===========================================================================*/

bool brain_ti_is_convergent_reasoning(brain_t brain) {
    (void)brain;
    if (!g_cached_engine) return false;
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(g_cached_engine, &stats);
    return stats.convergent_queries > 0;
}

uint32_t brain_ti_get_convergent_contributor_count(brain_t brain) {
    (void)brain;
    if (!g_cached_engine) return 0;
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(g_cached_engine, &stats);
    if (stats.convergent_queries == 0) return 0;
    return (uint32_t)stats.avg_convergent_contributors;
}

/* ---- Mesh bridge wrappers ---- */

bool brain_ti_mesh_is_available(void) {
    return reasoning_mesh_is_available();
}

uint32_t brain_ti_mesh_get_participant_count(void) {
    uint32_t count = 0;
    reasoning_mesh_get_channel_stats(&count, NULL);
    return count;
}

float brain_ti_mesh_get_coherence(void) {
    float coherence = 0.0f;
    reasoning_mesh_get_channel_stats(NULL, &coherence);
    return coherence;
}

/*=============================================================================
 * UNIFIED CONTINUOUS MODULATION PIPELINE
 *
 * Composes arousal (inverted-U cognitive gain), inflammation effects,
 * training instability response, Portia resource allocation, and
 * stress/cognitive capacity into a single modulation state.
 *===========================================================================*/

int brain_ti_compute_modulation_state(brain_t brain, brain_ti_modulation_state_t* state) {
    if (!state) { return -1; }
    memset(state, 0, sizeof(*state));

    /* Invalidate cached hypothalamus data so this cycle gets fresh values */
    invalidate_hypo_cache();

    /*
     * NULL brain: return sensible identity defaults so that
     * base_lr * final_lr_factor == base_lr, etc.
     */
    if (!brain) {
        state->arousal_level                = 0.5f;
        state->arousal_cognitive_gain       = 1.0f;
        state->arousal_memory_consolidation = 1.0f;
        state->circadian_efficiency         = 1.0f;
        state->rpe_bonus                    = 0.0f;
        state->inflammation_learning_factor = 1.0f;
        state->inflammation_precision       = 1.0f;
        state->instability_lr_scale         = 1.0f;
        state->instability_batch_scale      = 1.0f;
        state->instability_clip_factor      = 1.0f;
        state->portia_learning_gate         = 1.0f;
        state->portia_compute_budget        = 1.0f;
        state->stress_level                 = 0.0f;
        state->cognitive_capacity           = 1.0f;
        state->conflict_level               = 0.0f;
        state->final_lr_factor              = 1.0f;
        state->final_batch_factor           = 1.0f;
        state->final_clip_factor            = 1.0f;
        state->should_pause                 = false;
        return 0;
    }

    /* --- Arousal module (inverted-U cognitive gain) --- */
    state->arousal_level = brain_ti_get_arousal(brain);
    {
        arousal_params_t ap;
        memset(&ap, 0, sizeof(ap));
        if (arousal_compute_parameters(state->arousal_level, &ap) == 0) {
            state->arousal_cognitive_gain       = ap.cognitive_gain;
            state->arousal_memory_consolidation = ap.memory_consolidation;
        } else {
            /* Fallback: linear ramp (old behavior) */
            state->arousal_cognitive_gain       = 0.5f + 0.5f * state->arousal_level;
            state->arousal_memory_consolidation = 1.0f;
        }
    }

    /* --- Circadian efficiency --- */
    state->circadian_efficiency = brain_ti_get_circadian_efficiency(brain);

    /* --- Reward prediction error bonus --- */
    {
        float rpe = brain_ti_get_rpe(brain);
        state->rpe_bonus = nimcp_clampf(rpe * 0.2f, -0.2f, 0.3f);
    }

    /* --- Inflammation effects --- */
    if (brain->immune_enabled && brain->immune_system) {
        inflammation_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        if (brain_immune_get_inflammation_effects(brain->immune_system,
                                                   &effects) == 0) {
            /* capacity_factor is 1.0 at health, reduced at high inflammation */
            state->inflammation_learning_factor = effects.capacity_factor;
            /* channel_factor serves as precision modulation */
            state->inflammation_precision       = effects.channel_factor;
        } else {
            state->inflammation_learning_factor = 1.0f;
            state->inflammation_precision       = 1.0f;
        }
    } else {
        state->inflammation_learning_factor = 1.0f;
        state->inflammation_precision       = 1.0f;
    }

    /* --- Training instability response --- */
    /*
     * Compute instability from loss volatility and gradient variance.
     *
     * WHY:  Previously hardcoded to 1.0, making the documented
     *       exp(-3*instability_score) behavior never execute.
     * HOW:  Derive instability_score from the training logic bridge if
     *       available; otherwise estimate from brain immune system's
     *       inflammation capacity (high inflammation correlates with
     *       training instability).
     *
     * The training_logic_bridge_t is not stored on brain_t, so we cannot
     * call training_logic_get_instability_metrics() directly. Instead, we
     * use the inflammation capacity factor as a proxy: when the immune
     * system detects instability, it raises inflammation, reducing capacity.
     *
     * instability_score = 1.0 - inflammation_capacity_factor
     *   - healthy (capacity=1.0) → instability=0.0 → lr_scale=1.0
     *   - inflamed (capacity=0.5) → instability=0.5 → lr_scale=exp(-1.5)≈0.22
     *   - severe  (capacity=0.1) → instability=0.9 → lr_scale=exp(-2.7)≈0.07
     */
    {
        float instability_score = 0.0f;

        /* Use inflammation_learning_factor as proxy for instability.
         * Inflammation is already computed above, so reuse it. */
        if (state->inflammation_learning_factor < 1.0f) {
            instability_score = 1.0f - state->inflammation_learning_factor;
        }

        /* Also factor in conflict level from BG — high conflict indicates
         * the model is uncertain between competing strategies */
        float bg_conflict = brain_ti_get_conflict(brain);
        instability_score = fmaxf(instability_score, bg_conflict);

        instability_score = nimcp_clampf(instability_score, 0.0f, 1.0f);

        state->instability_lr_scale    = expf(-3.0f * instability_score);
        state->instability_batch_scale = 1.0f + instability_score;  /* increase batch size under instability */
        state->instability_clip_factor = fmaxf(0.1f, 1.0f - instability_score);  /* tighter clipping under instability */
    }

    /* --- Stress level (HPA axis cortisol) --- */
    /* NOTE: Computed before Portia so we can use stress as a resource pressure proxy */
    state->stress_level = brain_ti_get_stress_level(brain);

    /* --- Cognitive capacity --- */
    /* NOTE: Computed before Portia so we can use cognitive demand as a pressure proxy */
    state->cognitive_capacity = brain_ti_get_cognitive_capacity(brain);

    /* --- Portia resource allocation --- */
    {
        portia_allocation_t alloc;
        memset(&alloc, 0, sizeof(alloc));

        /*
         * Estimate resource pressure from brain state.
         * portia_compute_resource_pressure() requires a tier_switch handle
         * which is not stored on brain_t. Instead, we derive a proxy from
         * the HPA axis stress level and cognitive demand.
         *
         * BUG FIX: Previously hardcoded 0.0f, making Portia allocation
         * always return full-capacity values regardless of system state.
         */
        float resource_pressure = state->stress_level;  /* Proxy from HPA axis */

        /* If the hypothalamus reports high cognitive demand, blend it in */
        float cog_demand = 1.0f - state->cognitive_capacity;
        if (cog_demand > resource_pressure) {
            resource_pressure = 0.6f * cog_demand + 0.4f * resource_pressure;
        }

        resource_pressure = nimcp_clampf(resource_pressure, 0.0f, 1.0f);

        if (portia_compute_allocation(resource_pressure, &alloc) == 0) {
            state->portia_learning_gate = alloc.feature_gate_learning;
            state->portia_compute_budget = alloc.compute_budget_scale;
        } else {
            state->portia_learning_gate  = 1.0f;
            state->portia_compute_budget = 1.0f;
        }
    }

    /* --- BG conflict --- */
    state->conflict_level = brain_ti_get_conflict(brain);

    /* BUG-23: Neuromodulator levels are queried but only logged, not used in
     * the modulation computation. TODO: Either add DA/5HT/ACh/NE fields to
     * brain_ti_modulation_state_t and incorporate into the LR formula, or
     * remove this block to avoid wasted computation. Keeping for now since
     * the debug logging is useful during training diagnostics. */
    if (brain->neuromodulator_system) {
        float da = neuromodulator_get_level(brain->neuromodulator_system,
                                             NEUROMOD_DOPAMINE);
        float ser = neuromodulator_get_level(brain->neuromodulator_system,
                                              NEUROMOD_SEROTONIN);
        float ach = neuromodulator_get_level(brain->neuromodulator_system,
                                              NEUROMOD_ACETYLCHOLINE);
        float ne = neuromodulator_get_level(brain->neuromodulator_system,
                                             NEUROMOD_NOREPINEPHRINE);
        NIMCP_LOGGING_DEBUG("training_integration: neuromodulators "
                            "DA=%.3f 5HT=%.3f ACh=%.3f NE=%.3f",
                            da, ser, ach, ne);
        (void)da; (void)ser; (void)ach; (void)ne;
    }

    /* =================================================================
     * COMPOSE FINAL MODULATION FACTORS
     * ================================================================= */

    state->final_lr_factor =
        state->arousal_cognitive_gain *
        state->circadian_efficiency *
        (1.0f + state->rpe_bonus) *
        state->instability_lr_scale *
        state->inflammation_learning_factor *
        state->portia_learning_gate *
        (1.0f - 0.3f * state->stress_level) *
        (0.7f + 0.3f * state->cognitive_capacity);

    /* Clamp final LR factor to a safe range to prevent runaway scaling
     * even under extreme combinations of modulation inputs */
    state->final_lr_factor = nimcp_clampf(state->final_lr_factor, 0.01f, 10.0f);

    state->final_batch_factor =
        state->instability_batch_scale *
        state->portia_compute_budget;

    state->final_clip_factor = state->instability_clip_factor;

    state->should_pause =
        (state->instability_lr_scale < 0.01f) ||
        (state->inflammation_learning_factor < 0.1f) ||
        (state->portia_learning_gate < 0.05f);

    NIMCP_LOGGING_DEBUG("training_integration: unified modulation "
                        "lr_factor=%.4f batch_factor=%.4f clip_factor=%.4f "
                        "pause=%s (arousal_cg=%.3f circ=%.3f rpe_b=%.3f "
                        "infl=%.3f inst=%.3f portia=%.3f stress=%.3f "
                        "cogcap=%.3f)",
                        state->final_lr_factor, state->final_batch_factor,
                        state->final_clip_factor,
                        state->should_pause ? "YES" : "NO",
                        state->arousal_cognitive_gain,
                        state->circadian_efficiency,
                        state->rpe_bonus,
                        state->inflammation_learning_factor,
                        state->instability_lr_scale,
                        state->portia_learning_gate,
                        state->stress_level,
                        state->cognitive_capacity);

    return 0;
}

float brain_ti_compute_unified_lr(brain_t brain, float base_lr,
                                   brain_ti_modulation_state_t* state) {
    brain_ti_modulation_state_t local;
    brain_ti_modulation_state_t* s = state ? state : &local;

    brain_ti_compute_modulation_state(brain, s);
    return base_lr * s->final_lr_factor;
}

float brain_ti_compute_unified_batch(brain_t brain, float base_batch) {
    brain_ti_modulation_state_t s;
    brain_ti_compute_modulation_state(brain, &s);
    return base_batch * s.final_batch_factor;
}

float brain_ti_compute_unified_clip(brain_t brain, float base_clip) {
    brain_ti_modulation_state_t s;
    brain_ti_compute_modulation_state(brain, &s);
    return base_clip * s.final_clip_factor;
}

/*=============================================================================
 * DECISION CYCLE ORCHESTRATOR
 *
 * Combines Layer 1 (convergent decisions), Layer 2 (causal DAG), and
 * Layer 3 (abductive diagnosis) into a single observe → diagnose →
 * simulate → decide pipeline.
 *===========================================================================*/

#include "middleware/training/nimcp_training_convergent_decision.h"
#include "middleware/training/nimcp_training_diagnosis.h"
#include "middleware/training/nimcp_training_causal_model.h"

int brain_ti_compute_decision_cycle(
    brain_t brain,
    const brain_ti_training_metrics_t* metrics,
    brain_ti_decision_cycle_result_t* result)
{
    if (!metrics || !result) { return -1; }
    memset(result, 0, sizeof(*result));
    result->lr_factor = 1.0f;
    result->batch_factor = 1.0f;
    result->grad_clip_factor = 1.0f;

    /* Get brain state — compute modulation once and cache for reuse below.
     * FIX #8: Previously brain_ti_compute_modulation_state() was called twice
     * in this function (once here and again at line ~1034). Now computed once. */
    float arousal_level = 0.5f;
    float inflammation_level = 0.0f;
    float resource_pressure = 0.0f;
    brain_ti_modulation_state_t cached_mod_state;
    bool has_mod_state = false;
    if (brain) {
        arousal_level = brain_ti_get_arousal(brain);
        /* Get inflammation and resource pressure from modulation state */
        if (brain_ti_compute_modulation_state(brain, &cached_mod_state) == 0) {
            has_mod_state = true;
            /*
             * Semantic conversion:
             *   cached_mod_state.inflammation_learning_factor is CAPACITY (1.0=healthy, 0.0=impaired)
             *   diagnoser expects inflammation as SEVERITY (0.0=none, 1.0=severe)
             *   severity = 1.0 - capacity
             */
            inflammation_level = 1.0f - cached_mod_state.inflammation_learning_factor;
            /*
             * Semantic conversion:
             *   cached_mod_state.portia_compute_budget is BUDGET (1.0=full, 0.0=exhausted)
             *   diagnoser expects resource_pressure as PRESSURE (0.0=idle, 1.0=critical)
             *   pressure = 1.0 - budget
             */
            resource_pressure = 1.0f - cached_mod_state.portia_compute_budget;
        }
    }

    /* --- Layer 3: Abductive Diagnosis --- */
    /* BUG-21 TODO(perf): training_diagnoser_create/destroy every call causes
     * heap churn. Consider caching in _Thread_local storage and using
     * training_diagnoser_reset() between calls. Same applies to
     * training_causal_model (line ~1042) and training_convergent_session
     * (line ~1079) below. All three allocate+free per decision cycle. */
    training_diagnoser_t* diag = training_diagnoser_create();
    training_diagnosis_t diagnosis;
    memset(&diagnosis, 0, sizeof(diagnosis));
    diagnosis.recommended_lr_factor = 1.0f;

    if (diag) {
        training_diagnoser_observe_from_metrics(diag,
            metrics->loss_current, metrics->loss_previous,
            metrics->grad_norm, metrics->grad_norm_previous,
            metrics->loss_volatility, metrics->gradient_variance,
            metrics->current_lr, metrics->current_batch,
            arousal_level, inflammation_level, resource_pressure);

        training_diagnoser_diagnose(diag, &diagnosis);
        training_diagnoser_destroy(diag);
    }

    /* Copy diagnosis results */
    snprintf(result->primary_diagnosis, sizeof(result->primary_diagnosis),
             "%s", diagnosis.primary_cause);
    result->diagnosis_plausibility = diagnosis.primary_plausibility;
    result->recommend_pause = diagnosis.recommend_pause;
    result->recommend_rollback = diagnosis.recommend_rollback;

    /* --- Layer 2: Causal Simulation --- */
    training_causal_model_t* causal = training_causal_model_create();
    training_intervention_result_t causal_result;
    memset(&causal_result, 0, sizeof(causal_result));

    if (causal) {
        /* Feed current observations */
        training_causal_observation_t obs = {
            .learning_rate = metrics->current_lr,
            .batch_size = metrics->current_batch,
            .gradient_norm = metrics->grad_norm,
            .loss_current = metrics->loss_current,
            .loss_volatility = metrics->loss_volatility,
            .gradient_variance = metrics->gradient_variance,
            .arousal_level = arousal_level,
            .inflammation_level = inflammation_level,
            .resource_pressure = resource_pressure,
        };
        training_causal_model_observe(causal, &obs);

        /* If diagnosis recommends LR reduction, simulate it */
        if (diagnosis.recommend_reduce_lr && diagnosis.recommended_lr_factor < 1.0f) {
            float proposed_lr = metrics->current_lr * diagnosis.recommended_lr_factor;
            training_causal_model_query_lr_intervention(causal, proposed_lr, &causal_result);
        } else {
            /* Default: query current LR effect */
            training_causal_model_query_lr_intervention(causal, metrics->current_lr, &causal_result);
        }

        training_causal_model_destroy(causal);
    }

    snprintf(result->causal_explanation, sizeof(result->causal_explanation),
             "%s", causal_result.explanation);
    result->causal_confidence = causal_result.confidence;
    result->lr_change_beneficial = causal_result.is_beneficial;

    /* --- Layer 1: Convergent Decision --- */
    training_convergent_session_t* session = training_convergent_session_create(NULL);
    if (session) {
        /* Submit evidence from diagnosis */
        if (diagnosis.num_observations > 0) {
            training_evidence_t diag_ev = {
                .source_name = "diagnosis",
                .type = diagnosis.recommend_pause ? TRAINING_EVIDENCE_PAUSE :
                        diagnosis.recommend_rollback ? TRAINING_EVIDENCE_ROLLBACK :
                        diagnosis.recommend_reduce_lr ? TRAINING_EVIDENCE_LR_MODULATION :
                        TRAINING_EVIDENCE_CONTINUE,
                .lr_factor = diagnosis.recommended_lr_factor,
                .batch_factor = diagnosis.recommend_increase_batch ? 1.5f : 1.0f,
                .grad_clip_factor = diagnosis.recommend_tighter_clip ? 0.5f : 1.0f,
                .urgency = diagnosis.recommend_rollback ? 0.95f :
                          diagnosis.recommend_pause ? 0.8f : 0.3f,
                .confidence = diagnosis.primary_plausibility,
            };
            training_convergent_submit_evidence(session, &diag_ev);
        }

        /* Submit evidence from causal model */
        if (causal_result.confidence > 0.0f) {
            training_evidence_t causal_ev = {
                .source_name = "causal_model",
                .type = causal_result.is_beneficial ? TRAINING_EVIDENCE_LR_MODULATION :
                        TRAINING_EVIDENCE_CONTINUE,
                .lr_factor = causal_result.is_beneficial ?
                    (1.0f + causal_result.predicted_effect * 0.5f) :
                    (1.0f - causal_result.predicted_effect * 0.5f),
                .batch_factor = 1.0f,
                .grad_clip_factor = 1.0f,
                .urgency = causal_result.causal_strength * 0.5f,
                .confidence = causal_result.confidence,
            };
            training_convergent_submit_evidence(session, &causal_ev);
        }

        /* Submit evidence from brain subsystems via cached modulation state
         * FIX #8: Reuse cached_mod_state computed earlier instead of calling
         * brain_ti_compute_modulation_state() a second time. */
        if (brain && has_mod_state) {
            const brain_ti_modulation_state_t* mod = &cached_mod_state;
                /* Arousal evidence */
                training_evidence_t arousal_ev = {
                    .source_name = "arousal",
                    .type = TRAINING_EVIDENCE_CONTINUE,
                    .lr_factor = mod->arousal_cognitive_gain,
                    .batch_factor = 1.0f,
                    .grad_clip_factor = 1.0f,
                    .urgency = 0.1f,
                    .confidence = 0.8f,
                };
                training_convergent_submit_evidence(session, &arousal_ev);

                /* Instability evidence */
                training_evidence_t instability_ev = {
                    .source_name = "instability",
                    .type = mod->instability_lr_scale < 0.5f ?
                            TRAINING_EVIDENCE_PAUSE : TRAINING_EVIDENCE_CONTINUE,
                    .lr_factor = mod->instability_lr_scale,
                    .batch_factor = mod->instability_batch_scale,
                    .grad_clip_factor = mod->instability_clip_factor,
                    .urgency = mod->instability_lr_scale < 0.3f ? 0.9f : 0.2f,
                    .confidence = 0.9f,
                };
                training_convergent_submit_evidence(session, &instability_ev);

                /* Inflammation evidence */
                training_evidence_t inflam_ev = {
                    .source_name = "inflammation",
                    .type = mod->inflammation_learning_factor < 0.3f ?
                            TRAINING_EVIDENCE_PAUSE : TRAINING_EVIDENCE_CONTINUE,
                    .lr_factor = mod->inflammation_learning_factor,
                    .batch_factor = 1.0f,
                    .grad_clip_factor = 1.0f,
                    .urgency = mod->inflammation_learning_factor < 0.3f ? 0.7f : 0.1f,
                    .confidence = 0.85f,
                };
                training_convergent_submit_evidence(session, &inflam_ev);

                /* Portia resource evidence */
                training_evidence_t portia_ev = {
                    .source_name = "portia",
                    .type = TRAINING_EVIDENCE_CONTINUE,
                    .lr_factor = mod->portia_learning_gate,
                    .batch_factor = mod->portia_compute_budget,
                    .grad_clip_factor = 1.0f,
                    .urgency = mod->portia_compute_budget < 0.2f ? 0.6f : 0.1f,
                    .confidence = 0.85f,
                };
                training_convergent_submit_evidence(session, &portia_ev);

                /* Stress/capacity evidence */
                training_evidence_t stress_ev = {
                    .source_name = "stress",
                    .type = mod->stress_level > 0.8f ?
                            TRAINING_EVIDENCE_PAUSE : TRAINING_EVIDENCE_CONTINUE,
                    .lr_factor = 1.0f - 0.3f * mod->stress_level,
                    .batch_factor = 1.0f,
                    .grad_clip_factor = 1.0f,
                    .urgency = mod->stress_level > 0.8f ? 0.7f : 0.1f,
                    .confidence = 0.7f,
                };
                training_convergent_submit_evidence(session, &stress_ev);
        }

        /* Compute decision */
        training_convergent_decision_t decision;
        memset(&decision, 0, sizeof(decision));
        if (training_convergent_compute_decision(session, &decision) == 0) {
            result->consensus_action = decision.consensus_action;
            result->lr_factor = decision.lr_factor;
            result->batch_factor = decision.batch_factor;
            result->grad_clip_factor = decision.grad_clip_factor;
            result->urgency = decision.urgency;
            result->converged = decision.converged;
            result->num_contributors = decision.num_contributors;
        }

        training_convergent_session_destroy(session);
    }

    NIMCP_LOGGING_DEBUG("training_integration: decision cycle complete — "
                        "action=%d lr=%.4f batch=%.4f clip=%.4f "
                        "urgency=%.2f converged=%s diag='%.60s' "
                        "causal='%.60s'",
                        (int)result->consensus_action,
                        result->lr_factor, result->batch_factor,
                        result->grad_clip_factor, result->urgency,
                        result->converged ? "YES" : "NO",
                        result->primary_diagnosis,
                        result->causal_explanation);

    return 0;
}

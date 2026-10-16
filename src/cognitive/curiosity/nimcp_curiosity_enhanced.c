/**
 * @file nimcp_curiosity_enhanced.c
 * @brief Enhanced Curiosity System Implementation
 * @version 1.0.0
 * @date 2025-12-20
 *
 * WHAT: Implementation of 10 curiosity enhancements
 * WHY:  Complete biologically-inspired curiosity dynamics
 * HOW:  Modular sub-systems with unified interface
 *
 * @author NIMCP Development Team
 */

#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/* Include quantum bridge implementation */
#define NIMCP_CURIOSITY_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/curiosity/nimcp_curiosity_quantum_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "curiosity_enhanced"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(curiosity_enhanced, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Interest tracking entry for hash table
 */
typedef struct interest_entry_s {
    curiosity_topic_interest_t interest;
} interest_entry_t;

/**
 * @brief Recent stimulus for boredom detection
 */
typedef struct recent_stimulus_s {
    uint64_t hash;
    float novelty;
    uint64_t timestamp_ms;
} recent_stimulus_t;

#define MAX_RECENT_STIMULI 64

/**
 * @brief Enhanced curiosity system internal structure
 */
struct curiosity_enhanced_system_s {
    /* Configuration */
    curiosity_enhanced_config_t config;

    /* Base engine reference */
    curiosity_engine_t base_engine;

    /* State */
    curiosity_enhanced_state_t state;

    /* Statistics */
    curiosity_enhanced_stats_t stats;

    /* Interest tracking hash table */
    hash_table_t* interest_table;

    /* Recent stimuli for boredom detection */
    recent_stimulus_t recent_stimuli[MAX_RECENT_STIMULI];
    uint32_t num_recent_stimuli;
    uint32_t stimulus_write_idx;

    /* Connected systems */
    anxiety_system_t* anxiety_system;
    theory_of_mind_t* tom_system;

    /* Quantum bridge */
    curiosity_quantum_bridge_t* quantum_bridge;

    /* QMC integration */
    curiosity_qmc_config_t qmc_config;
    curiosity_qmc_stats_t qmc_stats;
    uint32_t qmc_seed;
    uint64_t total_topic_visits;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

/**
 * @brief Simple string hash
 */
static uint64_t hash_string(const char* str) {
    if (!str) return 0;
    uint64_t hash = 5381;
    int c = 0;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief Interest entry destructor for hash table
 */
static void interest_entry_destructor(void* value, size_t value_size) {
    (void)value_size;
    (void)value;
    /* No dynamic memory in interest_entry_t */
}

/* ============================================================================
 * Boredom Sub-system
 * ============================================================================ */

static void boredom_init(curiosity_boredom_state_t* state,
                         const curiosity_boredom_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->boredom_threshold = config ? config->boredom_threshold : BOREDOM_THRESHOLD_DEFAULT;
    state->novelty_seeking_boost = 1.0f;
    state->last_novel_event_ms = get_time_ms();
}

static void boredom_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_boredom_state_t* state = &sys->state.boredom;
    const curiosity_boredom_config_t* config = &sys->config.boredom;

    uint64_t now = get_time_ms();
    float time_since_novelty = (float)(now - state->last_novel_event_ms);

    /* Update understimulation duration */
    state->understimulation_duration_ms = time_since_novelty;

    /* Compute monotony based on repetition */
    if (state->repetition_count > 3) {
        state->monotony_level += config->monotony_decay_rate * dt_ms / 1000.0f;
    } else {
        state->monotony_level -= config->monotony_decay_rate * 2.0f * dt_ms / 1000.0f;
    }
    state->monotony_level = nimcp_clampf(state->monotony_level, 0.0f, 1.0f);

    /* Check understimulation timeout */
    float understim_factor = 0.0f;
    if (time_since_novelty > config->understimulation_timeout_ms) {
        understim_factor = (time_since_novelty - config->understimulation_timeout_ms) /
                          config->understimulation_timeout_ms;
        understim_factor = nimcp_clampf(understim_factor, 0.0f, 1.0f);
    }

    /* Combine factors for boredom */
    float boredom_level = (state->monotony_level + understim_factor) / 2.0f;

    /* Check if bored */
    bool was_bored = state->is_bored;
    state->is_bored = boredom_level > state->boredom_threshold;

    if (state->is_bored && !was_bored) {
        sys->stats.boredom_episodes++;
        NIMCP_LOGGING_DEBUG("Boredom triggered: monotony=%.2f, understim=%.2f",
                           state->monotony_level, understim_factor);
    }

    /* Compute novelty seeking boost */
    if (state->is_bored) {
        state->novelty_seeking_boost = 1.0f + (boredom_level - state->boredom_threshold) *
                                       config->novelty_boost_factor;
        state->novelty_seeking_boost = nimcp_clampf(state->novelty_seeking_boost,
                                              1.0f, BOREDOM_NOVELTY_SEEK_BOOST);
    } else {
        state->novelty_seeking_boost = 1.0f;
    }
}

static void boredom_report_stimulus(curiosity_enhanced_system_t* sys,
                                    uint64_t stimulus_hash, float novelty) {
    if (!sys) return;

    curiosity_boredom_state_t* state = &sys->state.boredom;
    uint64_t now = get_time_ms();

    /* Check if stimulus is repeated */
    bool is_repeat = false;
    for (uint32_t i = 0; i < sys->num_recent_stimuli; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->num_recent_stimuli > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)sys->num_recent_stimuli);
        }

        if (sys->recent_stimuli[i].hash == stimulus_hash) {
            is_repeat = true;
            break;
        }
    }

    /* Update repetition tracking */
    if (is_repeat) {
        state->repetition_count++;
    } else {
        state->repetition_count = 0;
    }

    /* Store stimulus */
    sys->recent_stimuli[sys->stimulus_write_idx].hash = stimulus_hash;
    sys->recent_stimuli[sys->stimulus_write_idx].novelty = novelty;
    sys->recent_stimuli[sys->stimulus_write_idx].timestamp_ms = now;
    sys->stimulus_write_idx = (sys->stimulus_write_idx + 1) % MAX_RECENT_STIMULI;
    if (sys->num_recent_stimuli < MAX_RECENT_STIMULI) {
        sys->num_recent_stimuli++;
    }

    /* Update novelty tracking */
    state->last_stimulus_novelty = novelty;
    if (novelty > 0.5f) {
        state->last_novel_event_ms = now;
        state->is_bored = false;
        state->monotony_level *= 0.5f;  /* Reset monotony on novelty */
        sys->stats.novelty_events++;
    }
}

/* ============================================================================
 * Interest Decay Sub-system
 * ============================================================================ */

static curiosity_topic_interest_t* interest_get_or_create(
    curiosity_enhanced_system_t* sys, const char* topic) {

    if (!sys || !topic || !sys->interest_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "interest_get_or_create: required parameter is NULL (sys, topic, sys->interest_table)");
        return NULL;
    }

    /* Look up existing */
    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        sys->interest_table, topic);

    if (entry) {
        return &entry->interest;
    }

    /* Create new entry */
    interest_entry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    strncpy(new_entry.interest.topic, topic, sizeof(new_entry.interest.topic) - 1);
    new_entry.interest.initial_interest = 1.0f;
    new_entry.interest.current_interest = 1.0f;
    new_entry.interest.peak_interest = 1.0f;
    new_entry.interest.decay_rate = sys->config.interest.base_decay_rate;
    new_entry.interest.first_exposure_ms = get_time_ms();
    new_entry.interest.last_exposure_ms = new_entry.interest.first_exposure_ms;
    new_entry.interest.exposure_count = 1;

    bool success = hash_table_insert_string(sys->interest_table, topic,
                                            &new_entry, sizeof(new_entry));
    if (!success) {
        NIMCP_LOGGING_ERROR("Failed to create interest entry for topic: %s", topic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "interest_get_or_create: hash table insert failed");
        return NULL;
    }

    entry = (interest_entry_t*)hash_table_lookup_string(sys->interest_table, topic);
    return entry ? &entry->interest : NULL;
}

static float interest_compute_decay(const curiosity_interest_config_t* config,
                                    const curiosity_topic_interest_t* interest,
                                    uint64_t now_ms) {
    if (!config || !interest) return 1.0f;

    float time_since_last = (float)(now_ms - interest->last_exposure_ms);

    if (config->enable_hyperbolic_decay) {
        /* Hyperbolic decay: I(t) = I0 / (1 + k*t) */
        float k = interest->decay_rate / 1000.0f;
        return interest->initial_interest / (1.0f + k * time_since_last);
    } else {
        /* Exponential decay: I(t) = I0 * exp(-k*t) */
        float half_life = (float)config->half_life_ms;
        float decay = powf(0.5f, time_since_last / half_life);
        return interest->initial_interest * decay;
    }
}

static float interest_compute_satiation(const curiosity_topic_interest_t* interest) {
    if (!interest) return 0.0f;

    /* Satiation based on exposure count */
    float satiation = 1.0f - expf(-(float)interest->exposure_count / 10.0f);
    return nimcp_clampf(satiation, 0.0f, 1.0f);
}

/* ============================================================================
 * Curiosity Types Sub-system
 * ============================================================================ */

static void types_init(curiosity_type_profile_t* profile,
                       const curiosity_type_config_t* config) {
    if (!profile) return;

    memset(profile, 0, sizeof(*profile));
    profile->dominant_type = CURIOSITY_TYPE_EPISTEMIC;  /* Default to knowledge-seeking */

    /* Initialize intensities from config weights */
    if (config) {
        for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
            }

            profile->type_intensities[i] = config->type_weights[i];
        }
    } else {
        /* Default equal distribution */
        for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
            }

            profile->type_intensities[i] = 1.0f / (float)CURIOSITY_TYPE_COUNT;
        }
    }

    profile->last_transition_ms = get_time_ms();
}

static void types_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_type_profile_t* profile = &sys->state.types;

    /* Find dominant type */
    float max_intensity = 0.0f;
    curiosity_type_t new_dominant = profile->dominant_type;

    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
        }

        if (profile->type_intensities[i] > max_intensity) {
            max_intensity = profile->type_intensities[i];
            new_dominant = (curiosity_type_t)i;
        }
    }

    /* Check for transition */
    if (new_dominant != profile->dominant_type) {
        uint64_t now = get_time_ms();
        profile->type_durations_ms[profile->dominant_type] +=
            now - profile->last_transition_ms;
        profile->dominant_type = new_dominant;
        profile->last_transition_ms = now;
        sys->stats.type_transitions++;
    }

    /* Normalize intensities */
    float total = 0.0f;
    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
        }

        total += profile->type_intensities[i];
    }
    if (total > 0.0f) {
        for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
            }

            profile->type_intensities[i] /= total;
        }
    }
}

/* ============================================================================
 * Approach-Avoidance Sub-system
 * ============================================================================ */

static void anxiety_balance_init(curiosity_approach_avoidance_t* state,
                                  const curiosity_anxiety_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->approach_tendency = 0.5f;
    state->avoidance_tendency = 0.0f;
    state->resolution_bias = config ? config->approach_bias : 0.6f;
}

static void anxiety_balance_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_approach_avoidance_t* state = &sys->state.anxiety_balance;

    /* Compute net motivation */
    state->net_motivation = state->approach_tendency - state->avoidance_tendency;

    /* Compute conflict */
    float min_tendency = fminf(state->approach_tendency, state->avoidance_tendency);
    state->conflict_level = min_tendency * 2.0f;  /* High when both are high */

    bool was_in_conflict = state->in_conflict;
    state->in_conflict = state->conflict_level > 0.5f;

    if (state->in_conflict && !was_in_conflict) {
        state->conflict_start_ms = get_time_ms();
        sys->stats.approach_avoidance_conflicts++;
    }

    /* Resolve conflict over time */
    if (state->in_conflict) {
        float resolution_rate = sys->config.anxiety.conflict_resolution_rate * dt_ms / 1000.0f;
        if (state->resolution_bias > 0.5f) {
            state->avoidance_tendency -= resolution_rate;
        } else {
            state->approach_tendency -= resolution_rate;
        }
        state->approach_tendency = nimcp_clampf(state->approach_tendency, 0.0f, 1.0f);
        state->avoidance_tendency = nimcp_clampf(state->avoidance_tendency, 0.0f, 1.0f);
    }
}

/* ============================================================================
 * Social Curiosity Sub-system
 * ============================================================================ */

static void social_init(curiosity_social_state_t* state,
                        const curiosity_social_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->gossip_interest = 0.5f;
    state->reputation_tracking = 0.5f;
    state->coalition_mapping = 0.3f;
    state->deception_detection = 0.3f;
}

static curiosity_social_target_t* social_get_or_create_target(
    curiosity_social_state_t* state, const char* agent_id) {

    if (!state || !agent_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_get_or_create_target: required parameter is NULL (state, agent_id)");
        return NULL;
    }

    /* Find existing */
    for (uint32_t i = 0; i < state->num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_targets > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)state->num_targets);
        }

        if (strcmp(state->targets[i].agent_id, agent_id) == 0) {
            return &state->targets[i];
        }
    }

    /* Create new if space available */
    if (state->num_targets >= MAX_SOCIAL_TARGETS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_get_or_create_target: capacity exceeded");
        return NULL;
    }

    curiosity_social_target_t* target = &state->targets[state->num_targets++];
    memset(target, 0, sizeof(*target));
    strncpy(target->agent_id, agent_id, sizeof(target->agent_id) - 1);
    target->social_interest = 0.5f;
    target->trust_level = 0.5f;
    target->is_tracked = true;

    return target;
}

static void social_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_social_state_t* state = &sys->state.social;
    const curiosity_social_config_t* config = &sys->config.social;

    /* Decay gossip interest */
    state->gossip_interest -= config->gossip_decay_rate * dt_ms / 1000.0f;
    state->gossip_interest = nimcp_clampf(state->gossip_interest, 0.1f, 1.0f);

    /* Decay social interest in targets */
    uint64_t now = get_time_ms();
    for (uint32_t i = 0; i < state->num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_targets > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)state->num_targets);
        }

        float time_since = (float)(now - state->targets[i].last_interaction_ms);
        float decay = expf(-time_since / 3600000.0f);  /* 1 hour half-life */
        state->targets[i].social_interest *= decay;
    }
}

/* ============================================================================
 * Meta-Curiosity Sub-system
 * ============================================================================ */

static void meta_init(curiosity_meta_state_t* state,
                      const curiosity_meta_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->self_awareness_of_interests = 0.3f;
    state->curiosity_pattern_recognition = 0.2f;
    state->learning_strategy_awareness = 0.2f;
    state->meta_curiosity_level = 0.3f;
}

static void meta_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_meta_state_t* state = &sys->state.meta;
    const curiosity_meta_config_t* config = &sys->config.meta;

    /* Grow meta-awareness over time */
    state->self_awareness_of_interests += config->meta_awareness_growth_rate * dt_ms / 1000.0f;
    state->self_awareness_of_interests = nimcp_clampf(state->self_awareness_of_interests, 0.0f, 1.0f);

    /* Check for automatic introspection */
    uint64_t now = get_time_ms();
    if (config->enable_automatic_introspection &&
        (now - state->last_introspection_ms) > (uint64_t)config->introspection_frequency_ms) {
        state->last_introspection_ms = now;
        sys->stats.introspection_events++;

        /* Simple meta-curiosity computation */
        state->meta_curiosity_level = state->self_awareness_of_interests * 0.5f +
                                      state->curiosity_pattern_recognition * 0.3f +
                                      state->learning_strategy_awareness * 0.2f;
    }
}

/* ============================================================================
 * Contagion Sub-system
 * ============================================================================ */

static void contagion_init(curiosity_contagion_state_t* state,
                           const curiosity_contagion_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->contagion_susceptibility = config ? config->base_susceptibility :
                                      CONTAGION_SUSCEPTIBILITY_DEFAULT;
}

static void contagion_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_contagion_state_t* state = &sys->state.contagion;
    const curiosity_contagion_config_t* config = &sys->config.contagion;

    /* Decay accumulated contagion */
    state->accumulated_contagion -= config->contagion_decay_rate * dt_ms / 1000.0f;
    state->accumulated_contagion = nimcp_clampf(state->accumulated_contagion, 0.0f, 1.0f);
}

/* ============================================================================
 * Surprise Learning Sub-system
 * ============================================================================ */

static void surprise_init(curiosity_surprise_learning_t* state,
                          const curiosity_surprise_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->learning_rate_boost = 1.0f;
}

static void surprise_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_surprise_learning_t* state = &sys->state.surprise;
    const curiosity_surprise_config_t* config = &sys->config.surprise;

    /* Decay surprise effects */
    state->surprise_magnitude -= config->surprise_decay_rate * dt_ms / 1000.0f;
    state->surprise_magnitude = nimcp_clampf(state->surprise_magnitude, 0.0f, 1.0f);

    /* Update learning rate boost */
    if (state->surprise_magnitude > config->surprise_threshold) {
        float excess = state->surprise_magnitude - config->surprise_threshold;
        state->learning_rate_boost = 1.0f + excess * (config->max_lr_boost - 1.0f);
    } else {
        state->learning_rate_boost = 1.0f;
    }
}

static float surprise_report(curiosity_enhanced_system_t* sys,
                             float prediction_error, const char* context) {
    if (!sys) return 1.0f;

    curiosity_surprise_learning_t* state = &sys->state.surprise;
    const curiosity_surprise_config_t* config = &sys->config.surprise;

    state->prediction_error = prediction_error;
    state->surprise_magnitude = nimcp_clampf(prediction_error, 0.0f, 1.0f);
    state->last_surprise_ms = get_time_ms();
    state->surprise_count++;

    /* Update running average */
    float alpha = 0.1f;
    float new_avg_surprise = alpha * state->surprise_magnitude +
                             (1.0f - alpha) * state->avg_surprise;
    if (isfinite(new_avg_surprise)) {
        state->avg_surprise = new_avg_surprise;
    }

    /* Compute boost */
    if (state->surprise_magnitude > config->surprise_threshold) {
        float excess = state->surprise_magnitude - config->surprise_threshold;
        state->learning_rate_boost = 1.0f + excess * (config->max_lr_boost - 1.0f);
        state->learning_rate_boost = nimcp_clampf(state->learning_rate_boost, 1.0f, config->max_lr_boost);

        state->memory_consolidation_priority = state->surprise_magnitude;
        state->attention_capture_strength = state->surprise_magnitude * 1.5f;

        sys->stats.surprise_events++;
        NIMCP_LOGGING_DEBUG("Surprise event: error=%.2f, boost=%.2f",
                           prediction_error, state->learning_rate_boost);
    }

    return state->learning_rate_boost;
}

/* ============================================================================
 * Fatigue Sub-system
 * ============================================================================ */

static void fatigue_init(curiosity_fatigue_state_t* state,
                         const curiosity_fatigue_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->rest_threshold = config ? config->rest_threshold : FATIGUE_REST_THRESHOLD;
    state->recovery_rate = config ? config->base_recovery_rate : FATIGUE_RECOVERY_RATE;
}

static void fatigue_update(curiosity_enhanced_system_t* sys, float dt_ms) {
    if (!sys) return;

    curiosity_fatigue_state_t* state = &sys->state.fatigue;
    const curiosity_fatigue_config_t* config = &sys->config.fatigue;

    if (state->is_resting) {
        /* Recovery mode */
        state->exploration_fatigue -= config->base_recovery_rate * dt_ms / 1000.0f;
        state->cognitive_depletion -= config->base_recovery_rate * dt_ms / 1000.0f;
        state->total_rest_ms += (uint64_t)dt_ms;

        state->exploration_fatigue = nimcp_clampf(state->exploration_fatigue, 0.0f, 1.0f);
        state->cognitive_depletion = nimcp_clampf(state->cognitive_depletion, 0.0f, 1.0f);

        /* Check if recovered enough */
        if (state->exploration_fatigue < 0.2f) {
            state->is_resting = false;
        }
    } else {
        /* Active mode - accumulate fatigue */
        state->exploration_fatigue += config->fatigue_accumulation_rate * dt_ms / 1000.0f;
        state->cognitive_depletion += config->fatigue_accumulation_rate * 0.5f * dt_ms / 1000.0f;
        state->total_exploration_ms += (uint64_t)dt_ms;

        state->exploration_fatigue = nimcp_clampf(state->exploration_fatigue, 0.0f, 1.0f);
        state->cognitive_depletion = nimcp_clampf(state->cognitive_depletion, 0.0f, 1.0f);

        /* Check if rest is needed */
        state->needs_rest = state->exploration_fatigue > state->rest_threshold;

        if (state->needs_rest && config->enable_auto_rest) {
            state->is_resting = true;
            state->last_rest_period_ms = get_time_ms();
            sys->stats.fatigue_rest_periods++;
        }
    }
}

/* ============================================================================
 * Counterfactual Sub-system
 * ============================================================================ */

static void counterfactual_init(curiosity_counterfactual_state_t* state,
                                const curiosity_counterfactual_config_t* config) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->counterfactual_curiosity = 0.3f;
    state->regret_sensitivity = 0.5f;
}

static int counterfactual_generate(curiosity_enhanced_system_t* sys,
                                   const char* decision_point,
                                   const char* actual_outcome,
                                   curiosity_counterfactual_t* cf) {
    if (!sys || !decision_point || !actual_outcome || !cf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counterfactual_generate: required parameter is NULL (sys, decision_point, actual_outcome, cf)");
        return -1;
    }

    curiosity_counterfactual_state_t* state = &sys->state.counterfactual;

    if (state->num_items >= MAX_COUNTERFACTUALS) {
        /* Replace oldest */
        memmove(&state->items[0], &state->items[1],
                (MAX_COUNTERFACTUALS - 1) * sizeof(curiosity_counterfactual_t));
        state->num_items = MAX_COUNTERFACTUALS - 1;
    }

    memset(cf, 0, sizeof(*cf));
    strncpy(cf->actual_outcome, actual_outcome, sizeof(cf->actual_outcome) - 1);
    snprintf(cf->counterfactual_question, sizeof(cf->counterfactual_question),
             "What if I had done differently at: %s?", decision_point);
    cf->decision_time_ms = get_time_ms();
    cf->learning_value = 0.5f;
    cf->exploration_cost = COUNTERFACTUAL_EXPLORATION_COST;

    /* Copy to state */
    state->items[state->num_items++] = *cf;
    sys->stats.counterfactuals_generated++;

    return 0;
}

/* ============================================================================
 * Aggregate Functions
 * ============================================================================ */

static void compute_aggregates(curiosity_enhanced_system_t* sys) {
    if (!sys) return;

    curiosity_enhanced_state_t* state = &sys->state;

    /* Combine all factors for overall drive */
    float boredom_factor = state->boredom.is_bored ?
                          state->boredom.novelty_seeking_boost : 1.0f;
    float anxiety_factor = nimcp_clampf(1.0f - state->anxiety_balance.avoidance_tendency, 0.2f, 1.0f);
    float fatigue_factor = nimcp_clampf(1.0f - state->fatigue.exploration_fatigue, 0.1f, 1.0f);
    float surprise_factor = state->surprise.learning_rate_boost;

    /* Get base curiosity from dominant type */
    float type_intensity = state->types.type_intensities[state->types.dominant_type];

    /* Combine */
    state->overall_curiosity_drive = type_intensity * boredom_factor * anxiety_factor *
                                     fatigue_factor * (0.5f + surprise_factor * 0.5f);
    state->overall_curiosity_drive = nimcp_clampf(state->overall_curiosity_drive, 0.0f, 1.0f);

    state->effective_exploration_rate = state->overall_curiosity_drive;
    if (state->fatigue.is_resting) {
        state->effective_exploration_rate *= 0.1f;
    }

    /* Update average stats with NaN/Inf guard */
    float alpha = 0.01f;
    float new_curiosity = alpha * state->overall_curiosity_drive +
                          (1.0f - alpha) * sys->stats.avg_curiosity_level;
    float new_boredom = alpha * state->boredom.monotony_level +
                        (1.0f - alpha) * sys->stats.avg_boredom_level;
    float new_fatigue = alpha * state->fatigue.exploration_fatigue +
                        (1.0f - alpha) * sys->stats.avg_fatigue_level;
    if (isfinite(new_curiosity)) sys->stats.avg_curiosity_level = new_curiosity;
    if (isfinite(new_boredom))   sys->stats.avg_boredom_level = new_boredom;
    if (isfinite(new_fatigue))   sys->stats.avg_fatigue_level = new_fatigue;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void curiosity_enhanced_config_default(curiosity_enhanced_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_config_default", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Boredom config */
    config->boredom.boredom_threshold = BOREDOM_THRESHOLD_DEFAULT;
    config->boredom.monotony_decay_rate = BOREDOM_MONOTONY_DECAY;
    config->boredom.novelty_boost_factor = BOREDOM_NOVELTY_SEEK_BOOST - 1.0f;
    config->boredom.understimulation_timeout_ms = 60000.0f;  /* 1 minute */
    config->boredom.enable_auto_novelty_seek = true;

    /* Interest config */
    config->interest.base_decay_rate = INTEREST_DECAY_RATE_DEFAULT;
    config->interest.satiation_threshold = SATIATION_THRESHOLD;
    config->interest.half_life_ms = INTEREST_HALF_LIFE_MS;
    config->interest.residual_interest_min = 0.1f;
    config->interest.enable_hyperbolic_decay = true;

    /* Types config */
    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
        }

        config->types.type_weights[i] = 1.0f / (float)CURIOSITY_TYPE_COUNT;
    }
    config->types.transition_threshold = 0.3f;
    config->types.enable_dynamic_switching = true;

    /* Anxiety config */
    config->anxiety.anxiety_suppress_threshold = ANXIETY_SUPPRESS_THRESHOLD;
    config->anxiety.approach_bias = 0.6f;
    config->anxiety.conflict_resolution_rate = 0.1f;
    config->anxiety.enable_gradual_exposure = true;

    /* Social config */
    config->social.gossip_decay_rate = GOSSIP_INTEREST_DECAY;
    config->social.trust_influence = 0.5f;
    config->social.coalition_update_rate = 0.01f;
    config->social.enable_deception_vigilance = true;

    /* Meta config */
    config->meta.introspection_frequency_ms = 60000.0f;
    config->meta.blind_spot_detection_threshold = 0.2f;
    config->meta.meta_awareness_growth_rate = 0.001f;
    config->meta.enable_automatic_introspection = true;

    /* Contagion config */
    config->contagion.base_susceptibility = CONTAGION_SUSCEPTIBILITY_DEFAULT;
    config->contagion.trust_susceptibility_factor = 0.5f;
    config->contagion.contagion_decay_rate = CONTAGION_DECAY_RATE;
    config->contagion.enable_selective_contagion = true;

    /* Surprise config */
    config->surprise.surprise_threshold = SURPRISE_THRESHOLD_DEFAULT;
    config->surprise.max_lr_boost = SURPRISE_LR_BOOST_MAX;
    config->surprise.surprise_decay_rate = 0.1f;
    config->surprise.memory_priority_factor = 1.5f;

    /* Fatigue config */
    config->fatigue.fatigue_accumulation_rate = FATIGUE_ACCUMULATION_RATE;
    config->fatigue.base_recovery_rate = FATIGUE_RECOVERY_RATE;
    config->fatigue.rest_threshold = FATIGUE_REST_THRESHOLD;
    config->fatigue.min_rest_duration_ms = 10000.0f;
    config->fatigue.enable_auto_rest = false;

    /* Counterfactual config */
    config->counterfactual.regret_threshold = 0.5f;
    config->counterfactual.exploration_cost_limit = 0.3f;
    config->counterfactual.learning_value_threshold = 0.3f;
    config->counterfactual.enable_automatic_generation = true;

    /* Global config */
    config->update_interval_ms = 100.0f;
    config->enable_bio_async = true;
    config->enable_all_enhancements = true;
    config->enable_quantum_curiosity = true;
}

curiosity_enhanced_system_t* curiosity_enhanced_create(
    const curiosity_enhanced_config_t* config,
    curiosity_engine_t base_engine) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_create", 0.0f);


    curiosity_enhanced_system_t* sys = (curiosity_enhanced_system_t*)
        nimcp_calloc(1, sizeof(curiosity_enhanced_system_t));

    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate enhanced curiosity system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_enhanced_create: sys is NULL");
        return NULL;
    }

    /* Store config */
    if (config) {
        sys->config = *config;
    } else {
        curiosity_enhanced_config_default(&sys->config);
    }

    sys->base_engine = base_engine;

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_enhanced_config_default: sys->mutex is NULL");
        return NULL;
    }

    /* Create interest hash table */
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .value_destructor = interest_entry_destructor,
        .case_insensitive = false,
        .thread_safe = false
    };

    sys->interest_table = hash_table_create(&ht_config);
    if (!sys->interest_table) {
        NIMCP_LOGGING_ERROR("Failed to create interest table");
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_enhanced_config_default: sys->interest_table is NULL");
        return NULL;
    }

    /* Initialize sub-systems */
    boredom_init(&sys->state.boredom, &sys->config.boredom);
    types_init(&sys->state.types, &sys->config.types);
    anxiety_balance_init(&sys->state.anxiety_balance, &sys->config.anxiety);
    social_init(&sys->state.social, &sys->config.social);
    meta_init(&sys->state.meta, &sys->config.meta);
    contagion_init(&sys->state.contagion, &sys->config.contagion);
    surprise_init(&sys->state.surprise, &sys->config.surprise);
    fatigue_init(&sys->state.fatigue, &sys->config.fatigue);
    counterfactual_init(&sys->state.counterfactual, &sys->config.counterfactual);

    sys->creation_time_ms = get_time_ms();
    sys->last_update_ms = sys->creation_time_ms;

    /* Create quantum bridge if enabled */
    if (sys->config.enable_quantum_curiosity) {
        curiosity_quantum_config_t quantum_config;
        curiosity_quantum_default_config(&quantum_config);

        sys->quantum_bridge = curiosity_quantum_create(&quantum_config);
        if (!sys->quantum_bridge) {
            NIMCP_LOGGING_WARN("Failed to create quantum bridge, continuing without quantum exploration");
        } else {
            NIMCP_LOGGING_INFO("Quantum curiosity bridge enabled (max_topics=%u, steps=%u)",
                              quantum_config.max_topics, quantum_config.exploration_steps);
        }
    }

    /* Initialize QMC configuration */
    curiosity_enhanced_qmc_default_config(&sys->qmc_config);
    memset(&sys->qmc_stats, 0, sizeof(sys->qmc_stats));
    sys->qmc_seed = (uint32_t)sys->creation_time_ms;
    sys->total_topic_visits = 0;

    NIMCP_LOGGING_INFO("Created enhanced curiosity system with %d enhancements%s",
                       sys->config.enable_all_enhancements ? 10 : 0,
                       sys->quantum_bridge ? " + quantum exploration" : "");

    return sys;
}

void curiosity_enhanced_destroy(curiosity_enhanced_system_t* system) {
    if (!system) return;

    /* Disconnect bio-async */
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_destroy", 0.0f);


    if (system->bio_async_enabled) {
        curiosity_enhanced_disconnect_bio_async(system);
    }

    /* Destroy quantum bridge */
    if (system->quantum_bridge) {
        curiosity_quantum_destroy(system->quantum_bridge);
    }

    /* Free blind spots */
    for (uint32_t i = 0; i < system->state.meta.num_blind_spots; i++) {
        if (system->state.meta.identified_blind_spots[i]) {
            nimcp_free(system->state.meta.identified_blind_spots[i]);
        }
    }

    /* Destroy hash table */
    if (system->interest_table) {
        hash_table_destroy(system->interest_table);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    system = NULL;
    NIMCP_LOGGING_INFO("Destroyed enhanced curiosity system");
}

int curiosity_enhanced_update(curiosity_enhanced_system_t* system, float dt_ms) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_update", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Update all sub-systems */
    boredom_update(system, dt_ms);
    types_update(system, dt_ms);
    anxiety_balance_update(system, dt_ms);
    social_update(system, dt_ms);
    meta_update(system, dt_ms);
    contagion_update(system, dt_ms);
    surprise_update(system, dt_ms);
    fatigue_update(system, dt_ms);

    /* Compute aggregates */
    compute_aggregates(system);

    system->state.last_update_ms = get_time_ms();
    system->last_update_ms = system->state.last_update_ms;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* Boredom functions */
bool curiosity_enhanced_is_bored(
    const curiosity_enhanced_system_t* system,
    curiosity_boredom_state_t* state) {

    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_is_bored", 0.0f);

    /* Thread-safe read: lock, copy, unlock */
    nimcp_platform_mutex_lock(system->mutex);
    curiosity_boredom_state_t local_boredom = system->state.boredom;
    nimcp_platform_mutex_unlock(system->mutex);

    if (state) {
        *state = local_boredom;
    }

    return local_boredom.is_bored;
}

int curiosity_enhanced_report_stimulus(
    curiosity_enhanced_system_t* system,
    uint64_t stimulus_hash,
    float novelty) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_report_stimulus", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    boredom_report_stimulus(system, stimulus_hash, novelty);
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

float curiosity_enhanced_get_boredom_boost(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_boredom_boost", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float boost = system->state.boredom.novelty_seeking_boost;
    nimcp_platform_mutex_unlock(system->mutex);

    return boost;
}

/* Interest decay functions */
float curiosity_enhanced_get_topic_interest(
    const curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_topic_interest", 0.0f);

    /* Thread-safe read: lock during hash table access */
    nimcp_platform_mutex_lock(system->mutex);

    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    if (!entry) {
        nimcp_platform_mutex_unlock(system->mutex);
        return 1.0f;  /* Unknown topic = full interest */
    }

    float result = interest_compute_decay(&system->config.interest,
                                          &entry->interest, get_time_ms());
    nimcp_platform_mutex_unlock(system->mutex);

    return result;
}

int curiosity_enhanced_record_exposure(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float learning_value) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_record_exposure", 0.0f);


    NIMCP_CHECK_THROW(system && topic, NIMCP_ERROR_NULL_ARG, "system or topic is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    curiosity_topic_interest_t* interest = interest_get_or_create(system, topic);
    if (!interest) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR;
    }

    interest->exposure_count++;
    interest->last_exposure_ms = get_time_ms();
    interest->satiation_level += learning_value * 0.1f;
    interest->satiation_level = nimcp_clampf(interest->satiation_level, 0.0f, 1.0f);

    if (interest->satiation_level > system->config.interest.satiation_threshold) {
        system->stats.interest_satiation_events++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

float curiosity_enhanced_compute_satiation(
    const curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_compute_satiation", 0.0f);

    /* Thread-safe read: lock during hash table access */
    nimcp_platform_mutex_lock(system->mutex);

    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    if (!entry) {
        nimcp_platform_mutex_unlock(system->mutex);
        return 0.0f;
    }

    float result = interest_compute_satiation(&entry->interest);
    nimcp_platform_mutex_unlock(system->mutex);

    return result;
}

float curiosity_enhanced_get_residual_interest(
    const curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_residual_interes", 0.0f);


    float current = curiosity_enhanced_get_topic_interest(system, topic);
    float satiation = curiosity_enhanced_compute_satiation(system, topic);
    float residual = current * (1.0f - satiation * 0.8f);

    return fmaxf(residual, system->config.interest.residual_interest_min);
}

/* Curiosity type functions */
curiosity_type_t curiosity_enhanced_get_dominant_type(
    const curiosity_enhanced_system_t* system) {

    if (!system) return CURIOSITY_TYPE_EPISTEMIC;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_dominant_type", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    curiosity_type_t type = system->state.types.dominant_type;
    nimcp_platform_mutex_unlock(system->mutex);

    return type;
}

int curiosity_enhanced_get_type_profile(
    const curiosity_enhanced_system_t* system,
    curiosity_type_profile_t* profile) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_type_profile", 0.0f);

    NIMCP_CHECK_THROW(system && profile, NIMCP_ERROR_NULL_ARG, "system or profile is NULL");

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    *profile = system->state.types;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int curiosity_enhanced_set_type_intensity(
    curiosity_enhanced_system_t* system,
    curiosity_type_t type,
    float intensity) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_set_type_intensity", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(type >= 0 && type < CURIOSITY_TYPE_COUNT, NIMCP_ERROR_INVALID, "invalid curiosity type");

    nimcp_platform_mutex_lock(system->mutex);
    system->state.types.type_intensities[type] = nimcp_clampf(intensity, 0.0f, 1.0f);
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int curiosity_enhanced_transition_type(
    curiosity_enhanced_system_t* system,
    curiosity_type_t new_type) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_transition_type", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    NIMCP_CHECK_THROW(new_type >= 0 && new_type < CURIOSITY_TYPE_COUNT, NIMCP_ERROR_INVALID, "invalid new curiosity type");

    nimcp_platform_mutex_lock(system->mutex);

    /* Boost new type, reduce others */
    for (int i = 0; i < CURIOSITY_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && CURIOSITY_TYPE_COUNT > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)CURIOSITY_TYPE_COUNT);
        }

        if (i == new_type) {
            system->state.types.type_intensities[i] = 0.5f;
        } else {
            system->state.types.type_intensities[i] *= 0.7f;
        }
    }

    system->state.types.dominant_type = new_type;
    system->state.types.last_transition_ms = get_time_ms();
    system->stats.type_transitions++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* Anxiety balance functions */
int curiosity_enhanced_connect_anxiety(
    curiosity_enhanced_system_t* system,
    anxiety_system_t* anxiety) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_connect_anxiety", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    system->anxiety_system = anxiety;
    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Connected anxiety system to enhanced curiosity");
    return 0;
}

float curiosity_enhanced_get_net_motivation(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_net_motivation", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float motivation = system->state.anxiety_balance.net_motivation;
    nimcp_platform_mutex_unlock(system->mutex);

    return motivation;
}

bool curiosity_enhanced_should_explore(
    const curiosity_enhanced_system_t* system,
    float threat_level) {

    if (!system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_should_explore", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float avoidance = system->state.anxiety_balance.avoidance_tendency;
    float approach = system->state.anxiety_balance.approach_tendency;
    nimcp_platform_mutex_unlock(system->mutex);

    float adjusted_avoidance = avoidance + threat_level;
    float net = approach - adjusted_avoidance;

    return net > 0.0f;
}

int curiosity_enhanced_report_conflict_resolution(
    curiosity_enhanced_system_t* system,
    bool approach_won) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_report_conflict_reso", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    if (approach_won) {
        system->state.anxiety_balance.approach_wins++;
        system->state.anxiety_balance.resolution_bias += 0.01f;
    } else {
        system->state.anxiety_balance.avoidance_wins++;
        system->state.anxiety_balance.resolution_bias -= 0.01f;
    }

    system->state.anxiety_balance.resolution_bias =
        nimcp_clampf(system->state.anxiety_balance.resolution_bias, 0.0f, 1.0f);
    system->state.anxiety_balance.in_conflict = false;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* Social curiosity functions */
int curiosity_enhanced_connect_tom(
    curiosity_enhanced_system_t* system,
    theory_of_mind_t* tom) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_connect_tom", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    system->tom_system = tom;
    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Connected ToM system to enhanced curiosity");
    return 0;
}

float curiosity_enhanced_assess_social_target(
    curiosity_enhanced_system_t* system,
    const char* agent_id,
    curiosity_social_target_t* target) {

    if (!system || !agent_id) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_assess_social_target", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    curiosity_social_target_t* t = social_get_or_create_target(
        &system->state.social, agent_id);

    if (!t) {
        nimcp_platform_mutex_unlock(system->mutex);
        return 0.0f;
    }

    if (target) {
        *target = *t;
    }

    float result = t->social_interest;
    nimcp_platform_mutex_unlock(system->mutex);

    return result;
}

int curiosity_enhanced_record_social_interaction(
    curiosity_enhanced_system_t* system,
    const char* agent_id,
    float info_gained) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_record_social_intera", 0.0f);


    NIMCP_CHECK_THROW(system && agent_id, NIMCP_ERROR_NULL_ARG, "system or agent_id is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    curiosity_social_target_t* t = social_get_or_create_target(
        &system->state.social, agent_id);

    if (t) {
        t->interaction_count++;
        t->last_interaction_ms = get_time_ms();
        t->information_value = info_gained;
        t->social_interest += info_gained * 0.1f;
        t->social_interest = nimcp_clampf(t->social_interest, 0.0f, 1.0f);
    }

    system->stats.social_curiosity_events++;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

float curiosity_enhanced_get_gossip_interest(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_gossip_interest", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float gossip = system->state.social.gossip_interest;
    nimcp_platform_mutex_unlock(system->mutex);

    return gossip;
}

/* Meta-curiosity functions */
int curiosity_enhanced_introspect(
    curiosity_enhanced_system_t* system,
    curiosity_meta_state_t* state) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_introspect", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    system->state.meta.last_introspection_ms = get_time_ms();
    system->state.meta.introspection_depth += 0.1f;
    system->state.meta.introspection_depth =
        nimcp_clampf(system->state.meta.introspection_depth, 0.0f, 1.0f);

    /* Update self-awareness based on introspection */
    system->state.meta.self_awareness_of_interests += 0.05f;
    system->state.meta.self_awareness_of_interests =
        nimcp_clampf(system->state.meta.self_awareness_of_interests, 0.0f, 1.0f);

    system->stats.introspection_events++;

    if (state) {
        *state = system->state.meta;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

uint32_t curiosity_enhanced_identify_blind_spots(
    curiosity_enhanced_system_t* system) {

    if (!system) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_identify_blind_spots", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    /* Simple blind spot detection: types with low intensity */
    uint32_t count = 0;
    for (int i = 0; i < CURIOSITY_TYPE_COUNT && count < MAX_BLIND_SPOTS; i++) {
        if (system->state.types.type_intensities[i] <
            system->config.meta.blind_spot_detection_threshold) {

            const char* type_str = curiosity_type_to_string((curiosity_type_t)i);
            if (system->state.meta.identified_blind_spots[count]) {
                nimcp_free(system->state.meta.identified_blind_spots[count]);
            }
            size_t alloc_size = strlen(type_str) + 32;
            system->state.meta.identified_blind_spots[count] =
                (char*)nimcp_malloc(alloc_size);
            if (system->state.meta.identified_blind_spots[count]) {
                snprintf(system->state.meta.identified_blind_spots[count], alloc_size,
                        "Low curiosity in: %s", type_str);
                count++;
            }
        }
    }

    system->state.meta.num_blind_spots = count;
    nimcp_platform_mutex_unlock(system->mutex);

    return count;
}

float curiosity_enhanced_get_meta_curiosity(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_meta_curiosity", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float level = system->state.meta.meta_curiosity_level;
    nimcp_platform_mutex_unlock(system->mutex);

    return level;
}

/* Contagion functions */
bool curiosity_enhanced_observe_curiosity(
    curiosity_enhanced_system_t* system,
    const curiosity_contagion_event_t* event) {

    if (!system || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, event)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_observe_curiosity", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    curiosity_contagion_state_t* state = &system->state.contagion;

    /* Compute contagion strength */
    float strength = event->observed_curiosity_intensity * state->contagion_susceptibility;

    /* Add to recent events */
    if (state->num_recent_events < 16) {
        state->recent_events[state->num_recent_events++] = *event;
    }

    state->accumulated_contagion += strength;
    state->accumulated_contagion = nimcp_clampf(state->accumulated_contagion, 0.0f, 1.0f);
    state->total_contagion_events++;

    /* Adopt curiosity if strong enough */
    bool adopted = strength > 0.3f;
    if (adopted) {
        state->curiosities_adopted++;

        /* Boost epistemic curiosity for observed topic */
        system->state.types.type_intensities[CURIOSITY_TYPE_EPISTEMIC] += strength * 0.1f;
    }

    system->stats.contagion_events++;
    nimcp_platform_mutex_unlock(system->mutex);

    return adopted;
}

float curiosity_enhanced_get_contagion_susceptibility(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_contagion_suscep", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float susceptibility = system->state.contagion.contagion_susceptibility;
    nimcp_platform_mutex_unlock(system->mutex);

    return susceptibility;
}

int curiosity_enhanced_set_contagion_susceptibility(
    curiosity_enhanced_system_t* system,
    float susceptibility) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_set_contagion_suscep", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    system->state.contagion.contagion_susceptibility = nimcp_clampf(susceptibility, 0.0f, 1.0f);
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* Surprise learning functions */
float curiosity_enhanced_report_surprise(
    curiosity_enhanced_system_t* system,
    float prediction_error,
    const char* context) {

    if (!system) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_report_surprise", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);
    float boost = surprise_report(system, prediction_error, context);
    nimcp_platform_mutex_unlock(system->mutex);

    return boost;
}

float curiosity_enhanced_get_surprise_boost(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_surprise_boost", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float boost = system->state.surprise.learning_rate_boost;
    nimcp_platform_mutex_unlock(system->mutex);

    return boost;
}

float curiosity_enhanced_prioritize_surprise(
    curiosity_enhanced_system_t* system,
    const char* experience) {

    if (!system) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_prioritize_surprise", 0.0f);

    (void)experience;  /* Could be used for more sophisticated prioritization */

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float priority = system->state.surprise.memory_consolidation_priority;
    nimcp_platform_mutex_unlock(system->mutex);

    return priority;
}

/* Fatigue functions */
float curiosity_enhanced_check_fatigue(
    const curiosity_enhanced_system_t* system,
    curiosity_fatigue_state_t* state) {

    if (!system) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_check_fatigue", 0.0f);

    /* Thread-safe read: lock, copy, unlock */
    nimcp_platform_mutex_lock(system->mutex);
    curiosity_fatigue_state_t local_fatigue = system->state.fatigue;
    nimcp_platform_mutex_unlock(system->mutex);

    if (state) {
        *state = local_fatigue;
    }

    return local_fatigue.exploration_fatigue;
}

int curiosity_enhanced_initiate_recovery(
    curiosity_enhanced_system_t* system,
    float rest_duration_ms) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_initiate_recovery", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    (void)rest_duration_ms;  /* Could be used to set expected rest duration */

    nimcp_platform_mutex_lock(system->mutex);

    system->state.fatigue.is_resting = true;
    system->state.fatigue.last_rest_period_ms = get_time_ms();
    system->stats.fatigue_rest_periods++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

bool curiosity_enhanced_needs_rest(
    const curiosity_enhanced_system_t* system) {

    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_needs_rest", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    bool needs = system->state.fatigue.needs_rest;
    nimcp_platform_mutex_unlock(system->mutex);

    return needs;
}

int curiosity_enhanced_end_recovery(
    curiosity_enhanced_system_t* system) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_end_recovery", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    system->state.fatigue.is_resting = false;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

/* Counterfactual functions */
int curiosity_enhanced_generate_counterfactual(
    curiosity_enhanced_system_t* system,
    const char* decision_point,
    const char* actual_outcome,
    curiosity_counterfactual_t* counterfactual) {

    if (!system || !decision_point || !actual_outcome || !counterfactual) {
        return NIMCP_ERROR_NULL_ARG;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_generate_counterfact", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);
    int ret = counterfactual_generate(system, decision_point, actual_outcome, counterfactual);
    nimcp_platform_mutex_unlock(system->mutex);

    return ret;
}

int curiosity_enhanced_explore_counterfactual(
    curiosity_enhanced_system_t* system,
    curiosity_counterfactual_t* counterfactual,
    float* learning_outcome) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_explore_counterfactu", 0.0f);


    NIMCP_CHECK_THROW(system && counterfactual, NIMCP_ERROR_NULL_ARG, "system or counterfactual is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    counterfactual->is_explored = true;
    system->state.counterfactual.counterfactuals_explored++;

    /* Simulate learning outcome */
    float outcome = counterfactual->learning_value * (1.0f - counterfactual->exploration_cost);
    outcome = nimcp_clampf(outcome, 0.0f, 1.0f);

    if (learning_outcome) {
        *learning_outcome = outcome;
    }

    /* Update average */
    float alpha = 0.1f;
    float new_learning_val = alpha * outcome + (1.0f - alpha) * system->state.counterfactual.avg_learning_value;
    if (isfinite(new_learning_val)) {
        system->state.counterfactual.avg_learning_value = new_learning_val;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

float curiosity_enhanced_get_counterfactual_curiosity(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_counterfactual_c", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float curiosity = system->state.counterfactual.counterfactual_curiosity;
    nimcp_platform_mutex_unlock(system->mutex);

    return curiosity;
}

/* State and statistics functions */
int curiosity_enhanced_get_state(
    const curiosity_enhanced_system_t* system,
    curiosity_enhanced_state_t* state) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_state", 0.0f);

    NIMCP_CHECK_THROW(system && state, NIMCP_ERROR_NULL_ARG, "system or state is NULL");

    /* Thread-safe read: lock, copy, unlock */
    nimcp_platform_mutex_lock(system->mutex);
    *state = system->state;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int curiosity_enhanced_get_stats(
    const curiosity_enhanced_system_t* system,
    curiosity_enhanced_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_stats", 0.0f);

    NIMCP_CHECK_THROW(system && stats, NIMCP_ERROR_NULL_ARG, "system or stats is NULL");

    /* Thread-safe read: lock, copy, unlock */
    nimcp_platform_mutex_lock(system->mutex);
    *stats = system->stats;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

void curiosity_enhanced_reset_stats(
    curiosity_enhanced_system_t* system) {

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_reset_stats", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(system->stats));
    nimcp_platform_mutex_unlock(system->mutex);
}

float curiosity_enhanced_get_overall_drive(
    const curiosity_enhanced_system_t* system) {

    if (!system) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_overall_drive", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    float drive = system->state.overall_curiosity_drive;
    nimcp_platform_mutex_unlock(system->mutex);

    return drive;
}

/* Bio-async functions */
int curiosity_enhanced_connect_bio_async(
    curiosity_enhanced_system_t* system) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    if (system->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CURIOSITY_BOREDOM,
        .module_name = "curiosity_enhanced",
        .inbox_capacity = 64,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected enhanced curiosity to bio-async router");
    }

    return 0;
}

int curiosity_enhanced_disconnect_bio_async(
    curiosity_enhanced_system_t* system) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_ARG, "system is NULL");
    if (!system->bio_async_enabled) return 0;

    bio_router_unregister_module(system->bio_ctx);
    system->bio_async_enabled = false;
    system->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Disconnected enhanced curiosity from bio-async router");
    return 0;
}

bool curiosity_enhanced_is_bio_async_connected(
    const curiosity_enhanced_system_t* system) {

    if (!system) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_is_bio_async_connect", 0.0f);

    /* Thread-safe read */
    nimcp_platform_mutex_lock(system->mutex);
    bool connected = system->bio_async_enabled;
    nimcp_platform_mutex_unlock(system->mutex);

    return connected;
}

/* String conversion functions */
const char* curiosity_type_to_string(curiosity_type_t type) {
    switch (type) {
        case CURIOSITY_TYPE_DIVERSIVE:  return "diversive";
        case CURIOSITY_TYPE_SPECIFIC:   return "specific";
        case CURIOSITY_TYPE_PERCEPTUAL: return "perceptual";
        case CURIOSITY_TYPE_EPISTEMIC:  return "epistemic";
        case CURIOSITY_TYPE_SOCIAL:     return "social";
        case CURIOSITY_TYPE_MORBID:     return "morbid";
        default:                        return "unknown";
    }
}

curiosity_type_t curiosity_type_from_string(const char* str) {
    if (!str) return CURIOSITY_TYPE_EPISTEMIC;

    if (strcmp(str, "diversive") == 0)  return CURIOSITY_TYPE_DIVERSIVE;
    if (strcmp(str, "specific") == 0)   return CURIOSITY_TYPE_SPECIFIC;
    if (strcmp(str, "perceptual") == 0) return CURIOSITY_TYPE_PERCEPTUAL;
    if (strcmp(str, "epistemic") == 0)  return CURIOSITY_TYPE_EPISTEMIC;
    if (strcmp(str, "social") == 0)     return CURIOSITY_TYPE_SOCIAL;
    if (strcmp(str, "morbid") == 0)     return CURIOSITY_TYPE_MORBID;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_curiosity_type_from_", 0.0f);


    return CURIOSITY_TYPE_EPISTEMIC;
}

/* ============================================================================
 * Quantum Bridge Integration
 * ============================================================================ */

curiosity_quantum_bridge_t* curiosity_enhanced_get_quantum_bridge(
    curiosity_enhanced_system_t* system) {

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_quantum_bridge", 0.0f);


    return system->quantum_bridge;
}

float curiosity_enhanced_quantum_explore(
    curiosity_enhanced_system_t* system,
    const char* start_topic,
    char* novel_topic) {

    if (!system || !system->quantum_bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_quantum_explore", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    /* Perform quantum exploration */
    float novelty = curiosity_quantum_explore(
        system->quantum_bridge,
        start_topic,
        0,  /* Use default steps */
        novel_topic
    );

    /* Update stats */
    if (novelty >= 0.0f) {
        system->stats.quantum_explorations++;

        /* Update average speedup */
        curiosity_quantum_stats_t qstats;
        if (curiosity_quantum_get_stats(system->quantum_bridge, &qstats) == 0) {
            system->stats.avg_quantum_speedup = qstats.avg_exploration_speedup;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return novelty;
}

int curiosity_enhanced_add_quantum_topic(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float curiosity_level,
    float novelty_score) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_add_quantum_topic", 0.0f);


    NIMCP_CHECK_THROW(system && topic, NIMCP_ERROR_NULL_ARG, "system or topic is NULL");
    NIMCP_CHECK_THROW(system->quantum_bridge, NIMCP_ERROR_INVALID, "quantum_bridge is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    int result = curiosity_quantum_add_topic(
        system->quantum_bridge,
        topic,
        curiosity_level,
        novelty_score
    );

    /* Also add to interest tracking */
    if (result >= 0) {
        curiosity_topic_interest_t* interest = interest_get_or_create(system, topic);
        if (interest) {
            interest->initial_interest = curiosity_level;
            interest->current_interest = curiosity_level;
            interest->novelty_score = novelty_score;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return result >= 0 ? 0 : NIMCP_ERROR;
}

float curiosity_enhanced_quantum_evaluate_novelty(
    curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic || !system->quantum_bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_quantum_evaluate_nov", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    float novelty = curiosity_quantum_evaluate_novelty(
        system->quantum_bridge,
        topic
    );

    nimcp_platform_mutex_unlock(system->mutex);

    return novelty;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int curiosity_enhanced_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Enhanced_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Enhanced_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Enhanced_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Quantum Monte Carlo Implementation
 * ============================================================================ */

/* Simple LCG for fast MC sampling */
static uint32_t qmc_lcg_next(uint32_t* seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

static float qmc_uniform(uint32_t* seed) {
    return (float)qmc_lcg_next(seed) / (float)0x7fffffff;
}

/* Box-Muller for Gaussian samples */
static float qmc_gaussian(uint32_t* seed, float mean, float std) {
    float u1 = qmc_uniform(seed);
    float u2 = qmc_uniform(seed);

    /* Avoid log(0) */
    if (u1 < 1e-10f) u1 = 1e-10f;

    float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
    return mean + std * z;
}

void curiosity_enhanced_qmc_default_config(curiosity_qmc_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_qmc_default_config", 0.0f);


    memset(config, 0, sizeof(*config));
    config->num_samples = 1000;
    config->burnin = 100;
    config->uncertainty_threshold = 0.3f;
    config->exploration_bonus = 0.2f;
    config->enable_empowerment = true;
    config->empowerment_horizon = 3;
    config->temperature = 1.0f;
    config->seed = 0;  /* Time-based */
}

int curiosity_enhanced_set_qmc_config(
    curiosity_enhanced_system_t* system,
    const curiosity_qmc_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_set_qmc_config", 0.0f);


    NIMCP_CHECK_THROW(system && config, NIMCP_ERROR_NULL_ARG, "system or config is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    system->qmc_config = *config;

    if (config->seed != 0) {
        system->qmc_seed = config->seed;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Updated QMC config: samples=%u, threshold=%.2f, empowerment=%s",
                       config->num_samples, config->uncertainty_threshold,
                       config->enable_empowerment ? "enabled" : "disabled");

    return 0;
}

int curiosity_enhanced_estimate_uncertainty(
    curiosity_enhanced_system_t* system,
    const char* topic,
    curiosity_qmc_uncertainty_t* result) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_estimate_uncertainty", 0.0f);


    NIMCP_CHECK_THROW(system && topic && result, NIMCP_ERROR_NULL_ARG, "system, topic, or result is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    memset(result, 0, sizeof(*result));

    /* Get base interest for topic */
    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    float base_interest = entry ? entry->interest.current_interest : 0.5f;
    float exposure_count = entry ? (float)entry->interest.exposure_count : 0.0f;

    /* Bootstrap sampling for uncertainty estimation */
    uint32_t num_samples = system->qmc_config.num_samples;
    float* samples = (float*)nimcp_calloc(num_samples, sizeof(float));

    if (!samples) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR;
    }

    float sum = 0.0f;
    float sum_sq = 0.0f;

    /* Sample interest with noise proportional to uncertainty */
    float noise_scale = 1.0f / (1.0f + exposure_count * 0.1f);  /* More exposures = less noise */

    for (uint32_t i = 0; i < num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_samples > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)num_samples);
        }

        float sample = qmc_gaussian(&system->qmc_seed, base_interest, noise_scale * 0.2f);
        sample = nimcp_clampf(sample, 0.0f, 1.0f);
        samples[i] = sample;
        sum += sample;
        sum_sq += sample * sample;
    }

    /* Compute statistics */
    result->mean_interest = sum / (float)num_samples;
    result->variance = (sum_sq / (float)num_samples) - (result->mean_interest * result->mean_interest);
    if (result->variance < 0.0f) result->variance = 0.0f;
    result->std_error = sqrtf(result->variance / (float)num_samples);

    /* 95% confidence interval */
    result->confidence_95_lower = result->mean_interest - 1.96f * result->std_error;
    result->confidence_95_upper = result->mean_interest + 1.96f * result->std_error;
    result->confidence_95_lower = nimcp_clampf(result->confidence_95_lower, 0.0f, 1.0f);
    result->confidence_95_upper = nimcp_clampf(result->confidence_95_upper, 0.0f, 1.0f);

    /* Decompose uncertainty */
    result->epistemic_uncertainty = noise_scale * 0.5f;  /* Reducible with more data */
    result->aleatoric_uncertainty = sqrtf(result->variance) - result->epistemic_uncertainty;
    if (result->aleatoric_uncertainty < 0.0f) result->aleatoric_uncertainty = 0.0f;

    result->effective_samples = num_samples;

    nimcp_free(samples);
    samples = NULL;

    /* Update statistics */
    system->qmc_stats.uncertainty_estimations++;
    system->qmc_stats.mc_samples_total += num_samples;

    float alpha = 0.01f;
    float new_eu = alpha * result->epistemic_uncertainty +
                   (1.0f - alpha) * system->qmc_stats.avg_epistemic_uncertainty;
    if (isfinite(new_eu)) {
        system->qmc_stats.avg_epistemic_uncertainty = new_eu;
    }

    if (result->epistemic_uncertainty > system->qmc_config.uncertainty_threshold) {
        system->qmc_stats.high_uncertainty_topics++;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int curiosity_enhanced_estimate_uncertainty_batch(
    curiosity_enhanced_system_t* system,
    const char** topics,
    uint32_t num_topics,
    curiosity_qmc_uncertainty_t* results) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_estimate_uncertainty", 0.0f);


    NIMCP_CHECK_THROW(system && topics && results, NIMCP_ERROR_NULL_ARG, "system, topics, or results is NULL");
    if (num_topics == 0) return 0;

    for (uint32_t i = 0; i < num_topics; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_topics > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)num_topics);
        }

        int ret = curiosity_enhanced_estimate_uncertainty(system, topics[i], &results[i]);
        if (ret != 0) {
            memset(&results[i], 0, sizeof(results[i]));
        }
    }

    return 0;
}

int curiosity_enhanced_compute_empowerment(
    curiosity_enhanced_system_t* system,
    const char* topic,
    uint32_t horizon,
    curiosity_empowerment_result_t* result) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_compute_empowerment", 0.0f);


    NIMCP_CHECK_THROW(system && topic && result, NIMCP_ERROR_NULL_ARG, "system, topic, or result is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    memset(result, 0, sizeof(*result));

    if (horizon == 0) {
        horizon = system->qmc_config.empowerment_horizon;
    }

    /* Empowerment = max I(A; S') over action distributions
     * Approximated via MC sampling of reachable states */

    /* Count available "actions" (related topics in graph) */
    uint32_t num_actions = 0;
    if (system->quantum_bridge) {
        num_actions = curiosity_quantum_get_graph_size(system->quantum_bridge);
    }
    if (num_actions == 0) {
        num_actions = 4;  /* Default action space */
    }
    if (num_actions > 64) {
        num_actions = 64;  /* Cap for MC efficiency */
    }

    result->num_actions = num_actions;
    result->action_empowerment = (float*)nimcp_calloc(num_actions, sizeof(float));

    if (!result->action_empowerment) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR;
    }

    /* MC estimation of empowerment */
    uint32_t num_samples = system->qmc_config.num_samples;
    float* state_counts = (float*)nimcp_calloc(num_actions * num_actions, sizeof(float));

    if (!state_counts) {
        nimcp_free(result->action_empowerment);
        result->action_empowerment = NULL;
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR;
    }

    /* Simulate action-state transitions */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        uint32_t action = qmc_lcg_next(&system->qmc_seed) % num_actions;
        uint32_t next_state = 0;

        /* Transition model: action has high probability of leading to corresponding state */
        float p = qmc_uniform(&system->qmc_seed);
        if (p < 0.7f) {
            next_state = action;  /* Deterministic component */
        } else {
            next_state = qmc_lcg_next(&system->qmc_seed) % num_actions;  /* Random */
        }

        state_counts[action * num_actions + next_state] += 1.0f;
    }

    /* Compute mutual information I(A; S') */
    float* p_a = (float*)nimcp_calloc(num_actions, sizeof(float));
    float* p_s = (float*)nimcp_calloc(num_actions, sizeof(float));

    if (!p_a || !p_s) {
        nimcp_free(state_counts);
        state_counts = NULL;
        nimcp_free(result->action_empowerment);
        result->action_empowerment = NULL;
        if (p_a) nimcp_free(p_a);
        if (p_s) nimcp_free(p_s);
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR;
    }

    /* Marginal distributions */
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            float count = state_counts[a * num_actions + s];
            p_a[a] += count;
            p_s[s] += count;
        }
    }

    /* Normalize */
    float total = (float)num_samples;
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        p_a[i] /= total;
        p_s[i] /= total;
    }

    /* Compute entropy H(S') */
    float H_s = 0.0f;
    for (uint32_t s = 0; s < num_actions; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_actions > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(s + 1) / (float)num_actions);
        }

        if (p_s[s] > 1e-10f) {
            H_s -= p_s[s] * logf(p_s[s]);
        }
    }

    /* Compute conditional entropy H(S'|A) */
    float H_s_given_a = 0.0f;
    for (uint32_t a = 0; a < num_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && num_actions > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(a + 1) / (float)num_actions);
        }

        if (p_a[a] < 1e-10f) continue;

        float action_total = 0.0f;
        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            action_total += state_counts[a * num_actions + s];
        }

        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            float p_s_given_a = state_counts[a * num_actions + s] / (action_total + 1e-10f);
            if (p_s_given_a > 1e-10f) {
                H_s_given_a -= p_a[a] * p_s_given_a * logf(p_s_given_a);
            }
        }

        /* Per-action empowerment */
        float action_entropy = 0.0f;
        for (uint32_t s = 0; s < num_actions; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_actions > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(s + 1) / (float)num_actions);
            }

            float p = state_counts[a * num_actions + s] / (action_total + 1e-10f);
            if (p > 1e-10f) {
                action_entropy -= p * logf(p);
            }
        }
        result->action_empowerment[a] = action_entropy / logf(2.0f);
    }

    /* Mutual information I(A; S') = H(S') - H(S'|A) */
    float mutual_info = H_s - H_s_given_a;
    if (mutual_info < 0.0f) mutual_info = 0.0f;

    /* Convert to bits */
    result->empowerment = mutual_info / logf(2.0f);
    result->entropy_current = H_s / logf(2.0f);
    result->entropy_reachable = H_s / logf(2.0f);
    result->channel_capacity = logf((float)num_actions) / logf(2.0f);

    /* Normalize empowerment to [0, 1] */
    if (result->channel_capacity > 0.0f) {
        result->empowerment_normalized = result->empowerment / result->channel_capacity;
        result->empowerment_normalized = nimcp_clampf(result->empowerment_normalized, 0.0f, 1.0f);
    }

    nimcp_free(state_counts);
    state_counts = NULL;
    nimcp_free(p_a);
    p_a = NULL;
    nimcp_free(p_s);
    p_s = NULL;

    /* Update statistics */
    system->qmc_stats.empowerment_calculations++;
    system->qmc_stats.mc_samples_total += num_samples;

    float alpha = 0.01f;
    float new_empowerment = alpha * result->empowerment + (1.0f - alpha) * system->qmc_stats.avg_empowerment;
    if (isfinite(new_empowerment)) {
        system->qmc_stats.avg_empowerment = new_empowerment;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Computed empowerment for '%s': %.3f bits (%.1f%% of capacity)",
                       topic, result->empowerment,
                       result->empowerment_normalized * 100.0f);

    return 0;
}

void curiosity_empowerment_result_free(curiosity_empowerment_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_curiosity_empowermen", 0.0f);


    if (result->action_empowerment) {
        nimcp_free(result->action_empowerment);
        result->action_empowerment = NULL;
    }
}

float curiosity_enhanced_sample_by_uncertainty(
    curiosity_enhanced_system_t* system,
    const char** topics,
    uint32_t num_topics,
    char* selected_topic) {

    if (!system || !topics || num_topics == 0 || !selected_topic) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_sample_by_uncertaint", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    /* Compute uncertainty-weighted scores */
    float* scores = (float*)nimcp_calloc(num_topics, sizeof(float));
    if (!scores) {
        nimcp_platform_mutex_unlock(system->mutex);
        return -1.0f;
    }

    float total_score = 0.0f;

    for (uint32_t i = 0; i < num_topics; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_topics > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)num_topics);
        }

        curiosity_qmc_uncertainty_t uncertainty;

        /* Estimate uncertainty (unlock/lock to avoid recursion issues) */
        nimcp_platform_mutex_unlock(system->mutex);
        int ret = curiosity_enhanced_estimate_uncertainty(system, topics[i], &uncertainty);
        nimcp_platform_mutex_lock(system->mutex);

        if (ret == 0) {
            /* Score = interest × (1 + epistemic_uncertainty) */
            float interest = uncertainty.mean_interest;
            float bonus = 1.0f + uncertainty.epistemic_uncertainty * 2.0f;
            scores[i] = interest * bonus;
        } else {
            scores[i] = 0.5f;  /* Default score */
        }

        total_score += scores[i];
    }

    /* Normalize and sample */
    if (total_score < 1e-10f) {
        /* Uniform sampling */
        uint32_t idx = qmc_lcg_next(&system->qmc_seed) % num_topics;
        strncpy(selected_topic, topics[idx], 255);
        selected_topic[255] = '\0';
        nimcp_free(scores);
        scores = NULL;
        nimcp_platform_mutex_unlock(system->mutex);
        return 0.5f;
    }

    float r = qmc_uniform(&system->qmc_seed) * total_score;
    float cumsum = 0.0f;
    uint32_t selected_idx = 0;

    for (uint32_t i = 0; i < num_topics; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_topics > 256) {
            curiosity_enhanced_heartbeat("curiosity_en_loop",
                             (float)(i + 1) / (float)num_topics);
        }

        cumsum += scores[i];
        if (r <= cumsum) {
            selected_idx = i;
            break;
        }
    }

    strncpy(selected_topic, topics[selected_idx], 255);
    selected_topic[255] = '\0';

    float selected_uncertainty = scores[selected_idx] / total_score;
    nimcp_free(scores);
    scores = NULL;

    nimcp_platform_mutex_unlock(system->mutex);

    return selected_uncertainty;
}

float curiosity_enhanced_get_exploration_bonus(
    curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_exploration_bonu", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    /* UCB-style exploration bonus */
    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    uint64_t visits = entry ? entry->interest.exposure_count : 0;
    uint64_t total = system->total_topic_visits;

    if (total == 0) {
        nimcp_platform_mutex_unlock(system->mutex);
        return system->qmc_config.exploration_bonus;  /* Max bonus for unvisited */
    }

    /* UCB1: sqrt(2 * ln(N) / n) */
    float bonus = 0.0f;
    if (visits == 0) {
        bonus = system->qmc_config.exploration_bonus;
    } else {
        float log_total = logf((float)(total + 1));
        bonus = sqrtf(2.0f * log_total / (float)(visits + 1));
        bonus *= system->qmc_config.exploration_bonus;
        bonus = nimcp_clampf(bonus, 0.0f, system->qmc_config.exploration_bonus);
    }

    system->qmc_stats.total_exploration_bonus += bonus;

    nimcp_platform_mutex_unlock(system->mutex);

    return bonus;
}

float curiosity_enhanced_update_interest_mc(
    curiosity_enhanced_system_t* system,
    const char* topic,
    float observed_interest,
    float observation_noise) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_update_interest_mc", 0.0f);


    observed_interest = nimcp_clampf(observed_interest, 0.0f, 1.0f);
    observation_noise = nimcp_clampf(observation_noise, 0.01f, 1.0f);

    nimcp_platform_mutex_lock(system->mutex);

    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    float prior_mean = entry ? entry->interest.current_interest : 0.5f;
    float prior_var = entry ? 1.0f / (1.0f + (float)entry->interest.exposure_count * 0.1f) : 0.25f;

    /* Bayesian update with Gaussian likelihood */
    float likelihood_var = observation_noise * observation_noise;

    /* Posterior: weighted combination of prior and likelihood */
    float posterior_var = 1.0f / (1.0f / prior_var + 1.0f / likelihood_var);
    float posterior_mean = posterior_var * (prior_mean / prior_var + observed_interest / likelihood_var);

    posterior_mean = nimcp_clampf(posterior_mean, 0.0f, 1.0f);

    /* Update entry */
    if (entry) {
        entry->interest.current_interest = posterior_mean;
        entry->interest.exposure_count++;
        entry->interest.last_exposure_ms = get_time_ms();
    }

    system->total_topic_visits++;

    nimcp_platform_mutex_unlock(system->mutex);

    return posterior_mean;
}

float curiosity_enhanced_estimate_info_gain_qmc(
    curiosity_enhanced_system_t* system,
    const char* topic) {

    if (!system || !topic) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_estimate_info_gain_q", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);

    /* Information gain = KL divergence from prior to posterior */
    interest_entry_t* entry = (interest_entry_t*)hash_table_lookup_string(
        system->interest_table, topic);

    float prior_mean = entry ? entry->interest.current_interest : 0.5f;
    float exposure_count = entry ? (float)entry->interest.exposure_count : 0.0f;

    /* Prior variance */
    float prior_var = 1.0f / (1.0f + exposure_count * 0.1f);

    /* Expected posterior variance after observation */
    float obs_noise = 0.2f;  /* Typical observation noise */
    float posterior_var = 1.0f / (1.0f / prior_var + 1.0f / (obs_noise * obs_noise));

    /* Information gain = 0.5 * log(prior_var / posterior_var) */
    float info_gain = 0.5f * logf(prior_var / posterior_var);

    /* Adjust for interest level (higher interest = more valuable information) */
    info_gain *= (0.5f + prior_mean * 0.5f);

    /* Convert to bits */
    info_gain /= logf(2.0f);
    info_gain = nimcp_clampf(info_gain, 0.0f, 5.0f);

    nimcp_platform_mutex_unlock(system->mutex);

    return info_gain;
}

uint32_t curiosity_enhanced_plan_exploration_mcts(
    curiosity_enhanced_system_t* system,
    const char* start_topic,
    uint32_t horizon,
    char** exploration_path,
    uint32_t max_path_length) {

    if (!system || !exploration_path || max_path_length == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_plan_exploration_mct", 0.0f);


    if (horizon == 0) {
        horizon = system->qmc_config.empowerment_horizon;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Simple MCTS-style planning via rollouts */
    uint32_t path_length = 0;
    uint32_t num_rollouts = system->qmc_config.num_samples / 10;
    if (num_rollouts < 10) num_rollouts = 10;

    /* Collect topics from quantum bridge if available */
    uint32_t num_topics = 0;
    char** available_topics = NULL;

    if (system->quantum_bridge) {
        num_topics = curiosity_quantum_get_graph_size(system->quantum_bridge);
        if (num_topics > 0) {
            available_topics = (char**)nimcp_calloc(num_topics, sizeof(char*));
            if (available_topics) {
                for (uint32_t i = 0; i < num_topics; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && num_topics > 256) {
                        curiosity_enhanced_heartbeat("curiosity_en_loop",
                                         (float)(i + 1) / (float)num_topics);
                    }

                    available_topics[i] = (char*)nimcp_malloc(256);
                    if (available_topics[i] && system->quantum_bridge->topics[i].is_active) {
                        strncpy(available_topics[i], system->quantum_bridge->topics[i].topic, 255);
                        available_topics[i][255] = '\0';
                    } else if (available_topics[i]) {
                        available_topics[i][0] = '\0';
                    }
                }
            }
        }
    }

    if (num_topics == 0) {
        /* No topics available */
        nimcp_platform_mutex_unlock(system->mutex);
        return 0;
    }

    /* Greedy rollout-based planning */
    char current_topic[NIMCP_NAME_BUFFER_SIZE];
    if (start_topic) {
        strncpy(current_topic, start_topic, 255);
        current_topic[255] = '\0';
    } else if (available_topics && available_topics[0][0] != '\0') {
        strncpy(current_topic, available_topics[0], 255);
        current_topic[255] = '\0';
    } else {
        /* Cleanup and return */
        if (available_topics) {
            for (uint32_t i = 0; i < num_topics; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_topics > 256) {
                    curiosity_enhanced_heartbeat("curiosity_en_loop",
                                     (float)(i + 1) / (float)num_topics);
                }

                if (available_topics[i]) nimcp_free(available_topics[i]);
            }
            nimcp_free(available_topics);
            available_topics = NULL;
        }
        nimcp_platform_mutex_unlock(system->mutex);
        return 0;
    }

    for (uint32_t step = 0; step < horizon && path_length < max_path_length; step++) {
        float* scores = (float*)nimcp_calloc(num_topics, sizeof(float));
        if (!scores) break;

        /* Evaluate each candidate topic */
        for (uint32_t t = 0; t < num_topics; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && num_topics > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(t + 1) / (float)num_topics);
            }

            if (available_topics[t][0] == '\0') continue;

            /* Score = info_gain + exploration_bonus */
            nimcp_platform_mutex_unlock(system->mutex);
            float info_gain = curiosity_enhanced_estimate_info_gain_qmc(system, available_topics[t]);
            float bonus = curiosity_enhanced_get_exploration_bonus(system, available_topics[t]);
            nimcp_platform_mutex_lock(system->mutex);

            scores[t] = info_gain + bonus;
        }

        /* Select best topic */
        float best_score = -1.0f;
        uint32_t best_idx = 0;
        for (uint32_t t = 0; t < num_topics; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && num_topics > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(t + 1) / (float)num_topics);
            }

            if (scores[t] > best_score) {
                best_score = scores[t];
                best_idx = t;
            }
        }

        nimcp_free(scores);
        scores = NULL;

        if (best_score <= 0.0f) break;

        /* Add to path */
        if (exploration_path[path_length]) {
            strncpy(exploration_path[path_length], available_topics[best_idx], 255);
            exploration_path[path_length][255] = '\0';
        }
        path_length++;

        /* Update current */
        strncpy(current_topic, available_topics[best_idx], 255);
    }

    /* Cleanup */
    if (available_topics) {
        for (uint32_t i = 0; i < num_topics; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_topics > 256) {
                curiosity_enhanced_heartbeat("curiosity_en_loop",
                                 (float)(i + 1) / (float)num_topics);
            }

            if (available_topics[i]) nimcp_free(available_topics[i]);
        }
        nimcp_free(available_topics);
        available_topics = NULL;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    return path_length;
}

int curiosity_enhanced_get_qmc_stats(
    const curiosity_enhanced_system_t* system,
    curiosity_qmc_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_get_qmc_stats", 0.0f);


    NIMCP_CHECK_THROW(system && stats, NIMCP_ERROR_NULL_ARG, "system or stats is NULL");

    *stats = system->qmc_stats;
    return 0;
}

void curiosity_enhanced_reset_qmc_stats(
    curiosity_enhanced_system_t* system) {

    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    curiosity_enhanced_heartbeat("curiosity_en_reset_qmc_stats", 0.0f);


    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->qmc_stats, 0, sizeof(system->qmc_stats));
    nimcp_platform_mutex_unlock(system->mutex);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_enhanced_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_curiosity_enhanced_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_enhanced_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_enhanced_training_begin: NULL argument");
        return -1;
    }
    curiosity_enhanced_heartbeat_instance(g_curiosity_enhanced_health_agent, "curiosity_enhanced_training_begin", 0.0f);
    (void)(struct interest_entry_s*)instance; /* Module state available for reset */
    return 0;
}

int curiosity_enhanced_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_enhanced_training_end: NULL argument");
        return -1;
    }
    curiosity_enhanced_heartbeat_instance(g_curiosity_enhanced_health_agent, "curiosity_enhanced_training_end", 1.0f);
    (void)(struct interest_entry_s*)instance; /* Module state available for finalization */
    return 0;
}

int curiosity_enhanced_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_enhanced_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_enhanced_heartbeat_instance(g_curiosity_enhanced_health_agent, "curiosity_enhanced_training_step", progress);
    (void)(struct interest_entry_s*)instance; /* Module state available for step adaptation */
    return 0;
}

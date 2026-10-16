/**
 * @file nimcp_trauma_resilience.c
 * @brief Trauma resilience and recall dampening — prevents PTSD-like feedback loops
 *
 * WHAT: Modulates involuntary recall intensity based on frequency, arousal, and habituation
 * WHY:  Without dampening, high-similarity traumatic memories get recalled repeatedly,
 *        each recall raises arousal, which triggers more recall — a runaway loop
 * HOW:  Per-engram recall frequency tracking, exponential habituation, arousal homeostasis
 *
 * @author Claude Code
 * @date 2026-03
 */

#include "cognitive/nimcp_trauma_resilience.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "TRAUMA_RESILIENCE"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_TRACKED_ENGRAMS  256
#define NORMAL_BLEND         0.15f
#define AROUSAL_EMA_ALPHA    0.05f
#define AROUSAL_BASELINE     0.5f

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_trauma_resilience {
    nimcp_trauma_resilience_config_t config;

    /* Recall frequency tracking (circular buffer) */
    uint64_t* recent_recall_ids;        /* Circular buffer of recently recalled engram IDs */
    float*    recent_recall_intensity;  /* Emotional intensity of each recall */
    uint32_t  recall_head;
    uint32_t  recall_count;

    /* Per-engram recall counts (for dampening) */
    uint64_t  tracked_engram_ids[MAX_TRACKED_ENGRAMS];
    uint32_t  engram_recall_counts[MAX_TRACKED_ENGRAMS];
    float     engram_habituation[MAX_TRACKED_ENGRAMS]; /* 1.0 = full, 0.0 = habituated */
    uint32_t  num_tracked;

    /* Arousal tracking */
    float     arousal_ema;              /* Smoothed arousal */
    uint32_t  high_arousal_steps;       /* Consecutive steps above ceiling */
    uint32_t  low_arousal_steps;        /* Consecutive steps below floor */

    /* Wellbeing */
    nimcp_mental_state_t state;
    float     wellbeing_score;
    uint32_t  crisis_count;
    uint32_t  dampening_count;

    /* Step counter */
    uint32_t  total_steps;
};

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_trauma_resilience_config_t nimcp_trauma_resilience_config_default(void)
{
    nimcp_trauma_resilience_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.max_recall_frequency = 5;
    cfg.recall_window_steps  = 500;
    cfg.dampening_factor     = 0.7f;
    cfg.min_recall_blend     = 0.02f;

    cfg.arousal_ceiling      = 0.85f;
    cfg.arousal_floor        = 0.15f;
    cfg.arousal_patience     = 200;
    cfg.homeostatic_pull     = 0.01f;

    cfg.habituation_rate     = 0.9f;
    cfg.habituation_memory   = 100;

    cfg.stress_threshold     = 0.6f;
    cfg.distress_threshold   = 0.3f;
    cfg.crisis_threshold     = 0.1f;

    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_trauma_resilience_t* nimcp_trauma_resilience_create(const nimcp_trauma_resilience_config_t* config)
{
    nimcp_trauma_resilience_config_t cfg = config ? *config : nimcp_trauma_resilience_config_default();

    nimcp_trauma_resilience_t* tr = (nimcp_trauma_resilience_t*)nimcp_calloc(1, sizeof(nimcp_trauma_resilience_t));
    if (!tr) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate trauma resilience system");
        return NULL;
    }

    tr->config = cfg;

    /* Allocate circular recall buffer */
    uint32_t buf_size = cfg.habituation_memory > 0 ? cfg.habituation_memory : 100;
    tr->recent_recall_ids = (uint64_t*)nimcp_calloc(buf_size, sizeof(uint64_t));
    tr->recent_recall_intensity = (float*)nimcp_calloc(buf_size, sizeof(float));
    if (!tr->recent_recall_ids || !tr->recent_recall_intensity) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate recall tracking buffers");
        nimcp_free(tr->recent_recall_ids);
        nimcp_free(tr->recent_recall_intensity);
        nimcp_free(tr);
        return NULL;
    }

    /* Initialize per-engram tracking */
    tr->num_tracked = 0;
    for (uint32_t i = 0; i < MAX_TRACKED_ENGRAMS; i++) {
        tr->engram_habituation[i] = 1.0f;
    }

    /* Initialize arousal to baseline */
    tr->arousal_ema = AROUSAL_BASELINE;
    tr->state = NIMCP_MENTAL_HEALTHY;
    tr->wellbeing_score = 1.0f;

    LOG_INFO(LOG_MODULE, "Trauma resilience system created (max_recall_freq=%u, dampening=%.2f, "
             "habituation_rate=%.2f, habituation_memory=%u)",
             cfg.max_recall_frequency, cfg.dampening_factor,
             cfg.habituation_rate, buf_size);

    return tr;
}

void nimcp_trauma_resilience_destroy(nimcp_trauma_resilience_t* tr)
{
    if (!tr) {
        return;
    }

    nimcp_free(tr->recent_recall_ids);
    nimcp_free(tr->recent_recall_intensity);
    nimcp_free(tr);
}

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Find an engram in the tracked array, or return -1.
 */
static int find_tracked_engram(const nimcp_trauma_resilience_t* tr, uint64_t engram_id)
{
    for (uint32_t i = 0; i < tr->num_tracked; i++) {
        if (tr->tracked_engram_ids[i] == engram_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Add an engram to the tracked array. Evicts oldest if full.
 * @return Index of the newly added engram.
 */
static uint32_t add_tracked_engram(nimcp_trauma_resilience_t* tr, uint64_t engram_id)
{
    if (tr->num_tracked < MAX_TRACKED_ENGRAMS) {
        uint32_t idx = tr->num_tracked;
        tr->tracked_engram_ids[idx] = engram_id;
        tr->engram_recall_counts[idx] = 0;
        tr->engram_habituation[idx] = 1.0f;
        tr->num_tracked++;
        return idx;
    }

    /* Evict the engram with the lowest recall count (least concerning) */
    uint32_t min_idx = 0;
    uint32_t min_count = tr->engram_recall_counts[0];
    for (uint32_t i = 1; i < MAX_TRACKED_ENGRAMS; i++) {
        if (tr->engram_recall_counts[i] < min_count) {
            min_count = tr->engram_recall_counts[i];
            min_idx = i;
        }
    }
    tr->tracked_engram_ids[min_idx] = engram_id;
    tr->engram_recall_counts[min_idx] = 0;
    tr->engram_habituation[min_idx] = 1.0f;
    return min_idx;
}

/**
 * @brief Compute the maximum recall count across all tracked engrams.
 */
static uint32_t max_recall_count(const nimcp_trauma_resilience_t* tr)
{
    uint32_t max_c = 0;
    for (uint32_t i = 0; i < tr->num_tracked; i++) {
        if (tr->engram_recall_counts[i] > max_c) {
            max_c = tr->engram_recall_counts[i];
        }
    }
    return max_c;
}

/**
 * @brief Compute wellbeing and update mental state.
 */
static void update_wellbeing(nimcp_trauma_resilience_t* tr)
{
    /* Arousal stability: penalize extremes (1.0 at baseline, 0.0 at edges) */
    float arousal_stability = 1.0f - fabsf(tr->arousal_ema - AROUSAL_BASELINE) * 2.0f;
    if (arousal_stability < 0.0f) arousal_stability = 0.0f;

    /* Rumination index: how fixated on one memory (0 = diverse, 1 = fixated) */
    float rumination = 0.0f;
    uint32_t max_freq = tr->config.max_recall_frequency;
    if (max_freq > 0) {
        uint32_t max_c = max_recall_count(tr);
        rumination = (float)max_c / (float)max_freq;
        if (rumination > 1.0f) rumination = 1.0f;
    }

    /* Recall diversity: inverse of rumination */
    float recall_diversity = 1.0f - rumination;

    /* Composite wellbeing */
    tr->wellbeing_score = 0.3f * arousal_stability
                        + 0.3f * recall_diversity
                        + 0.4f * (1.0f - rumination);
    if (tr->wellbeing_score < 0.0f) tr->wellbeing_score = 0.0f;
    if (tr->wellbeing_score > 1.0f) tr->wellbeing_score = 1.0f;

    /* Determine state from wellbeing */
    nimcp_mental_state_t prev_state = tr->state;
    if (tr->wellbeing_score > tr->config.stress_threshold) {
        tr->state = NIMCP_MENTAL_HEALTHY;
    } else if (tr->wellbeing_score > tr->config.distress_threshold) {
        tr->state = NIMCP_MENTAL_STRESSED;
    } else if (tr->wellbeing_score > tr->config.crisis_threshold) {
        tr->state = NIMCP_MENTAL_DISTRESSED;
    } else {
        tr->state = NIMCP_MENTAL_CRISIS;
        if (prev_state != NIMCP_MENTAL_CRISIS) {
            tr->crisis_count++;
            LOG_WARN(LOG_MODULE, "CRISIS state entered (wellbeing=%.3f, crisis_count=%u)",
                     tr->wellbeing_score, tr->crisis_count);
        }
    }

    /* Count active dampened memories */
    tr->dampening_count = 0;
    for (uint32_t i = 0; i < tr->num_tracked; i++) {
        if (tr->engram_recall_counts[i] > 1) {
            tr->dampening_count++;
        }
    }
}

/* ============================================================================
 * Core API
 * ============================================================================ */

float nimcp_trauma_resilience_modulate_recall(nimcp_trauma_resilience_t* tr,
    uint64_t recalled_engram_id, float recall_similarity, float current_arousal)
{
    if (!tr) {
        return NORMAL_BLEND;
    }

    tr->total_steps++;

    /* Look up this engram in tracked set */
    int idx = find_tracked_engram(tr, recalled_engram_id);
    float blend = NORMAL_BLEND;

    if (idx >= 0) {
        /* Known engram — apply habituation and dampening */
        uint32_t count = tr->engram_recall_counts[idx];
        float habituation = tr->engram_habituation[idx];

        /* Habituation: each repeat reduces impact exponentially */
        blend = NORMAL_BLEND * habituation;

        /* Dampening: each repeat beyond the first reduces further */
        if (count > 0) {
            float damp = 1.0f;
            for (uint32_t r = 0; r < count && r < 20; r++) {
                damp *= tr->config.dampening_factor;
            }
            blend *= damp;
        }

        /* Hard suppress if over frequency limit */
        if (count >= tr->config.max_recall_frequency) {
            blend = tr->config.min_recall_blend;
            LOG_DEBUG(LOG_MODULE, "Engram %lu suppressed (recall_count=%u >= max=%u)",
                      (unsigned long)recalled_engram_id, count,
                      tr->config.max_recall_frequency);
        }
    }
    /* If not found, blend stays at NORMAL_BLEND (first recall = normal) */

    /* Mental state reduction */
    if (tr->state == NIMCP_MENTAL_CRISIS) {
        blend *= 0.1f;  /* 90% reduction in crisis */
    } else if (tr->state == NIMCP_MENTAL_DISTRESSED) {
        blend *= 0.5f;  /* 50% reduction when distressed */
    }

    /* Clamp */
    if (blend < tr->config.min_recall_blend) blend = tr->config.min_recall_blend;
    if (blend > NORMAL_BLEND) blend = NORMAL_BLEND;

    /* Update wellbeing periodically */
    if (tr->total_steps % 10 == 0) {
        update_wellbeing(tr);
    }

    return blend;
}

void nimcp_trauma_resilience_record_recall(nimcp_trauma_resilience_t* tr,
    uint64_t engram_id, float emotional_intensity)
{
    if (!tr) {
        return;
    }

    /* Add to circular buffer */
    uint32_t buf_size = tr->config.habituation_memory > 0 ? tr->config.habituation_memory : 100;
    tr->recent_recall_ids[tr->recall_head] = engram_id;
    tr->recent_recall_intensity[tr->recall_head] = emotional_intensity;
    tr->recall_head = (tr->recall_head + 1) % buf_size;
    if (tr->recall_count < buf_size) {
        tr->recall_count++;
    }

    /* Update per-engram tracking */
    int idx = find_tracked_engram(tr, engram_id);
    if (idx < 0) {
        idx = (int)add_tracked_engram(tr, engram_id);
    }
    tr->engram_recall_counts[idx]++;

    /* Apply habituation decay */
    tr->engram_habituation[idx] *= tr->config.habituation_rate;
    if (tr->engram_habituation[idx] < 0.01f) {
        tr->engram_habituation[idx] = 0.01f;  /* Floor to prevent complete zeroing */
    }

    /* High-intensity recalls accelerate habituation decay (paradoxically,
     * the brain needs MORE dampening for intense memories) */
    if (emotional_intensity > 0.8f) {
        tr->engram_habituation[idx] *= 0.9f;  /* Extra 10% decay for intense recalls */
    }
}

float nimcp_trauma_resilience_regulate_arousal(nimcp_trauma_resilience_t* tr,
    float current_arousal)
{
    if (!tr) {
        return current_arousal;
    }

    /* Update EMA */
    tr->arousal_ema = (1.0f - AROUSAL_EMA_ALPHA) * tr->arousal_ema
                    + AROUSAL_EMA_ALPHA * current_arousal;

    float adjusted = current_arousal;

    /* High arousal intervention */
    if (tr->arousal_ema > tr->config.arousal_ceiling) {
        tr->high_arousal_steps++;
        tr->low_arousal_steps = 0;

        if (tr->high_arousal_steps > tr->config.arousal_patience) {
            /* Apply homeostatic pull toward baseline */
            float pull = tr->config.homeostatic_pull;

            /* Increase pull strength for very prolonged high arousal */
            if (tr->high_arousal_steps > tr->config.arousal_patience * 2) {
                pull *= 2.0f;
            }
            if (tr->high_arousal_steps > tr->config.arousal_patience * 4) {
                pull *= 2.0f;  /* 4x pull for extreme prolonged arousal */
            }

            adjusted = current_arousal - pull;
            if (adjusted < tr->config.arousal_floor) {
                adjusted = tr->config.arousal_floor;
            }
        }
    }
    /* Low arousal intervention */
    else if (tr->arousal_ema < tr->config.arousal_floor) {
        tr->low_arousal_steps++;
        tr->high_arousal_steps = 0;

        if (tr->low_arousal_steps > tr->config.arousal_patience) {
            float pull = tr->config.homeostatic_pull;
            adjusted = current_arousal + pull;
            if (adjusted > tr->config.arousal_ceiling) {
                adjusted = tr->config.arousal_ceiling;
            }
        }
    }
    /* Normal range — reset counters */
    else {
        tr->high_arousal_steps = 0;
        tr->low_arousal_steps = 0;
    }

    /* Clamp to [0, 1] */
    if (adjusted < 0.0f) adjusted = 0.0f;
    if (adjusted > 1.0f) adjusted = 1.0f;

    return adjusted;
}

int nimcp_trauma_resilience_get_wellbeing(const nimcp_trauma_resilience_t* tr,
    nimcp_wellbeing_t* wellbeing)
{
    if (!tr || !wellbeing) {
        return -1;
    }

    memset(wellbeing, 0, sizeof(nimcp_wellbeing_t));

    /* Arousal stability */
    wellbeing->arousal_stability = 1.0f - fabsf(tr->arousal_ema - AROUSAL_BASELINE) * 2.0f;
    if (wellbeing->arousal_stability < 0.0f) wellbeing->arousal_stability = 0.0f;

    /* Rumination index */
    uint32_t max_freq = tr->config.max_recall_frequency;
    if (max_freq > 0) {
        uint32_t max_c = max_recall_count(tr);
        wellbeing->rumination_index = (float)max_c / (float)max_freq;
        if (wellbeing->rumination_index > 1.0f) wellbeing->rumination_index = 1.0f;
    }

    wellbeing->recall_diversity = 1.0f - wellbeing->rumination_index;
    wellbeing->neuromod_balance = 1.0f;  /* Placeholder — could integrate with neuromodulators */

    wellbeing->wellbeing_score = tr->wellbeing_score;
    wellbeing->state = tr->state;
    wellbeing->crisis_count = tr->crisis_count;
    wellbeing->dampening_active = tr->dampening_count;

    return 0;
}

nimcp_mental_state_t nimcp_trauma_resilience_get_state(const nimcp_trauma_resilience_t* tr)
{
    if (!tr) {
        return NIMCP_MENTAL_HEALTHY;
    }
    return tr->state;
}

int nimcp_trauma_resilience_reset(nimcp_trauma_resilience_t* tr)
{
    if (!tr) {
        return -1;
    }

    LOG_INFO(LOG_MODULE, "Therapeutic reset: clearing all recall tracking and restoring healthy state");

    /* Reset per-engram tracking */
    tr->num_tracked = 0;
    memset(tr->tracked_engram_ids, 0, sizeof(tr->tracked_engram_ids));
    memset(tr->engram_recall_counts, 0, sizeof(tr->engram_recall_counts));
    for (uint32_t i = 0; i < MAX_TRACKED_ENGRAMS; i++) {
        tr->engram_habituation[i] = 1.0f;
    }

    /* Reset circular buffer */
    uint32_t buf_size = tr->config.habituation_memory > 0 ? tr->config.habituation_memory : 100;
    memset(tr->recent_recall_ids, 0, buf_size * sizeof(uint64_t));
    memset(tr->recent_recall_intensity, 0, buf_size * sizeof(float));
    tr->recall_head = 0;
    tr->recall_count = 0;

    /* Reset arousal */
    tr->arousal_ema = AROUSAL_BASELINE;
    tr->high_arousal_steps = 0;
    tr->low_arousal_steps = 0;

    /* Reset state */
    tr->state = NIMCP_MENTAL_HEALTHY;
    tr->wellbeing_score = 1.0f;
    tr->dampening_count = 0;
    /* Preserve crisis_count — it's a lifetime counter */

    return 0;
}

const char* nimcp_mental_state_name(nimcp_mental_state_t state)
{
    switch (state) {
        case NIMCP_MENTAL_HEALTHY:    return "HEALTHY";
        case NIMCP_MENTAL_STRESSED:   return "STRESSED";
        case NIMCP_MENTAL_DISTRESSED: return "DISTRESSED";
        case NIMCP_MENTAL_CRISIS:     return "CRISIS";
        default:                      return "UNKNOWN";
    }
}

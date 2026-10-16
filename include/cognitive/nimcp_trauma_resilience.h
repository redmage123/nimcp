/**
 * @file nimcp_trauma_resilience.h
 * @brief Trauma resilience and recall dampening system
 *
 * WHAT: Prevents PTSD-like feedback loops from involuntary recall
 * WHY:  Involuntary recall can repeatedly trigger traumatic memories,
 *        creating a runaway positive-feedback loop of high arousal +
 *        fixated recall that degrades all cognition.
 * HOW:  Track recall frequency per engram, apply habituation + dampening,
 *        regulate arousal homeostasis, compute composite wellbeing score.
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal pattern completion can reactivate traumatic memories
 * - Repeated involuntary recall without extinction = PTSD maintenance
 * - Arousal homeostasis via HPA axis regulation
 * - Emotional habituation (repeated stimuli lose impact over time)
 * - Prefrontal inhibition of amygdala-driven recall
 *
 * INTEGRATION:
 * - Called by brain_decide() BEFORE involuntary recall blend
 * - Modulates recall blend ratio (0.15 normal -> 0.02 suppressed)
 * - Complements existing mental_health_monitor (disorder detection)
 *
 * @author Claude Code
 * @date 2026-03
 */

#ifndef NIMCP_TRAUMA_RESILIENCE_H
#define NIMCP_TRAUMA_RESILIENCE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mental health state */
typedef enum {
    NIMCP_MENTAL_HEALTHY = 0,       /* Normal operation */
    NIMCP_MENTAL_STRESSED,          /* Elevated arousal, manageable */
    NIMCP_MENTAL_DISTRESSED,        /* Sustained high arousal, recall suppression active */
    NIMCP_MENTAL_CRISIS,            /* Severe imbalance, emergency dampening */
} nimcp_mental_state_t;

/* Wellbeing metrics */
typedef struct {
    float wellbeing_score;          /* 0-1, composite mental health score */
    float arousal_stability;        /* 0-1, how stable arousal has been */
    float recall_diversity;         /* 0-1, variety of recalled memories (low = fixation) */
    float neuromod_balance;         /* 0-1, balance across neuromodulators */
    float rumination_index;         /* 0-1, how much the same memory repeats (high = bad) */
    nimcp_mental_state_t state;
    uint32_t crisis_count;          /* Times crisis state was entered */
    uint32_t dampening_active;      /* Number of memories currently dampened */
} nimcp_wellbeing_t;

/* Trauma resilience config */
typedef struct {
    /* Recall dampening */
    uint32_t max_recall_frequency;  /* Max times same engram recalled per window (default 5) */
    uint32_t recall_window_steps;   /* Window size in training steps (default 500) */
    float dampening_factor;         /* Reduce recall influence by this factor each repeat (default 0.7) */
    float min_recall_blend;         /* Floor for recall blend ratio (default 0.02) */

    /* Arousal homeostasis */
    float arousal_ceiling;          /* Max sustained arousal before dampening (default 0.85) */
    float arousal_floor;            /* Min sustained arousal before boosting (default 0.15) */
    uint32_t arousal_patience;      /* Steps of extreme arousal before intervention (default 200) */
    float homeostatic_pull;         /* Strength of pull toward baseline (default 0.01 per step) */

    /* Emotional habituation */
    float habituation_rate;         /* How fast repeated stimuli lose impact (default 0.9 per repeat) */
    uint32_t habituation_memory;    /* How many recent recalls to track (default 100) */

    /* Wellbeing thresholds */
    float stress_threshold;         /* Wellbeing below this = STRESSED (default 0.6) */
    float distress_threshold;       /* Below this = DISTRESSED (default 0.3) */
    float crisis_threshold;         /* Below this = CRISIS (default 0.1) */
} nimcp_trauma_resilience_config_t;

typedef struct nimcp_trauma_resilience nimcp_trauma_resilience_t;

/* Lifecycle */
nimcp_trauma_resilience_t* nimcp_trauma_resilience_create(const nimcp_trauma_resilience_config_t* config);
void nimcp_trauma_resilience_destroy(nimcp_trauma_resilience_t* tr);

/**
 * @brief Modulate recall blend ratio before involuntary recall.
 *
 * Call every inference step BEFORE involuntary recall blend.
 * Returns the adjusted recall blend ratio:
 *   0.15 = normal (no dampening)
 *   0.02 = maximum suppression (min_recall_blend)
 *
 * @param tr               Trauma resilience handle
 * @param recalled_engram_id  ID of the engram about to be recalled
 * @param recall_similarity   Cosine similarity of the recall match
 * @param current_arousal     Current arousal level [0,1]
 * @return Adjusted blend ratio [min_recall_blend, 0.15]
 */
float nimcp_trauma_resilience_modulate_recall(nimcp_trauma_resilience_t* tr,
    uint64_t recalled_engram_id, float recall_similarity, float current_arousal);

/**
 * @brief Record that a memory was recalled (for frequency tracking).
 *
 * @param tr               Trauma resilience handle
 * @param engram_id        ID of the recalled engram
 * @param emotional_intensity  Emotional intensity of the recall [0,1]
 */
void nimcp_trauma_resilience_record_recall(nimcp_trauma_resilience_t* tr,
    uint64_t engram_id, float emotional_intensity);

/**
 * @brief Arousal homeostasis: regulate arousal toward baseline.
 *
 * Call every step. Returns adjusted arousal value.
 *
 * @param tr               Trauma resilience handle
 * @param current_arousal  Raw arousal level [0,1]
 * @return Adjusted arousal [0,1]
 */
float nimcp_trauma_resilience_regulate_arousal(nimcp_trauma_resilience_t* tr,
    float current_arousal);

/**
 * @brief Get current wellbeing metrics.
 *
 * @param tr        Trauma resilience handle
 * @param wellbeing Output wellbeing struct
 * @return 0 on success, -1 on error
 */
int nimcp_trauma_resilience_get_wellbeing(const nimcp_trauma_resilience_t* tr,
    nimcp_wellbeing_t* wellbeing);

/**
 * @brief Get current mental state.
 *
 * @param tr  Trauma resilience handle
 * @return Current mental state enum
 */
nimcp_mental_state_t nimcp_trauma_resilience_get_state(const nimcp_trauma_resilience_t* tr);

/**
 * @brief Force reset to healthy state (therapeutic intervention / manual override).
 *
 * @param tr  Trauma resilience handle
 * @return 0 on success, -1 on error
 */
int nimcp_trauma_resilience_reset(nimcp_trauma_resilience_t* tr);

/**
 * @brief Get human-readable name for a mental state.
 *
 * @param state  Mental state enum
 * @return Static string name
 */
const char* nimcp_mental_state_name(nimcp_mental_state_t state);

/**
 * @brief Get default configuration.
 *
 * @return Config with sensible defaults
 */
nimcp_trauma_resilience_config_t nimcp_trauma_resilience_config_default(void);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_TRAUMA_RESILIENCE_H */

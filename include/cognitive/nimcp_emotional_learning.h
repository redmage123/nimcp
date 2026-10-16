/**
 * @file nimcp_emotional_learning.h
 * @brief Emotional modulation of learning rate — arousing experiences learn faster
 */
#ifndef NIMCP_EMOTIONAL_LEARNING_H
#define NIMCP_EMOTIONAL_LEARNING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float surprise_boost;          /* LR multiplier for surprising outcomes (default 3.0) */
    float reward_boost;            /* LR multiplier for rewarded experiences (default 2.0) */
    float failure_boost;           /* LR multiplier for failures/high loss (default 2.5) */
    float boredom_decay;           /* LR multiplier for predictable experiences (default 0.5) */
    float arousal_ema_alpha;       /* EMA smoothing for arousal tracking (default 0.1) */
    float surprise_threshold;      /* Loss delta above this = surprising (default 2.0) */
} nimcp_emotional_learning_config_t;

typedef struct nimcp_emotional_learning nimcp_emotional_learning_t;

nimcp_emotional_learning_t* nimcp_emotional_learning_create(
    const nimcp_emotional_learning_config_t* config);
void nimcp_emotional_learning_destroy(nimcp_emotional_learning_t* el);

/* Compute emotionally-modulated learning rate.
 * Call BEFORE learn_vector — returns adjusted LR based on emotional state. */
float nimcp_emotional_learning_modulate(nimcp_emotional_learning_t* el,
    float base_lr, float current_loss, float reward, bool is_novel);

/* Record outcome to update emotional state */
void nimcp_emotional_learning_record(nimcp_emotional_learning_t* el,
    float loss, float reward);

/* Get current arousal level (0-1) */
float nimcp_emotional_learning_get_arousal(const nimcp_emotional_learning_t* el);

nimcp_emotional_learning_config_t nimcp_emotional_learning_config_default(void);

#ifdef __cplusplus
}
#endif
#endif

/**
 * @file nimcp_emotional_learning.c
 * @brief Emotional modulation of learning — arousing events get stronger encoding
 *
 * Biological basis: norepinephrine + cortisol during emotional arousal enhance
 * hippocampal LTP, making emotional memories stronger. We mirror this by
 * boosting learning rate for surprising, rewarding, or failure experiences.
 */
#include "cognitive/nimcp_emotional_learning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "EMOTIONAL_LEARNING"

struct nimcp_emotional_learning {
    nimcp_emotional_learning_config_t config;
    float loss_ema;             /* EMA of recent losses */
    float loss_variance_ema;    /* EMA of loss variance (for surprise detection) */
    float arousal;              /* Current arousal level 0-1 */
    float reward_ema;           /* EMA of recent rewards */
    uint32_t step_count;
};

nimcp_emotional_learning_config_t nimcp_emotional_learning_config_default(void) {
    nimcp_emotional_learning_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.surprise_boost = 3.0f;
    cfg.reward_boost = 2.0f;
    cfg.failure_boost = 2.5f;
    cfg.boredom_decay = 0.5f;
    cfg.arousal_ema_alpha = 0.1f;
    cfg.surprise_threshold = 2.0f;
    return cfg;
}

nimcp_emotional_learning_t* nimcp_emotional_learning_create(
    const nimcp_emotional_learning_config_t* config)
{
    nimcp_emotional_learning_t* el = nimcp_calloc(1, sizeof(nimcp_emotional_learning_t));
    if (!el) return NULL;
    el->config = config ? *config : nimcp_emotional_learning_config_default();
    el->arousal = 0.5f;
    el->loss_ema = 100.0f;            /* Assume moderate initial loss to avoid NaN/div-by-zero */
    el->loss_variance_ema = 100.0f;   /* Assume moderate variance */
    return el;
}

void nimcp_emotional_learning_destroy(nimcp_emotional_learning_t* el) {
    nimcp_free(el);
}

float nimcp_emotional_learning_modulate(nimcp_emotional_learning_t* el,
    float base_lr, float current_loss, float reward, bool is_novel)
{
    if (!el) return base_lr;

    float multiplier = 1.0f;
    float alpha = el->config.arousal_ema_alpha;

    /* Surprise detection: loss deviation from expected */
    float loss_delta = fabsf(current_loss - el->loss_ema);
    float loss_stddev = sqrtf(el->loss_variance_ema + 1e-8f);
    float surprise = loss_delta / (loss_stddev + 1e-8f);

    if (surprise > el->config.surprise_threshold) {
        multiplier *= el->config.surprise_boost;
        el->arousal = fminf(1.0f, el->arousal + 0.2f);
    }

    /* Reward boost */
    if (reward > 0.5f) {
        multiplier *= 1.0f + (el->config.reward_boost - 1.0f) * reward;
        el->arousal = fminf(1.0f, el->arousal + 0.1f);
    }

    /* Failure boost (high loss) */
    if (current_loss > el->loss_ema * 2.0f && el->step_count > 10) {
        multiplier *= el->config.failure_boost;
        el->arousal = fminf(1.0f, el->arousal + 0.15f);
    }

    /* Novelty boost */
    if (is_novel) {
        multiplier *= 1.5f;
    }

    /* Boredom decay (predictable = low surprise) */
    if (surprise < 0.5f && !is_novel && reward < 0.2f) {
        multiplier *= el->config.boredom_decay;
        el->arousal = fmaxf(0.0f, el->arousal - 0.05f);
    }

    /* Arousal decay toward baseline */
    el->arousal = el->arousal * 0.95f + 0.5f * 0.05f;

    /* Clamp multiplier to reasonable range */
    if (multiplier < 0.1f) multiplier = 0.1f;
    if (multiplier > 5.0f) multiplier = 5.0f;

    return base_lr * multiplier;
}

void nimcp_emotional_learning_record(nimcp_emotional_learning_t* el,
    float loss, float reward)
{
    if (!el) return;
    float alpha = el->config.arousal_ema_alpha;

    float delta = loss - el->loss_ema;
    el->loss_ema = el->loss_ema * (1.0f - alpha) + loss * alpha;
    el->loss_variance_ema = el->loss_variance_ema * (1.0f - alpha) + delta * delta * alpha;
    el->reward_ema = el->reward_ema * (1.0f - alpha) + reward * alpha;
    el->step_count++;
}

float nimcp_emotional_learning_get_arousal(const nimcp_emotional_learning_t* el) {
    return el ? el->arousal : 0.5f;
}

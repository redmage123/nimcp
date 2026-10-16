/**
 * @file nimcp_food_reward.h
 * @brief Food Reward Processing Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * Implements food reward processing including palatability evaluation,
 * nutritional value estimation, and reward learning. Integrates with
 * hypothalamus for hunger/satiety modulation.
 */

#ifndef NIMCP_FOOD_REWARD_H
#define NIMCP_FOOD_REWARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#define FOOD_REWARD_MAX_MEMORY  100
#define FOOD_REWARD_DECAY_TAU   86400.0f /* 24 hours */

typedef struct {
    float reward_sensitivity;
    float satiety_effect;       /* How much satiety reduces reward */
    float novelty_bonus;
    float learning_rate;
    bool enable_conditioned_preference;
} food_reward_config_t;

typedef struct {
    uint32_t food_id;
    char name[64];
    float learned_reward;
    float nutritional_value;
    food_category_t category;
    bool is_safe;
    uint32_t encounter_count;
    uint64_t last_encounter;
} food_memory_t;

typedef struct food_reward_ctx* food_reward_ctx_t;

food_reward_config_t food_reward_default_config(void);
food_reward_ctx_t food_reward_create(const food_reward_config_t* config);
void food_reward_destroy(food_reward_ctx_t ctx);

int food_reward_compute(food_reward_ctx_t ctx, const taste_perception_t* perception, float hunger_level, float* reward);
int food_reward_estimate_nutrition(food_reward_ctx_t ctx, const taste_perception_t* perception, float* energy, float* protein, float* fat);
int food_reward_learn(food_reward_ctx_t ctx, const taste_perception_t* perception, float post_ingestive_signal, float learning_rate);
int food_reward_apply_satiety(food_reward_ctx_t ctx, float satiety_level, float* modulated_reward);
float food_reward_get_novelty(food_reward_ctx_t ctx, const taste_perception_t* perception);
int food_reward_store_memory(food_reward_ctx_t ctx, const char* name, const taste_perception_t* perception, food_category_t category, uint32_t* food_id);
int food_reward_recall(food_reward_ctx_t ctx, const taste_perception_t* perception, food_memory_t* memory, float* match_score);
bool food_reward_is_safe(food_reward_ctx_t ctx, const taste_perception_t* perception);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FOOD_REWARD_H */

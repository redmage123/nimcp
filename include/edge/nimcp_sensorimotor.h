/**
 * @file nimcp_sensorimotor.h
 * @brief Closed-loop sensorimotor controller — sensor → brain → motor → environment
 */
#ifndef NIMCP_SENSORIMOTOR_H
#define NIMCP_SENSORIMOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NIMCP_SM_MODE_INFERENCE_ONLY = 0,
    NIMCP_SM_MODE_LEARNING,
    NIMCP_SM_MODE_REINFORCEMENT,
} nimcp_sm_mode_t;

typedef struct {
    nimcp_sm_mode_t mode;
    float loop_hz;
    float learning_rate;
    float reward_discount;
    float exploration_noise;
    uint32_t max_episode_steps;
    uint32_t num_episodes;
    float curiosity_weight;
    bool enable_domain_randomization;
} nimcp_sm_config_t;

typedef struct {
    uint32_t episode_id;
    uint32_t num_steps;
    float total_reward;
    float avg_reward;
    float max_reward;
    bool terminated;
    float episode_time_sec;
} nimcp_episode_stats_t;

typedef struct {
    uint32_t total_episodes;
    uint32_t total_steps;
    float mean_episode_reward;
    float best_episode_reward;
    float mean_episode_length;
    float current_exploration;
    uint32_t collisions;
} nimcp_sm_stats_t;

typedef struct nimcp_sensorimotor nimcp_sensorimotor_t;

nimcp_sensorimotor_t* nimcp_sensorimotor_create(
    void* brain, void* sim_bridge, void* motor_output,
    void* sensor_hub, void* safety_watchdog,
    const nimcp_sm_config_t* config);
void nimcp_sensorimotor_destroy(nimcp_sensorimotor_t* sm);

int nimcp_sensorimotor_run_episode(nimcp_sensorimotor_t* sm, nimcp_episode_stats_t* stats_out);
int nimcp_sensorimotor_train(nimcp_sensorimotor_t* sm, uint32_t num_episodes, nimcp_sm_stats_t* stats_out);
int nimcp_sensorimotor_step(nimcp_sensorimotor_t* sm, float* reward_out, bool* done_out);
int nimcp_sensorimotor_reset(nimcp_sensorimotor_t* sm);
int nimcp_sensorimotor_get_observation(const nimcp_sensorimotor_t* sm, float* features, uint32_t max_features);
int nimcp_sensorimotor_get_stats(const nimcp_sensorimotor_t* sm, nimcp_sm_stats_t* stats);
int nimcp_sensorimotor_set_exploration(nimcp_sensorimotor_t* sm, float noise);
nimcp_sm_config_t nimcp_sensorimotor_config_default(void);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SENSORIMOTOR_H */

/**
 * @file nimcp_sensorimotor.c
 * @brief Closed-loop sensorimotor controller implementation
 *
 * WHAT: Connects sensor → brain → motor → environment in a continuous loop
 * WHY:  Enables embodied learning through action-consequence feedback
 * HOW:  Steps sim, composes observation, runs brain inference, translates to
 *       motor commands, feeds back to sim. Supports RL + curiosity reward.
 */

#include "edge/nimcp_sensorimotor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define LOG_MODULE "SENSORIMOTOR"

/* Forward declarations — avoid circular header deps */
extern void* nimcp_sim_state_create(uint32_t num_joints);
extern void nimcp_sim_state_destroy(void* state);
extern int nimcp_sim_bridge_step(void* sim, const float* actions, uint32_t n, void* state);
extern int nimcp_sim_bridge_reset(void* sim, void* state);
extern int nimcp_sim_bridge_compose_sensors(const void* sim, const void* state,
                                             float* features, uint32_t max);
extern int nimcp_sim_bridge_randomize(void* sim);
extern int nimcp_motor_translate(void* motor, const float* brain_out,
                                  uint32_t n_out, float* cmd, uint32_t max);
extern uint32_t nimcp_motor_get_num_channels(const void* motor);
extern void nimcp_watchdog_heartbeat(void* wd);
extern int nimcp_watchdog_validate_output(void* wd, float* out, uint32_t n);

/* Weak stubs for brain API — overridden by real brain when linked.
 * This allows sensorimotor to compile and link independently. */
__attribute__((weak))
int nimcp_brain_infer(void* brain, const float* features,
                       uint32_t num_features, float* outputs,
                       uint32_t num_outputs) {
    (void)brain; (void)features; (void)num_features; (void)outputs; (void)num_outputs;
    return -1;
}
__attribute__((weak))
int nimcp_brain_learn(void* brain, const float* features,
                       uint32_t n_feat, const float* target,
                       uint32_t n_tgt, const char* label,
                       float confidence, float lr) {
    (void)brain; (void)features; (void)n_feat; (void)target; (void)n_tgt;
    (void)label; (void)confidence; (void)lr;
    return -1;
}
__attribute__((weak))
int nimcp_brain_update_reward(void* brain, float reward, float confidence) {
    (void)brain; (void)reward; (void)confidence;
    return -1;
}

/* Sim state accessors */
typedef struct {
    float* joint_positions;
    float* joint_velocities;
    float* body_position;
    float* body_orientation;
    float* body_velocity;
    float* body_angular_velocity;
    uint32_t num_joints;
    float sim_time;
    bool collision_detected;
    float reward;
} sm_sim_state_t;

#define OBS_DIM 128
#define BRAIN_OUT_DIM 4096
#define MAX_ACTIONS 32

struct nimcp_sensorimotor {
    void* brain;
    void* sim;
    void* motor;
    void* sensor_hub;
    void* watchdog;
    nimcp_sm_config_t config;

    void* current_state;
    void* prev_state;
    float observation[OBS_DIM];
    float brain_output[BRAIN_OUT_DIM];
    float action[MAX_ACTIONS];
    uint32_t obs_dim;
    uint32_t action_dim;

    uint32_t episode_step;
    float episode_reward;
    float max_step_reward;

    float curiosity_ema;
    uint32_t rng_state;

    nimcp_sm_stats_t stats;
    uint64_t last_step_ts;
    float step_interval_us;
};

/* Simple xorshift PRNG */
static uint32_t _xorshift(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *state = x;
    return x;
}

/* Gaussian noise via Box-Muller */
static float _gaussian(uint32_t* rng) {
    float u1 = (_xorshift(rng) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    float u2 = (_xorshift(rng) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
}

nimcp_sm_config_t nimcp_sensorimotor_config_default(void) {
    nimcp_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = NIMCP_SM_MODE_REINFORCEMENT;
    cfg.loop_hz = 30.0f;
    cfg.learning_rate = 0.001f;
    cfg.reward_discount = 0.99f;
    cfg.exploration_noise = 0.1f;
    cfg.max_episode_steps = 1000;
    cfg.num_episodes = 100;
    cfg.curiosity_weight = 0.1f;
    cfg.enable_domain_randomization = true;
    return cfg;
}

nimcp_sensorimotor_t* nimcp_sensorimotor_create(
    void* brain, void* sim_bridge, void* motor_output,
    void* sensor_hub, void* safety_watchdog,
    const nimcp_sm_config_t* config)
{
    if (!brain || !sim_bridge || !motor_output) {
        LOG_ERROR("[%s] create: brain, sim, and motor are required", LOG_MODULE);
        return NULL;
    }

    nimcp_sensorimotor_t* sm = nimcp_calloc(1, sizeof(nimcp_sensorimotor_t));
    if (!sm) return NULL;

    sm->brain = brain;
    sm->sim = sim_bridge;
    sm->motor = motor_output;
    sm->sensor_hub = sensor_hub;
    sm->watchdog = safety_watchdog;
    sm->config = config ? *config : nimcp_sensorimotor_config_default();

    sm->current_state = nimcp_sim_state_create(8);
    sm->prev_state = nimcp_sim_state_create(8);
    if (!sm->current_state || !sm->prev_state) {
        nimcp_sim_state_destroy(sm->current_state);
        nimcp_sim_state_destroy(sm->prev_state);
        nimcp_free(sm);
        return NULL;
    }

    sm->obs_dim = OBS_DIM;
    sm->action_dim = nimcp_motor_get_num_channels(motor_output);
    if (sm->action_dim == 0 || sm->action_dim > MAX_ACTIONS)
        sm->action_dim = 4;

    sm->rng_state = (uint32_t)(nimcp_time_now_us() & 0xFFFFFFFF);
    if (sm->rng_state == 0) sm->rng_state = 42;

    sm->step_interval_us = 1000000.0f / sm->config.loop_hz;
    sm->curiosity_ema = 0.0f;

    LOG_INFO("[%s] Created (mode=%d, hz=%.0f, episodes=%u, explore=%.2f)",
             LOG_MODULE, sm->config.mode, sm->config.loop_hz,
             sm->config.num_episodes, sm->config.exploration_noise);
    return sm;
}

void nimcp_sensorimotor_destroy(nimcp_sensorimotor_t* sm) {
    if (!sm) return;
    nimcp_sim_state_destroy(sm->current_state);
    nimcp_sim_state_destroy(sm->prev_state);
    nimcp_free(sm);
}

int nimcp_sensorimotor_reset(nimcp_sensorimotor_t* sm) {
    if (!sm) return -1;

    if (sm->config.enable_domain_randomization)
        nimcp_sim_bridge_randomize(sm->sim);

    int rc = nimcp_sim_bridge_reset(sm->sim, sm->current_state);
    sm->episode_step = 0;
    sm->episode_reward = 0.0f;
    sm->max_step_reward = 0.0f;
    memset(sm->observation, 0, sizeof(sm->observation));
    memset(sm->brain_output, 0, sizeof(sm->brain_output));
    memset(sm->action, 0, sizeof(sm->action));

    /* Compose initial observation */
    nimcp_sim_bridge_compose_sensors(sm->sim, sm->current_state,
                                     sm->observation, sm->obs_dim);
    return rc;
}

int nimcp_sensorimotor_step(nimcp_sensorimotor_t* sm, float* reward_out, bool* done_out) {
    if (!sm) return -1;

    uint64_t step_start = nimcp_time_now_us();

    /* 1. Compose observation from current sim state */
    int n_obs = nimcp_sim_bridge_compose_sensors(sm->sim, sm->current_state,
                                                  sm->observation, sm->obs_dim);
    if (n_obs <= 0) n_obs = (int)sm->obs_dim;
    if ((uint32_t)n_obs > sm->obs_dim) n_obs = (int)sm->obs_dim; /* Clamp to buffer size */

    /* 2. Brain inference */
    nimcp_brain_infer(sm->brain, sm->observation, (uint32_t)n_obs,
                      sm->brain_output, BRAIN_OUT_DIM);

    /* 3. Add exploration noise */
    if (sm->config.exploration_noise > 0.0f) {
        for (uint32_t i = 0; i < BRAIN_OUT_DIM && i < 32; i++) {
            sm->brain_output[i] += _gaussian(&sm->rng_state) *
                                    sm->config.exploration_noise;
        }
    }

    /* 4. Motor translate: brain output → motor commands */
    nimcp_motor_translate(sm->motor, sm->brain_output, BRAIN_OUT_DIM,
                                 sm->action, sm->action_dim);

    /* 5. Safety watchdog validates motor commands */
    if (sm->watchdog) {
        nimcp_watchdog_heartbeat(sm->watchdog);
        nimcp_watchdog_validate_output(sm->watchdog, sm->action, sm->action_dim);
    }

    /* 6. Save previous state, step simulation */
    void* tmp = sm->prev_state;
    sm->prev_state = sm->current_state;
    sm->current_state = tmp;
    nimcp_sim_bridge_step(sm->sim, sm->action, sm->action_dim, sm->current_state);

    /* 7. Compute reward */
    sm_sim_state_t* state = (sm_sim_state_t*)sm->current_state;
    float external_reward = state ? state->reward : 0.0f;

    /* Curiosity: prediction error as intrinsic reward */
    float new_obs[OBS_DIM];
    nimcp_sim_bridge_compose_sensors(sm->sim, sm->current_state, new_obs, sm->obs_dim);
    float obs_magnitude = 0.0f;
    for (uint32_t i = 0; i < sm->obs_dim && i < OBS_DIM; i++)
        obs_magnitude += new_obs[i] * new_obs[i];
    obs_magnitude = sqrtf(obs_magnitude / (float)sm->obs_dim);

    float prediction_error = fabsf(obs_magnitude - sm->curiosity_ema);
    sm->curiosity_ema = 0.95f * sm->curiosity_ema + 0.05f * obs_magnitude;
    float intrinsic_reward = prediction_error * sm->config.curiosity_weight;

    float total_reward = external_reward + intrinsic_reward;

    /* 8. Learning */
    if (sm->config.mode == NIMCP_SM_MODE_LEARNING) {
        nimcp_brain_learn(sm->brain, sm->observation, (uint32_t)n_obs,
                          new_obs, (uint32_t)n_obs, "sensorimotor",
                          0.5f, sm->config.learning_rate);
    } else if (sm->config.mode == NIMCP_SM_MODE_REINFORCEMENT) {
        nimcp_brain_update_reward(sm->brain, total_reward,
                                  total_reward > 0.5f ? 0.8f : 0.3f);
    }

    /* 9. Update tracking */
    sm->episode_step++;
    sm->episode_reward += total_reward;
    if (total_reward > sm->max_step_reward)
        sm->max_step_reward = total_reward;
    sm->stats.total_steps++;

    /* 10. Check termination */
    bool done = false;
    if (state && state->collision_detected) {
        done = true;
        sm->stats.collisions++;
    }
    if (sm->episode_step >= sm->config.max_episode_steps)
        done = true;

    if (reward_out) *reward_out = total_reward;
    if (done_out) *done_out = done;

    /* 11. Rate limit */
    uint64_t elapsed = nimcp_time_now_us() - step_start;
    if (elapsed < (uint64_t)sm->step_interval_us) {
        usleep((useconds_t)(sm->step_interval_us - elapsed));
    }

    return 0;
}

int nimcp_sensorimotor_run_episode(nimcp_sensorimotor_t* sm,
                                    nimcp_episode_stats_t* stats_out)
{
    if (!sm) return -1;

    uint64_t ep_start = nimcp_time_now_us();
    nimcp_sensorimotor_reset(sm);

    float reward;
    bool done = false;
    while (!done) {
        nimcp_sensorimotor_step(sm, &reward, &done);
    }

    sm->stats.total_episodes++;

    if (stats_out) {
        stats_out->episode_id = sm->stats.total_episodes;
        stats_out->num_steps = sm->episode_step;
        stats_out->total_reward = sm->episode_reward;
        stats_out->avg_reward = sm->episode_step > 0
            ? sm->episode_reward / (float)sm->episode_step : 0.0f;
        stats_out->max_reward = sm->max_step_reward;
        stats_out->terminated = sm->episode_step < sm->config.max_episode_steps;
        stats_out->episode_time_sec = (float)(nimcp_time_now_us() - ep_start) / 1e6f;
    }

    LOG_INFO("[%s] Episode %u: %u steps, reward=%.3f, %s",
             LOG_MODULE, sm->stats.total_episodes, sm->episode_step,
             sm->episode_reward,
             sm->episode_step < sm->config.max_episode_steps ? "TERMINATED" : "MAX_STEPS");
    return 0;
}

int nimcp_sensorimotor_train(nimcp_sensorimotor_t* sm, uint32_t num_episodes,
                              nimcp_sm_stats_t* stats_out)
{
    if (!sm) return -1;

    float best_reward = -1e9f;
    float total_reward_sum = 0.0f;
    float total_length_sum = 0.0f;

    for (uint32_t ep = 0; ep < num_episodes; ep++) {
        nimcp_episode_stats_t ep_stats;
        nimcp_sensorimotor_run_episode(sm, &ep_stats);

        total_reward_sum += ep_stats.total_reward;
        total_length_sum += (float)ep_stats.num_steps;
        if (ep_stats.total_reward > best_reward)
            best_reward = ep_stats.total_reward;

        /* Decay exploration */
        sm->config.exploration_noise *= 0.995f;
        if (sm->config.exploration_noise < 0.01f)
            sm->config.exploration_noise = 0.01f;

        if ((ep + 1) % 10 == 0) {
            LOG_INFO("[%s] Training: %u/%u episodes, mean_reward=%.3f, "
                     "best=%.3f, explore=%.3f",
                     LOG_MODULE, ep + 1, num_episodes,
                     total_reward_sum / (float)(ep + 1),
                     best_reward, sm->config.exploration_noise);
        }
    }

    if (stats_out) {
        *stats_out = sm->stats;
        stats_out->mean_episode_reward = total_reward_sum / (float)num_episodes;
        stats_out->best_episode_reward = best_reward;
        stats_out->mean_episode_length = total_length_sum / (float)num_episodes;
        stats_out->current_exploration = sm->config.exploration_noise;
    }
    return 0;
}

int nimcp_sensorimotor_get_observation(const nimcp_sensorimotor_t* sm,
                                        float* features, uint32_t max_features)
{
    if (!sm || !features) return -1;
    uint32_t n = max_features < sm->obs_dim ? max_features : sm->obs_dim;
    memcpy(features, sm->observation, n * sizeof(float));
    return (int)n;
}

int nimcp_sensorimotor_get_stats(const nimcp_sensorimotor_t* sm,
                                  nimcp_sm_stats_t* stats)
{
    if (!sm || !stats) return -1;
    *stats = sm->stats;
    return 0;
}

int nimcp_sensorimotor_set_exploration(nimcp_sensorimotor_t* sm, float noise) {
    if (!sm) return -1;
    sm->config.exploration_noise = noise;
    return 0;
}

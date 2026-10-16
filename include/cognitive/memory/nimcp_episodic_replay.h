/**
 * @file nimcp_episodic_replay.h
 * @brief Episodic replay during sleep/consolidation
 *
 * WHAT: Replay important recent experiences at accelerated speed during sleep
 * WHY:  Biological brains consolidate memories during sleep via hippocampal
 *       replay — high-importance experiences are replayed to strengthen traces
 * HOW:  Circular buffer records (features, target, label, loss, reward) tuples.
 *       During consolidation, top-N by importance are replayed through
 *       brain.learn_vector at reduced learning rate.
 *
 * BIOLOGICAL BASIS:
 * - Wilson & McNaughton (1994): Hippocampal replay during sleep
 * - Diekelmann & Born (2010): Sleep-dependent memory consolidation
 * - Prioritized experience replay (Schaul et al., 2015)
 */

#ifndef NIMCP_EPISODIC_REPLAY_H
#define NIMCP_EPISODIC_REPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPISODIC_REPLAY_DEFAULT_CAPACITY  1000
#define EPISODIC_REPLAY_MAX_FEATURES      4096
#define EPISODIC_REPLAY_MAX_TARGET        4096
#define EPISODIC_REPLAY_MAX_LABEL         128

typedef struct {
    uint32_t replay_count;            /* Experiences to replay per consolidation (default 50) */
    float replay_speed_multiplier;    /* Compressed replay speed (default 10x) */
    float importance_threshold;       /* Min importance to replay (default 0.5) */
    float replay_lr_scale;            /* LR multiplier during replay (default 0.3) */
    bool prioritize_high_loss;        /* Prioritize high-loss experiences (default true) */
    bool prioritize_high_reward;      /* Prioritize high-reward experiences (default true) */
    uint32_t buffer_capacity;         /* Circular buffer size (default 1000) */
} nimcp_episodic_replay_config_t;

typedef struct nimcp_episodic_replay nimcp_episodic_replay_t;

/* Forward declare brain handle */
typedef struct nimcp_brain_handle* nimcp_brain_t;

/* Lifecycle */
nimcp_episodic_replay_t* nimcp_episodic_replay_create(const nimcp_episodic_replay_config_t* config);
void nimcp_episodic_replay_destroy(nimcp_episodic_replay_t* handle);

/**
 * @brief Record an experience during training
 *
 * @param handle       Replay handle
 * @param features     Input feature vector
 * @param num_features Number of features
 * @param target       Target output vector
 * @param target_size  Size of target
 * @param label        Text label (may be NULL)
 * @param loss         Training loss for this example
 * @param reward       Reward signal for this example
 * @return 0 on success, -1 on failure
 */
int nimcp_episodic_replay_record(nimcp_episodic_replay_t* handle,
    const float* features, uint32_t num_features,
    const float* target, uint32_t target_size,
    const char* label, float loss, float reward);

/**
 * @brief Consolidate memories during sleep — replay top experiences
 *
 * Selects top-N experiences by importance score:
 *   importance = loss * 0.5 + reward * 0.3 + recency * 0.2
 * Replays each through brain.learn_vector at reduced learning rate.
 *
 * @param handle        Replay handle
 * @param brain         Brain handle for learn_vector calls
 * @param learning_rate Base learning rate (will be scaled by replay_lr_scale)
 * @return Number of experiences replayed, or -1 on failure
 */
int nimcp_episodic_replay_consolidate(nimcp_episodic_replay_t* handle,
    nimcp_brain_t brain, float learning_rate);

/**
 * @brief Consolidate using internal brain_t pointer (for sleep cycle)
 *
 * Same behavior as nimcp_episodic_replay_consolidate but takes the internal
 * brain_t (passed as void* to keep this header free of the internal brain
 * header) and calls the internal brain_learn_vector directly. Used by the
 * sleep-wake deep-NREM stage, which holds an internal brain reference and
 * cannot form the public nimcp_brain_t handle without circular includes.
 *
 * @param handle          Replay handle
 * @param brain_internal  Internal brain_t pointer (from sleep->brain_ref)
 * @param learning_rate   Base learning rate (scaled by replay_lr_scale)
 * @return Number of experiences replayed, or -1 on failure
 */
int nimcp_episodic_replay_consolidate_internal(nimcp_episodic_replay_t* handle,
    void* brain_internal, float learning_rate);

/**
 * @brief REM-style random recombination pass (for sleep_stage_rem).
 *
 * Samples random pairs of episodic experiences and replays linearly-mixed
 * feature vectors at very low learning rate. Biologically analogous to
 * REM sleep's creative recombination — novel feature combinations are
 * presented to the network, enabling new associations to form without
 * disrupting consolidated memory. Uses internal brain_t, not the public
 * handle.
 *
 * @param handle         Replay handle
 * @param brain_internal Internal brain_t pointer
 * @param learning_rate  Base LR (will be scaled by replay_lr_scale * 0.3)
 * @param num_pairs      Number of random pairs to blend (default 8)
 * @return Number of pairs actually replayed, or -1 on failure
 */
int nimcp_episodic_replay_rem_recombine_internal(
    nimcp_episodic_replay_t* handle,
    void* brain_internal, float learning_rate, uint32_t num_pairs);

/**
 * @brief Get current number of experiences in buffer
 */
uint32_t nimcp_episodic_replay_get_buffer_size(const nimcp_episodic_replay_t* handle);

/**
 * @brief Return default configuration
 */
nimcp_episodic_replay_config_t nimcp_episodic_replay_config_default(void);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_EPISODIC_REPLAY_H */

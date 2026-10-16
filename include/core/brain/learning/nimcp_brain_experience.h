/**
 * @file nimcp_brain_experience.h
 * @brief Unified Experience API — merged inference + learning pipeline
 *
 * WHAT: brain_experience() combines perception and learning in a single call.
 *       Every sensory experience triggers both prediction and plasticity.
 *
 * WHY:  Biological brains don't separate "inference" from "learning."
 *       Every perception modifies the brain. This enables developmental
 *       learning where Claude teaches Athena through interaction, not
 *       batch training.
 *
 * HOW:  Forward pass → prediction error (vs world model) → attention-gated
 *       plasticity → optional teacher reward signal → world model update
 *
 * ARCHITECTURE:
 *   input → forward_pass → output (prediction)
 *                ↓
 *        prediction_error = |predicted - observed|
 *                ↓
 *        attention_gate (thalamic modulation)
 *                ↓
 *        if (attention > threshold):
 *            Hebbian/STDP plasticity (active pathways)
 *            TPB reward learning (if teacher signal present)
 *            World model update
 *                ↓
 *        return {output, prediction_error, attention_level, learning_applied}
 */

#ifndef NIMCP_BRAIN_EXPERIENCE_H
#define NIMCP_BRAIN_EXPERIENCE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Result of a brain experience (unified infer+learn)
 */
typedef struct {
    float prediction_error;     /**< How surprised the brain was (0=expected, 1=novel) */
    float attention_level;      /**< Thalamic attention during this experience (0-1) */
    float learning_rate_used;   /**< Effective LR after attention modulation */
    bool  learning_applied;     /**< Whether plasticity was triggered */
    bool  synapse_formed;       /**< Whether synaptogenesis occurred this experience */
    float reward_signal;        /**< Teacher reward if provided (-1 to 1, 0=none) */
    uint64_t experience_id;     /**< Monotonic experience counter */
} brain_experience_result_t;

/**
 * @brief Configuration for experience-based learning
 */
typedef struct {
    bool enabled;                       /**< Master switch for inference learning */
    float base_learning_rate;           /**< Base LR for experience learning (default 0.001) */
    float attention_threshold;          /**< Min attention to trigger learning (default 0.3) */
    float attention_lr_scale;           /**< How much attention scales LR (default 3.0) */
    float novelty_boost;               /**< Extra LR boost for novel stimuli (default 1.5) */
    bool enable_hebbian;               /**< Enable Hebbian/STDP during experience */
    bool enable_reward_learning;        /**< Enable reward-based weight updates */
    bool enable_world_model_update;     /**< Update world model with each experience */
    bool enable_structural_plasticity;  /**< Allow new synapse formation during experience */
    float synaptogenesis_threshold;     /**< Min prediction error to trigger synaptogenesis (default 0.7) */
    uint32_t consolidation_interval;    /**< Auto-consolidate every N experiences (0=disabled) */
} brain_experience_config_t;

/**
 * @brief Get default experience configuration
 */
brain_experience_config_t brain_experience_default_config(void);

/**
 * @brief Configure experience-based learning on a brain
 *
 * @param brain     Brain instance
 * @param config    Experience configuration
 * @return 0 on success, -1 on error
 */
int brain_experience_configure(brain_t brain, const brain_experience_config_t* config);

/**
 * @brief Unified experience: perceive input, predict output, and learn
 *
 * This is the core developmental learning function. It:
 * 1. Runs forward pass to generate prediction (output)
 * 2. Computes prediction error against world model expectation
 * 3. Gates learning by thalamic attention level
 * 4. Applies lightweight plasticity (Hebbian/STDP) on active pathways
 * 5. If teacher_reward != 0, applies reward-based learning via TPB
 * 6. Updates world model with observed input
 *
 * @param brain          Brain instance
 * @param input          Input features (sensory stimulus)
 * @param input_size     Number of input features
 * @param output         Output buffer (brain's prediction/response)
 * @param output_size    Output buffer size
 * @param teacher_reward Optional teacher signal: -1.0 (wrong) to 1.0 (correct), 0=no signal
 * @param result         Output: experience result with metrics
 * @return true on success, false on error
 */
bool brain_experience(
    brain_t brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    float teacher_reward,
    brain_experience_result_t* result
);

/**
 * @brief Provide a correction (supervised teaching signal) after an experience
 *
 * Called by the teacher when Athena's response was wrong.
 * Uses the full training pipeline with the expected output as target.
 *
 * @param brain          Brain instance
 * @param expected       Correct output vector
 * @param expected_size  Size of expected output
 * @return Loss value (0=perfect), -1.0 on error
 */
float brain_experience_correct(
    brain_t brain,
    const float* expected,
    uint32_t expected_size
);

/**
 * @brief Direct attention to a specific modality
 *
 * Modulates thalamic gating to prioritize a sensory channel.
 *
 * @param brain     Brain instance
 * @param modality  Modality string: "visual", "auditory", "speech", "somatosensory"
 * @param strength  Attention strength (0.0 to 1.0)
 * @return 0 on success
 */
int brain_experience_attend(brain_t brain, const char* modality, float strength);

#endif /* NIMCP_BRAIN_EXPERIENCE_H */

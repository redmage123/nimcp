/**
 * @file nimcp_brain_innate.h
 * @brief Innate Circuit Hardwiring — pre-configured infant capabilities
 *
 * WHAT: Pre-wires biologically-inspired innate circuits at brain creation time
 * WHY:  Human infants aren't blank slates — they have built-in biases for faces,
 *       voices, biological motion, and basic reflexes. Athena starts the same way.
 * HOW:  Sets specific neuron biases and connection weights in designated regions
 *       of the neural network to create functional innate circuits.
 *
 * CIRCUITS:
 *   1. Face attention bias — boosted weights for face-like spatial frequencies
 *   2. Voice attention bias — boosted weights for human voice F0 (100-400Hz)
 *   3. Biological motion bias — weights for articulated motion patterns
 *   4. Spinal reflexes — rooting, grasping, Moro (startle)
 *   5. Cry vocalization — distress → motor pattern for crying
 *   6. Social reward — face/voice detection → dopamine boost
 *   7. Habituation — repeated stimulus → decreased attention
 *   8. Novelty preference — new stimulus → increased attention
 */

#ifndef NIMCP_BRAIN_INNATE_H
#define NIMCP_BRAIN_INNATE_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

/**
 * @brief Developmental stage for innate circuit configuration
 */
typedef enum {
    INNATE_STAGE_NEWBORN = 0,   /**< Reflexes only, basic sensory, high habituation */
    INNATE_STAGE_INFANT  = 1,   /**< 2-6mo: Object tracking, voice recognition, social smile */
    INNATE_STAGE_CRAWLER = 2,   /**< 6-12mo: Object permanence, babbling, imitation */
    INNATE_STAGE_TODDLER = 3,   /**< 12-24mo: First words, tool use, pointing */
    INNATE_STAGE_CHILD   = 4,   /**< 2-4yr: Grammar, theory of mind, why questions */
    INNATE_STAGE_COUNT
} innate_stage_t;

/**
 * @brief Innate circuit configuration
 */
typedef struct {
    innate_stage_t stage;               /**< Developmental stage */
    bool enable_face_bias;              /**< Face attention bias (LGN pathway) */
    bool enable_voice_bias;             /**< Voice attention bias (MGN pathway) */
    bool enable_motion_bias;            /**< Biological motion bias (V5/MT) */
    bool enable_reflexes;               /**< Spinal reflexes (rooting, grasping, Moro) */
    bool enable_cry;                    /**< Cry vocalization circuit */
    bool enable_social_reward;          /**< Social reward (face/voice → dopamine) */
    bool enable_habituation;            /**< Habituation circuit */
    bool enable_novelty;                /**< Novelty preference circuit */
    float bias_strength;                /**< Strength of innate biases (0.0-1.0, default 0.5) */
} innate_config_t;

/**
 * @brief Get default innate configuration for a given stage
 */
innate_config_t innate_default_config(innate_stage_t stage);

/**
 * @brief Hardwire innate circuits into a brain
 *
 * Should be called after brain creation but before any experience/training.
 * Modifies neuron biases and connection weights to create innate circuits.
 *
 * @param brain  Brain instance (must have network initialized)
 * @param config Innate circuit configuration
 * @return 0 on success, -1 on error
 */
int brain_innate_hardwire(brain_t brain, const innate_config_t* config);

/**
 * @brief Get the current developmental stage name as string
 */
const char* innate_stage_name(innate_stage_t stage);

#endif /* NIMCP_BRAIN_INNATE_H */

//=============================================================================
// nimcp_aesthetic_evaluation.h - Aesthetic Quality Evaluation
//=============================================================================
/**
 * @file nimcp_aesthetic_evaluation.h
 * @brief Evaluates aesthetic quality of artistic content
 *
 * WHAT: Assesses artistic quality using Berlyne's aesthetics theory
 * WHY:  Enable AI to appreciate and judge art quality
 * HOW:  Multi-dimensional evaluation considering novelty, complexity, etc.
 *
 * THEORETICAL BASIS:
 * - Berlyne's Aesthetics (1971): Novelty, complexity, arousal potential
 * - Plutchik's Emotion Wheel: Basic emotional responses
 * - Aesthetic Distance Theory: Optimal engagement level
 * - Information Theory: Entropy-based complexity measures
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_AESTHETIC_EVALUATION_H
#define NIMCP_AESTHETIC_EVALUATION_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Aesthetic evaluator configuration
 */
typedef struct {
    /* Dimension weights */
    float novelty_weight;           /**< [0-1] Weight for novelty dimension */
    float complexity_weight;        /**< [0-1] Weight for complexity dimension */
    float familiarity_weight;       /**< [0-1] Weight for familiarity dimension */
    float hedonic_weight;           /**< [0-1] Weight for hedonic tone */
    float arousal_weight;           /**< [0-1] Weight for arousal potential */

    /* Quality thresholds */
    float min_quality_threshold;    /**< Minimum acceptable quality */
    float max_complexity_preference; /**< Preferred max complexity (inverted U) */
    float optimal_novelty;          /**< Optimal novelty level (inverted U) */

    /* Modality-specific adjustments */
    bool enable_text_analysis;      /**< Enable text-specific analysis */
    bool enable_music_analysis;     /**< Enable music-specific analysis */
    bool enable_visual_analysis;    /**< Enable visual-specific analysis */

    /* Integration flags */
    bool use_emotion_system;        /**< Use connected emotion system */
    bool use_memory_system;         /**< Use connected memory for familiarity */

    /* Resource limits */
    uint32_t max_analysis_time_ms;  /**< Maximum analysis time */
} aesthetic_evaluator_config_t;

/**
 * @brief Initialize evaluator config with defaults
 */
void aesthetic_evaluator_config_defaults(aesthetic_evaluator_config_t* config);

//=============================================================================
// Evaluator Structure
//=============================================================================

/**
 * @brief Aesthetic evaluator state
 */
struct aesthetic_evaluator {
    aesthetic_evaluator_config_t config;

    /* Analysis models */
    void* text_analyzer;            /**< Text analysis model */
    void* music_analyzer;           /**< Music analysis model */
    void* visual_analyzer;          /**< Visual analysis model */

    /* Feature extractors */
    void* novelty_detector;         /**< Novelty detection model */
    void* complexity_analyzer;      /**< Complexity analysis model */
    void* emotion_extractor;        /**< Emotion extraction model */

    /* Reference databases */
    void* style_database;           /**< Database of known styles */
    void* work_database;            /**< Database of known works */

    /* Integration pointers */
    void* emotion_system;           /**< External emotion system */
    void* memory_system;            /**< External memory system */

    /* Cortical integration pointers */
    void* visual_cortex;            /**< Visual cortex (V1-V5) for visual quality */
    void* audio_cortex;             /**< Audio cortex (A1) for audio quality */
    void* speech_cortex;            /**< Speech cortex for prosody analysis */

    /* Statistics */
    uint64_t total_evaluations;
    float avg_quality_score;
    uint64_t last_evaluation_us;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create aesthetic evaluator
 *
 * @param config Configuration (NULL for defaults)
 * @return Evaluator or NULL on error
 */
aesthetic_evaluator_t* aesthetic_evaluator_create(
    const aesthetic_evaluator_config_t* config);

/**
 * @brief Destroy aesthetic evaluator
 *
 * @param eval Evaluator to destroy
 */
void aesthetic_evaluator_destroy(aesthetic_evaluator_t* eval);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate text content aesthetically
 *
 * Analyzes literary quality considering:
 * - Prose rhythm and flow
 * - Vocabulary richness
 * - Structural complexity
 * - Emotional resonance
 * - Originality of expression
 *
 * @param eval Evaluator
 * @param text Text content
 * @param len Text length in bytes
 * @param modality Text modality (poetry, prose, etc.)
 * @param out Output evaluation result
 * @return 0 on success, -1 on error
 */
int aesthetic_evaluate_text(aesthetic_evaluator_t* eval,
                            const char* text, size_t len,
                            art_modality_t modality,
                            aesthetic_evaluation_t* out);

/**
 * @brief Evaluate music content aesthetically
 *
 * Analyzes musical quality considering:
 * - Melodic interest and development
 * - Harmonic richness
 * - Rhythmic complexity
 * - Structural coherence
 * - Emotional expressiveness
 *
 * @param eval Evaluator
 * @param tracks Music tracks
 * @param num_tracks Number of tracks
 * @param out Output evaluation result
 * @return 0 on success, -1 on error
 */
int aesthetic_evaluate_music(aesthetic_evaluator_t* eval,
                             const music_track_t* tracks, uint32_t num_tracks,
                             aesthetic_evaluation_t* out);

/**
 * @brief Evaluate visual content aesthetically
 *
 * Analyzes visual quality considering:
 * - Composition and balance
 * - Color harmony
 * - Technical execution
 * - Visual complexity
 * - Emotional impact
 *
 * @param eval Evaluator
 * @param image Image to evaluate
 * @param out Output evaluation result
 * @return 0 on success, -1 on error
 */
int aesthetic_evaluate_visual(aesthetic_evaluator_t* eval,
                              const visual_image_t* image,
                              aesthetic_evaluation_t* out);

/**
 * @brief Evaluate audio waveform aesthetically
 *
 * Direct audio analysis without MIDI representation
 *
 * @param eval Evaluator
 * @param audio_data Audio samples
 * @param num_samples Number of samples
 * @param sample_rate Sample rate
 * @param out Output evaluation result
 * @return 0 on success, -1 on error
 */
int aesthetic_evaluate_audio(aesthetic_evaluator_t* eval,
                             const float* audio_data, uint64_t num_samples,
                             uint32_t sample_rate,
                             aesthetic_evaluation_t* out);

//=============================================================================
// Dimension Analysis API
//=============================================================================

/**
 * @brief Calculate novelty score
 *
 * @param eval Evaluator
 * @param content Content (type depends on modality)
 * @param modality Content modality
 * @return Novelty score [0-1]
 */
float aesthetic_calculate_novelty(aesthetic_evaluator_t* eval,
                                  const void* content,
                                  art_modality_t modality);

/**
 * @brief Calculate complexity score
 *
 * @param eval Evaluator
 * @param content Content
 * @param modality Content modality
 * @return Complexity score [0-1]
 */
float aesthetic_calculate_complexity(aesthetic_evaluator_t* eval,
                                     const void* content,
                                     art_modality_t modality);

/**
 * @brief Calculate familiarity score
 *
 * @param eval Evaluator
 * @param content Content
 * @param modality Content modality
 * @return Familiarity score [0-1]
 */
float aesthetic_calculate_familiarity(aesthetic_evaluator_t* eval,
                                      const void* content,
                                      art_modality_t modality);

/**
 * @brief Calculate hedonic tone
 *
 * @param eval Evaluator
 * @param content Content
 * @param modality Content modality
 * @return Hedonic tone [-1, +1]
 */
float aesthetic_calculate_hedonic_tone(aesthetic_evaluator_t* eval,
                                       const void* content,
                                       art_modality_t modality);

/**
 * @brief Calculate arousal potential
 *
 * @param eval Evaluator
 * @param content Content
 * @param modality Content modality
 * @return Arousal potential [0-1]
 */
float aesthetic_calculate_arousal(aesthetic_evaluator_t* eval,
                                  const void* content,
                                  art_modality_t modality);

//=============================================================================
// Emotional Analysis API
//=============================================================================

/**
 * @brief Extract emotional response from content
 *
 * @param eval Evaluator
 * @param content Content
 * @param modality Content modality
 * @param out Output emotional response
 * @return 0 on success, -1 on error
 */
int aesthetic_extract_emotions(aesthetic_evaluator_t* eval,
                               const void* content,
                               art_modality_t modality,
                               aesthetic_emotional_response_t* out);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set emotion system for integration
 *
 * @param eval Evaluator
 * @param emotion Emotion system pointer
 */
void aesthetic_evaluator_set_emotion_system(aesthetic_evaluator_t* eval,
                                             void* emotion);

/**
 * @brief Set memory system for familiarity
 *
 * @param eval Evaluator
 * @param memory Memory system pointer
 */
void aesthetic_evaluator_set_memory_system(aesthetic_evaluator_t* eval,
                                            void* memory);

/**
 * @brief Set visual cortex for visual quality assessment
 *
 * @param eval Evaluator
 * @param visual_cortex Visual cortex pointer (V1-V5 features)
 */
void aesthetic_evaluator_set_visual_cortex(aesthetic_evaluator_t* eval,
                                            void* visual_cortex);

/**
 * @brief Set audio cortex for audio quality assessment
 *
 * @param eval Evaluator
 * @param audio_cortex Audio cortex pointer (A1 features)
 */
void aesthetic_evaluator_set_audio_cortex(aesthetic_evaluator_t* eval,
                                           void* audio_cortex);

/**
 * @brief Set speech cortex for prosody/euphony assessment
 *
 * @param eval Evaluator
 * @param speech_cortex Speech cortex pointer
 */
void aesthetic_evaluator_set_speech_cortex(aesthetic_evaluator_t* eval,
                                            void* speech_cortex);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get quality label from score
 *
 * @param score Quality score [0-1]
 * @return Label string ("poor", "fair", "good", "excellent", "masterpiece")
 */
const char* aesthetic_quality_label(float score);

/**
 * @brief Combine dimensions into overall quality
 *
 * Uses configured weights to combine Berlyne dimensions
 *
 * @param eval Evaluator (for weights)
 * @param dims Dimension values
 * @return Overall quality [0-1]
 */
float aesthetic_combine_dimensions(const aesthetic_evaluator_t* eval,
                                   const aesthetic_dimensions_t* dims);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AESTHETIC_EVALUATION_H */

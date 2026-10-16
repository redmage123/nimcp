//=============================================================================
// nimcp_brain_init_creative.h - Creative System Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_creative.h
 * @brief Creative System Initialization Functions
 *
 * WHAT: Initialization functions for creative/artistic cognitive system
 * WHY:  SRP refactoring - separate creative initialization logic
 * HOW:  Creates creative orchestrator and connects all integration bridges
 *
 * CREATIVE SYSTEM CAPABILITIES:
 * - Aesthetic Appreciation: Berlyne aesthetics, emotional response
 * - Style Learning: Style embeddings, archetype blending
 * - Content Generation: Text, music, visual, video
 * - Ethics Validation: Copyright, safety, bias detection
 *
 * INTEGRATION BRIDGES:
 * - Neural Bridge: Diffusion/GAN/API backends
 * - Ethics Bridge: Content validation
 * - Training Bridge: Style learning, LoRA
 * - Memory Bridge: Artistic preferences
 *
 * @version Phase Creative: Creative System Integration
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_BRAIN_INIT_CREATIVE_H
#define NIMCP_BRAIN_INIT_CREATIVE_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Creative System Initialization API
//=============================================================================

/**
 * @brief Initialize creative system subsystem
 *
 * WHAT: Creates creative orchestrator and connects all integration bridges
 * WHY:  Enable artistic appreciation and generation capabilities
 * HOW:  Creates orchestrator, then appreciation, inspiration, generation subsystems
 *
 * CAPABILITIES ENABLED:
 * - Aesthetic evaluation using Berlyne's aesthetics theory
 * - Style perception and representation via embeddings
 * - Influence blending from multiple artistic sources
 * - Text generation (poetry, prose, screenplay)
 * - Music generation (composition, arrangement)
 * - Visual generation (diffusion, GAN)
 * - Video generation and multimodal direction
 * - Content validation and ethics checking
 *
 * INITIALIZATION ORDER:
 * 1. Create creative orchestrator (master coordinator)
 * 2. Create appreciation subsystem (aesthetic evaluator, style perception)
 * 3. Create inspiration subsystem (style representation, influence blender)
 * 4. Create generation subsystem (text, music, visual, video generators)
 * 5. Create validation bridges (ethics, neural, training)
 * 6. Connect to emotion system
 * 7. Connect to memory system
 * 8. Connect to ethics engine
 * 9. Connect to GPU context (if available)
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_subsystem(brain_t brain);

/**
 * @brief Initialize appreciation subsystem
 *
 * WHAT: Creates aesthetic evaluator, emotion bridge, memory bridge
 * WHY:  Enable artistic appreciation capabilities
 * HOW:  Creates modules following Berlyne's aesthetics theory
 *
 * COMPONENTS:
 * - Aesthetic Evaluator: Quality assessment using novelty, complexity, etc.
 * - Emotion Bridge: Connect aesthetic response to emotion system
 * - Memory Bridge: Store artistic experiences and preferences
 * - Style Perception: Recognize and analyze artistic styles
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_appreciation(brain_t brain);

/**
 * @brief Initialize inspiration subsystem
 *
 * WHAT: Creates style representation, influence blender, pattern extractor
 * WHY:  Enable learning from and combining artistic influences
 * HOW:  Creates style embeddings and blending mechanisms
 *
 * COMPONENTS:
 * - Style Representation: Style embeddings with archetype support
 * - Influence Blender: Weighted combination of multiple styles
 * - Pattern Extractor: Extract patterns from existing artworks
 * - Knowledge Bridge: Connect to art knowledge graph
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_inspiration(brain_t brain);

/**
 * @brief Initialize generation subsystem
 *
 * WHAT: Creates text, music, visual, video generators
 * WHY:  Enable content creation capabilities
 * HOW:  Creates generator modules with configurable backends
 *
 * COMPONENTS:
 * - Text Generator: Poetry, prose, screenplay, lyrics
 * - Music Generator: Composition, arrangement, MIDI/audio export
 * - Visual Generator: Diffusion and GAN-based image generation
 * - Video Generator: Video synthesis and editing
 * - Multimodal Director: Full project coordination
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_generation(brain_t brain);

/**
 * @brief Initialize creative neural bridge
 *
 * WHAT: Creates unified interface to neural generation backends
 * WHY:  Abstract diffusion, GAN, and API backends
 * HOW:  Routes to appropriate backend based on config/availability
 *
 * BACKENDS SUPPORTED:
 * - Local Diffusion (ONNX): Stable Diffusion, SDXL
 * - Local GAN (ONNX): StyleGAN2/3, BigGAN
 * - Cloud API: Stability AI, OpenAI, Replicate
 * - Hybrid: Local first, API fallback
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_neural_bridge(brain_t brain);

/**
 * @brief Initialize creative ethics bridge
 *
 * WHAT: Creates ethics validation for creative content
 * WHY:  Ensure generated content is safe and appropriate
 * HOW:  Connects to ethics engine with creative-specific checks
 *
 * VALIDATIONS:
 * - Copyright/plagiarism detection
 * - NSFW/violence/hate content detection
 * - Deepfake/deception detection
 * - Bias and stereotyping detection
 * - Privacy (unauthorized likenesses) detection
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_ethics_bridge(brain_t brain);

/**
 * @brief Initialize creative training bridge
 *
 * WHAT: Creates training interface for creative models
 * WHY:  Enable style learning and model fine-tuning
 * HOW:  Connects to training pipeline with creative workflows
 *
 * TRAINING TYPES:
 * - Style learning from examples
 * - LoRA adapter training
 * - DreamBooth personalization
 * - Preference learning (RLHF)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_training_bridge(brain_t brain);

/**
 * @brief Initialize creative validation bridge
 *
 * WHAT: Creates main validation pipeline for creative content
 * WHY:  Defense-in-depth validation for generated content
 * HOW:  Multi-stage pipeline: quality, coherence, copyright, ethics, originality
 *
 * VALIDATION STAGES:
 * - Quality Check: Minimum aesthetic quality threshold
 * - Coherence Check: Internal consistency
 * - Copyright Check: Similarity to known works
 * - Ethics Check: Harmful content detection
 * - Originality Check: Novelty assessment
 * - Bias Check: Stereotyping and underrepresentation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_creative_bridge(brain_t brain);

//=============================================================================
// Creative System Integration API
//=============================================================================

/**
 * @brief Connect creative system to emotion system
 *
 * WHAT: Links creative to emotion for aesthetic responses
 * WHY:  Art appreciation involves emotional response
 * HOW:  Maps aesthetic dimensions to emotional states
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_emotion(brain_t brain);

/**
 * @brief Connect creative system to memory system
 *
 * WHAT: Links creative to memory for preference storage
 * WHY:  Store artistic experiences and learned preferences
 * HOW:  Registers creative as memory consumer/producer
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_memory(brain_t brain);

/**
 * @brief Connect creative system to ethics engine
 *
 * WHAT: Links creative to brain ethics for validation
 * WHY:  Ensure creative outputs meet ethical standards
 * HOW:  Routes generated content through ethics evaluation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_ethics(brain_t brain);

/**
 * @brief Connect creative system to GPU context
 *
 * WHAT: Links creative to GPU for accelerated generation
 * WHY:  Diffusion/GAN models require GPU acceleration
 * HOW:  Shares GPU context with neural bridge backends
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_gpu(brain_t brain);

/**
 * @brief Connect creative system to knowledge graph
 *
 * WHAT: Links creative to KG for art knowledge
 * WHY:  Access art history, style relationships, etc.
 * HOW:  Creates knowledge bridge to internal KG
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_knowledge(brain_t brain);

/**
 * @brief Connect creative system to training pipeline
 *
 * WHAT: Links creative to training for style learning
 * WHY:  Enable learning new styles and preferences
 * HOW:  Registers creative with brain training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_training(brain_t brain);

/**
 * @brief Connect creative system to immune system
 *
 * WHAT: Links creative to brain immune for health monitoring
 * WHY:  Monitor validation pipeline health
 * HOW:  Reports validation failures as immune events
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_immune(brain_t brain);

/**
 * @brief Connect creative system to visual cortex
 *
 * WHAT: Links creative to visual cortex for visual perception
 * WHY:  Enable visual art appreciation and generation feedback
 * HOW:  Routes visual features from V1-V5 to aesthetic evaluator
 *
 * INTEGRATION POINTS:
 * - Visual Generator: Receives style guidance from V1 edge/color analysis
 * - Aesthetic Evaluator: Uses V4 shape/color features for quality assessment
 * - Style Perception: Leverages IT area for object/style recognition
 * - Diffusion Bridge: Uses visual cortex for iterative refinement feedback
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_visual_cortex(brain_t brain);

/**
 * @brief Connect creative system to audio cortex
 *
 * WHAT: Links creative to audio cortex for auditory perception
 * WHY:  Enable music appreciation and composition feedback
 * HOW:  Routes A1 frequency/temporal features to music subsystem
 *
 * INTEGRATION POINTS:
 * - Music Generator: Receives harmonic analysis from cochlear processing
 * - Aesthetic Evaluator: Uses temporal patterns for rhythm quality
 * - Style Perception: Leverages frequency features for genre recognition
 * - Influence Blender: Uses audio memories for style learning
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_audio_cortex(brain_t brain);

/**
 * @brief Connect creative system to speech cortex
 *
 * WHAT: Links creative to speech cortex for linguistic processing
 * WHY:  Enable poetry prosody and lyrics analysis
 * HOW:  Routes phoneme/prosody features to text generation
 *
 * INTEGRATION POINTS:
 * - Text Generator: Receives prosody features for rhythm/meter
 * - Aesthetic Evaluator: Uses phonological analysis for euphony scoring
 * - Style Perception: Leverages Wernicke's area for stylistic voice
 * - Lyrics Generator: Uses phoneme patterns for rhyme/assonance
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_speech_cortex(brain_t brain);

/**
 * @brief Connect creative system to cortical columns
 *
 * WHAT: Links creative to cortical column architecture
 * WHY:  Enable hierarchical feature processing for art
 * HOW:  Routes multi-scale features through minicolumn/hypercolumn hierarchy
 *
 * INTEGRATION POINTS:
 * - Pattern Extractor: Uses columnar competition for feature selection
 * - Style Representation: Leverages hypercolumn pooling for style vectors
 * - Visual Generator: Uses predictive coding for iterative refinement
 * - Music Generator: Uses temporal columns for rhythm patterns
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_creative_to_cortical_columns(brain_t brain);

//=============================================================================
// Creative System Cleanup API
//=============================================================================

/**
 * @brief Destroy creative system subsystem
 *
 * WHAT: Cleans up all creative system resources
 * WHY:  Proper resource management on brain destruction
 * HOW:  Destroys all creative components in reverse init order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_creative_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_CREATIVE_H */

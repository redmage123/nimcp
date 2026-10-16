//=============================================================================
// nimcp_brain_internal_creative.h - Creative System Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_creative.h
 * @brief Internal brain_struct fields for creative system
 *
 * WHAT: Defines brain_struct fields for creative/artistic cognitive system
 * WHY:  Modularize brain_internal.h - separate creative fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * CREATIVE SYSTEM CAPABILITIES:
 * - Aesthetic Appreciation: Berlyne aesthetics, emotional response to art
 * - Style Representation: Style embeddings, archetype blending
 * - Content Generation: Text (poetry, prose), Music (MIDI/audio), Visual (diffusion/GAN), Video
 * - Multimodal Direction: Full film/cinema production coordination
 * - Ethics Validation: Copyright, harmful content, bias detection
 *
 * INTEGRATION BRIDGES:
 * - Neural Bridge: Unified interface to diffusion/GAN/API backends
 * - Ethics Bridge: Content validation and safety
 * - Training Bridge: Style learning, LoRA, preference learning
 * - Validation Bridge: Quality, coherence, originality checks
 *
 * REFACTORING HISTORY:
 * - Created for Phase Creative: Creative/Artistic System Integration
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase Creative: Creative System Integration
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_CREATIVE_H
#define NIMCP_BRAIN_INTERNAL_CREATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Creative Types
//=============================================================================
// Full definitions are in cognitive/creative/ headers

struct creative_orchestrator;              // Creative cognitive orchestrator
struct aesthetic_evaluator;                // Aesthetic quality assessment
struct style_representer;                  // Style embeddings and archetypes
struct influence_blender;                  // Multi-influence combination
struct creative_pattern_extractor;         // Pattern extraction from art

struct text_generator;                     // Text/poetry/screenplay generation
struct music_generator;                    // Music composition generation
struct visual_generator;                   // Visual art generation
struct video_generator;                    // Video/cinema generation
struct multimodal_director;                // Full project coordination

struct creative_bridge;                    // Content validation bridge
struct creative_neural_bridge;             // Neural backend interface
struct creative_ethics_bridge;             // Ethics validation bridge
struct creative_training_bridge;           // Training/fine-tuning bridge

struct creative_emotion_bridge;            // Emotion-aesthetic integration
struct creative_memory_bridge;             // Artistic memory integration
struct creative_knowledge_bridge;          // Art knowledge graph integration
struct style_perception;                   // Style recognition/analysis

//=============================================================================
// Creative System Fields for brain_struct
//=============================================================================
/**
 * CREATIVE SYSTEM INTEGRATION (Artistic Appreciation and Generation)
 *
 * Creative System provides artistic cognition capabilities:
 * - Aesthetic Appreciation: Evaluate artistic quality (Berlyne aesthetics)
 * - Style Learning: Learn and represent artistic styles
 * - Content Generation: Generate text, music, visual art, video
 * - Multimodal Direction: Coordinate full creative projects (films)
 * - Ethics Validation: Ensure content safety and originality
 *
 * The creative orchestrator unifies:
 * - Appreciation Subsystem: Aesthetic evaluation, emotional response, style perception
 * - Inspiration Subsystem: Style embeddings, influence blending, pattern extraction
 * - Generation Subsystem: Text, music, visual, video generators
 * - External Models: ONNX Runtime, diffusion models, GANs, cloud APIs
 * - Validation Bridges: Quality, ethics, copyright, originality
 *
 * Integrates with:
 * - Emotion System: Aesthetic emotional responses (awe, sublime, joy)
 * - Memory System: Artistic experience storage and preferences
 * - Knowledge Graph: Art knowledge, style relationships
 * - Ethics Engine: Content safety and appropriateness
 * - Brain Immune System: Validation pipeline health
 * - Training System: Style learning and preference adaptation
 * - GPU Context: Accelerated generation via diffusion/GAN
 *
 * FIELD NAMING CONVENTION:
 * - creative_*: Core creative system components
 * - creative_*_bridge: Integration bridges to other subsystems
 * - creative_enabled: Master enable flag
 * - last_creative_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining creative fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - creative_orchestrator: Master coordinator for creative subsystems
 * - aesthetic_evaluator: Aesthetic quality assessment
 * - style_representation: Style embeddings and archetype handling
 * - influence_blender: Multi-style blending
 * - creative_pattern_extractor: Pattern extraction from artworks
 * - text_generator: Poetry, prose, screenplay generation
 * - music_generator: Music composition and arrangement
 * - visual_generator: Image/art generation (diffusion/GAN)
 * - video_generator: Video/cinema generation
 * - multimodal_director: Full project coordination
 * - creative_bridge: Content validation pipeline
 * - creative_neural_bridge: Neural backend interface
 * - creative_ethics_bridge: Ethics and safety validation
 * - creative_training_bridge: Training and fine-tuning
 * - creative_emotion_bridge: Emotion-aesthetic integration
 * - creative_memory_bridge: Artistic memory integration
 * - creative_enabled: Master enable flag for creative subsystem
 * - last_creative_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_CREATIVE                                          \
    /* === CREATIVE SYSTEM INTEGRATION (Artistic Appreciation & Generation) === */ \
    struct creative_orchestrator* creative_orchestrator;    /* Master orchestrator */ \
    struct aesthetic_evaluator* aesthetic_evaluator;        /* Aesthetic quality assessment */ \
    struct style_representer* style_representation;         /* Style embeddings/archetypes */ \
    struct influence_blender* influence_blender;            /* Multi-influence blending */ \
    struct creative_pattern_extractor* creative_pattern_extractor; /* Pattern extraction */ \
    struct text_generator* text_generator;                  /* Text/poetry generation */ \
    struct music_generator* music_generator;                /* Music composition */ \
    struct visual_generator* visual_generator;              /* Visual art generation */ \
    struct video_generator* video_generator;                /* Video/cinema generation */ \
    struct multimodal_director* multimodal_director;        /* Full project direction */ \
    struct creative_bridge* creative_bridge;                /* Validation pipeline */ \
    struct creative_neural_bridge* creative_neural_bridge;  /* Neural backend interface */ \
    struct creative_ethics_bridge* creative_ethics_bridge;  /* Ethics validation */ \
    struct creative_training_bridge* creative_training_bridge; /* Training/fine-tuning */ \
    struct creative_emotion_bridge* creative_emotion_bridge; /* Emotion-aesthetic bridge */ \
    struct creative_memory_bridge* creative_memory_bridge;  /* Artistic memory bridge */ \
    struct creative_knowledge_bridge* creative_knowledge_bridge; /* Art KG bridge */ \
    struct style_perception* style_perception;              /* Style recognition */ \
    bool creative_enabled;                                  /* Creative system enabled */ \
    bool creative_lazy_init;                                /* Defer initialization */ \
    uint64_t last_creative_update_us;                       /* Last update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_CREATIVE_H */

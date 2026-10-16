# NIMCP Imagination & Generative Capabilities
## Biologically-Inspired Diffusion for Image, Video, and Audio Generation

**Version:** 1.0
**Date:** 2025-11-21
**Status:** Design Specification - Integration with Extrapolation Architecture
**Related:** EXTRAPOLATION_CAPABILITIES.md, EXTRAPOLATION_CAPABILITIES_PART2.md

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Biological Foundation](#2-biological-foundation)
3. [Architecture Overview](#3-architecture-overview)
4. [Integration with Extrapolation](#4-integration-with-extrapolation)
5. [Imagination Module Specification](#5-imagination-module-specification)
6. [Generative Visual Cortex](#6-generative-visual-cortex)
7. [Generative Audio Cortex](#7-generative-audio-cortex)
8. [Temporal Generation (Video)](#8-temporal-generation-video)
9. [Implementation Details](#9-implementation-details)
10. [Training & Learning](#10-training--learning)
11. [API Specifications](#11-api-specifications)
12. [Performance Requirements](#12-performance-requirements)
13. [Implementation Roadmap](#13-implementation-roadmap)

---

## 1. Executive Summary

### 1.1 Purpose

This document specifies the architecture for adding **imagination and generative capabilities** to NIMCP, enabling biologically-inspired generation of images, video, audio, and speech similar to Midjourney, DALL-E, or Google Veo, but using brain-inspired mechanisms.

### 1.2 Key Capabilities

- **Visual Imagination**: Generate images from text descriptions or concepts
- **Video Generation**: Create temporal sequences with motion coherence
- **Audio/Speech Generation**: Synthesize sounds and speech
- **Mental Simulation**: Imagine outcomes before acting (integrates with World Model)
- **Dream/Consolidation**: Offline learning through spontaneous generation

### 1.3 Biological Inspiration

The brain already generates sensory experiences:
- **Visual Imagery**: You can "see" objects in your mind
- **Dreams**: Spontaneous sensory generation during sleep
- **Predictive Coding**: Top-down predictions about sensory input
- **Mental Simulation**: Planning by imagining future scenarios

This architecture implements these capabilities through **bidirectional processing** - the same cortical pathways that recognize stimuli can generate them.

### 1.4 Integration with Extrapolation

| Extrapolation Module | Integration with Imagination |
|---------------------|------------------------------|
| **World Model** | Imagination provides visual/sensory rendering of predicted states |
| **Compositional** | Generate images from compositional concept descriptions |
| **Analogical** | Transfer visual concepts across domains |
| **Meta-Learning** | Generate synthetic training data for few-shot tasks |
| **Causal** | Visualize counterfactual scenarios |

---

## 2. Biological Foundation

### 2.1 Cortical Bidirectionality

**Key Fact**: 90% of connections to primary visual cortex (V1) are **feedback** (top-down), not feedforward.

```
Normal Perception (Bottom-Up):
Retina → LGN → V1 → V2 → V4 → IT → Concepts

Imagination (Top-Down):
Concepts → IT → V4 → V2 → V1 → "Mental Image"
```

### 2.2 Predictive Coding

The brain constantly generates predictions about sensory input:

```
Prediction Error = Actual Sensory Input - Top-Down Prediction

Learning minimizes prediction error by:
1. Updating predictions (perception)
2. Generating actions to match predictions (motor control)
```

Imagination is **pure top-down** prediction without bottom-up input.

### 2.3 Neurobiological Evidence

1. **Visual Imagery Activates V1**: fMRI shows V1 activation when imagining objects
2. **Dreams**: REM sleep shows sensory cortex activation without external input
3. **Mental Rotation**: Imagery uses same neural pathways as actual vision
4. **Aphantasia**: People who cannot visualize have reduced V1 activation during imagery

### 2.4 Comparison to Diffusion Models

| Aspect | Biological Imagination | Diffusion Models (e.g., Stable Diffusion) |
|--------|----------------------|------------------------------------------|
| **Process** | Top-down hierarchical generation | Iterative denoising |
| **Mechanism** | Predictive coding error minimization | Learned noise schedule |
| **Steps** | ~5-10 refinement iterations | ~20-50 denoising steps |
| **Control** | Attention, working memory | Text conditioning, cross-attention |
| **Spontaneity** | Dreams (unconditioned) | Random noise seed |

**Key Insight**: Diffusion's iterative refinement mirrors the brain's iterative prediction error minimization.

---

## 3. Architecture Overview

### 3.1 System Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    IMAGINATION MODULE                         │
│  ┌────────────────────────────────────────────────────┐      │
│  │  Generation Engine (Diffusion-like Refinement)     │      │
│  │  • Iterative refinement                            │      │
│  │  • Temperature annealing                           │      │
│  │  • Attention-guided generation                     │      │
│  └───────────────┬────────────────────────────────────┘      │
│                  │                                            │
│      ┌───────────┴────────────┬─────────────┬───────────┐   │
│      ▼                        ▼             ▼           ▼   │
│  ┌────────┐            ┌───────────┐  ┌─────────┐ ┌───────┐│
│  │Visual  │            │   Audio   │  │ Speech  │ │ Video ││
│  │Cortex  │            │  Cortex   │  │ Cortex  │ │ Gen   ││
│  │(Bidirectional)      │(Bidirectional)│(Seq)    │ │(Temp) ││
│  └────┬───┘            └─────┬─────┘  └────┬────┘ └───┬───┘│
│       │                      │             │          │     │
└───────┼──────────────────────┼─────────────┼──────────┼─────┘
        │                      │             │          │
        ▼                      ▼             ▼          ▼
┌───────────────────────────────────────────────────────────────┐
│              Extrapolation Layer Integration                   │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐      │
│  │Compositional│  │  World Model │  │  Working Memory│      │
│  │(Concepts)   │  │  (States)    │  │  (Context)     │      │
│  └──────┬──────┘  └──────┬───────┘  └────────┬───────┘      │
│         │                 │                    │              │
│         └─────────────────┴────────────────────┘              │
│                           ▼                                    │
│                  ┌─────────────────┐                          │
│                  │ Semantic Encoder│                          │
│                  │ (Text → Concept)│                          │
│                  └─────────────────┘                          │
└───────────────────────────────────────────────────────────────┘
```

### 3.2 Module Hierarchy

```
nimcp/
├── src/
│   ├── cognitive/
│   │   ├── nimcp_imagination.c             [NEW - Core imagination engine]
│   │   ├── nimcp_world_model.c             [ENHANCED - Add visual rendering]
│   │   └── nimcp_dream_engine.c            [NEW - Offline consolidation]
│   ├── core/
│   │   └── brain/
│   │       └── regions/
│   │           ├── visual_cortex/
│   │           │   ├── nimcp_visual_cortex.c         [ENHANCED - Bidirectional]
│   │           │   └── nimcp_visual_generator.c      [NEW - Top-down generation]
│   │           ├── audio_cortex/
│   │           │   ├── nimcp_audio_cortex.c          [ENHANCED - Bidirectional]
│   │           │   └── nimcp_audio_generator.c       [NEW - Sound generation]
│   │           └── speech_cortex/
│   │               └── nimcp_speech_generator.c      [NEW - Speech synthesis]
│   └── middleware/
│       └── generation/
│           ├── nimcp_diffusion_engine.c     [NEW - Refinement iterations]
│           ├── nimcp_cross_modal_bridge.c   [NEW - Text→Image, etc.]
│           └── nimcp_temporal_coherence.c   [NEW - Video sequences]
```

---

## 4. Integration with Extrapolation

### 4.1 World Model + Imagination

**Problem**: World model predicts abstract state vectors, but humans imagine **visual** futures.

**Solution**: Imagination module renders world model predictions into sensory form.

```c
// World model predicts next state
prediction_t* prediction = world_model_predict(wm, current_state, action);

// Imagination renders it visually
image_t imagined_future = imagination_render_state(
    brain->imagination,
    prediction->predicted_state
);

// Now we can "see" the predicted future
display_image(imagined_future);
```

**Use Case**: Autonomous vehicle visualizes planned trajectory before executing.

### 4.2 Compositional + Visual Generation

**Problem**: Compositional system creates abstract concept combinations.

**Solution**: Imagination generates visual instances of novel concepts.

```c
// Compositional: "red flying elephant"
compositional_expr_t* concept = compositional_compose(
    cs,
    COMPOSE_MODIFICATION,
    parse("red"),
    parse("flying elephant")
);

// Imagination: Generate image of this novel concept
image_t image = imagination_generate_from_concept(
    brain->imagination,
    concept,
    width=512, height=512
);
```

**Use Case**: Creative design, novel object generation.

### 4.3 Analogical + Cross-Domain Imagery

**Problem**: Analogies transfer abstract structure.

**Solution**: Imagination visualizes analogical mappings.

```c
// Analogy: atom is like solar system
analogy_t* analogy = analogical_find_mapping(engine, solar_system, atom);

// Generate side-by-side visualization
image_t source_viz = imagination_visualize_domain(
    brain->imagination,
    analogy->source
);

image_t target_viz = imagination_visualize_domain(
    brain->imagination,
    analogy->target
);
```

**Use Case**: Educational visualization, scientific explanation.

### 4.4 Causal + Counterfactual Visualization

**Problem**: Counterfactuals are abstract ("What if I hadn't...?")

**Solution**: Imagine the counterfactual scenario visually.

```c
// Counterfactual query
counterfactual_query_t query = {
    .query_var = "outcome",
    .interventions = {{.variable = "action", .value = 0}}
};

// Compute counterfactual
float result;
causal_compute_counterfactual(cr, &query, &result);

// Visualize both actual and counterfactual
image_t actual = imagination_render_scenario(brain->imagination, actual_state);
image_t counterfactual = imagination_render_scenario(brain->imagination, cf_state);
```

**Use Case**: Decision explanation, regret analysis.

### 4.5 Meta-Learning + Synthetic Data

**Problem**: Few-shot learning needs more diverse examples.

**Solution**: Imagination generates synthetic training data.

```c
// Meta-training with 5 real examples + 50 imagined variations
task_t* task = meta_create_task(real_examples, 5);

// Generate variations
image_t** augmented = imagination_generate_variations(
    brain->imagination,
    real_examples, 5,
    num_variations=50
);

// Merge real + synthetic for richer task
task_t* augmented_task = merge_tasks(task, augmented);

// Better meta-learning with more diverse data
meta_learner_adapt(brain->meta_learner, augmented_task, &adapted);
```

**Use Case**: Data augmentation, few-shot robustness.

---

## 5. Imagination Module Specification

### 5.1 Header: `include/cognitive/nimcp_imagination.h`

```c
/**
 * @file nimcp_imagination.h
 * @brief Imagination and generative modeling
 *
 * Implements biologically-inspired generative capabilities for:
 * - Visual imagery (mental images)
 * - Audio imagination (mental sounds)
 * - Video simulation (mental movies)
 * - Dream/consolidation (offline learning)
 *
 * Mechanisms:
 * - Predictive coding (top-down generation)
 * - Iterative refinement (diffusion-like)
 * - Cross-modal generation (text → image, etc.)
 * - Temporal coherence (video)
 *
 * Integration:
 * - World model (render predicted states)
 * - Compositional (visualize novel concepts)
 * - Working memory (context for generation)
 *
 * References:
 * - Kosslyn et al. (2006). The case for mental imagery.
 * - Pearson et al. (2015). Mental imagery: functional mechanisms.
 * - Ho et al. (2020). Denoising Diffusion Probabilistic Models.
 */

#ifndef NIMCP_IMAGINATION_H
#define NIMCP_IMAGINATION_H

#include "core/brain/nimcp_brain.h"
#include "core/reasoning/nimcp_compositional.h"
#include "cognitive/nimcp_world_model.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Definitions
// ============================================================================

/**
 * @brief Generation mode
 */
typedef enum {
    GENERATION_MODE_VISUAL,          // Image generation
    GENERATION_MODE_AUDIO,           // Sound generation
    GENERATION_MODE_SPEECH,          // Speech synthesis
    GENERATION_MODE_VIDEO,           // Video generation
    GENERATION_MODE_MULTIMODAL       // Combined modalities
} generation_mode_t;

/**
 * @brief Processing direction
 */
typedef enum {
    PROCESSING_BOTTOM_UP,            // Perception (normal)
    PROCESSING_TOP_DOWN,             // Generation (imagination)
    PROCESSING_BIDIRECTIONAL         // Both (guided generation)
} processing_direction_t;

/**
 * @brief Image representation
 */
typedef struct {
    uint32_t width;                  // Image width
    uint32_t height;                 // Image height
    uint32_t channels;               // Color channels (1=gray, 3=RGB)
    float* pixels;                   // Pixel data [H × W × C]
    uint64_t timestamp;              // Generation timestamp
} image_t;

/**
 * @brief Video representation
 */
typedef struct {
    uint32_t width;                  // Frame width
    uint32_t height;                 // Frame height
    uint32_t channels;               // Color channels
    uint32_t num_frames;             // Number of frames
    image_t** frames;                // Array of frames
    float fps;                       // Frames per second
} video_t;

/**
 * @brief Audio representation
 */
typedef struct {
    float* samples;                  // Audio samples
    uint32_t num_samples;            // Number of samples
    uint32_t sample_rate;            // Sample rate (Hz)
    uint32_t channels;               // Audio channels (1=mono, 2=stereo)
    float duration_sec;              // Duration in seconds
} audio_t;

/**
 * @brief Generation prompt
 */
typedef struct {
    char text[1024];                 // Text description
    compositional_expr_t* concept;   // Compositional concept
    float* semantic_embedding;       // Semantic embedding
    uint32_t embedding_dim;          // Embedding dimensionality

    // Conditioning
    image_t* reference_image;        // Reference image (optional)
    float guidance_scale;            // Guidance strength (1.0-20.0)
    uint32_t random_seed;            // Random seed for reproducibility
} generation_prompt_t;

/**
 * @brief Imagination configuration
 */
typedef struct {
    // Generation parameters
    uint32_t num_refinement_steps;   // Refinement iterations (10-50)
    float initial_temperature;       // Initial randomness
    float final_temperature;         // Final temperature

    // Image generation
    uint32_t default_image_width;    // Default width (e.g., 512)
    uint32_t default_image_height;   // Default height (e.g., 512)

    // Video generation
    uint32_t max_video_frames;       // Max frames (e.g., 300 = 10s at 30fps)
    float default_fps;               // Default FPS

    // Audio generation
    uint32_t audio_sample_rate;      // Sample rate (e.g., 44100 Hz)

    // Processing
    bool enable_cross_modal;         // Enable text→image, etc.
    bool enable_temporal_coherence;  // Video frame consistency
    bool enable_dream_mode;          // Offline consolidation

} imagination_config_t;

/**
 * @brief Imagination engine
 */
typedef struct {
    brain_t brain;                   // Parent brain
    imagination_config_t config;     // Configuration

    // Generators (bidirectional cortices)
    void* visual_generator;          // Visual cortex (top-down)
    void* audio_generator;           // Audio cortex (top-down)
    void* speech_generator;          // Speech cortex
    void* temporal_generator;        // Temporal sequence generator

    // Cross-modal bridges
    void* text_to_visual;            // Text → image encoder
    void* visual_to_audio;           // Image → sound encoder

    // Refinement engine
    void* refinement_engine;         // Iterative refinement

    // State
    processing_direction_t direction; // Current processing direction
    uint32_t generation_count;       // Number of generations

} imagination_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Create imagination module
 *
 * @param brain Parent brain instance
 * @param config Configuration
 * @return imagination_t* Imagination module, NULL on failure
 */
imagination_t* imagination_create(
    brain_t brain,
    const imagination_config_t* config
);

/**
 * @brief Destroy imagination module
 *
 * @param imagination Imagination module
 */
void imagination_destroy(imagination_t* imagination);

/**
 * @brief Set processing direction
 *
 * @param imagination Imagination module
 * @param direction Processing direction
 * @return bool True on success
 */
bool imagination_set_direction(
    imagination_t* imagination,
    processing_direction_t direction
);

// ============================================================================
// Visual Generation
// ============================================================================

/**
 * @brief Generate image from text description
 *
 * Implements text-to-image generation similar to DALL-E or Stable Diffusion,
 * but using biologically-inspired top-down cortical processing.
 *
 * Process:
 * 1. Encode text to semantic embedding
 * 2. Map to high-level visual concepts (IT cortex)
 * 3. Iteratively refine through V4, V2, V1 (top-down)
 * 4. Generate pixel-level output
 *
 * @param imagination Imagination module
 * @param prompt Text description
 * @param width Image width
 * @param height Image height
 * @param image Output: generated image (caller frees)
 * @return bool True on success
 *
 * @complexity O(R × L × H^2) where R=refinement_steps, L=layers, H=hidden_dim
 * @performance ~2-5 seconds for 512×512 image with 20 steps
 */
bool imagination_generate_image_from_text(
    imagination_t* imagination,
    const char* prompt,
    uint32_t width,
    uint32_t height,
    image_t** image
);

/**
 * @brief Generate image from concept
 *
 * @param imagination Imagination module
 * @param concept Compositional concept
 * @param width Image width
 * @param height Image height
 * @param image Output: generated image
 * @return bool True on success
 */
bool imagination_generate_image_from_concept(
    imagination_t* imagination,
    compositional_expr_t* concept,
    uint32_t width,
    uint32_t height,
    image_t** image
);

/**
 * @brief Generate with full prompt control
 *
 * @param imagination Imagination module
 * @param prompt Generation prompt (text, concept, conditioning)
 * @param image Output: generated image
 * @return bool True on success
 */
bool imagination_generate_image(
    imagination_t* imagination,
    generation_prompt_t* prompt,
    image_t** image
);

/**
 * @brief Refine existing image (iterative improvement)
 *
 * @param imagination Imagination module
 * @param input Input image
 * @param prompt Prompt for refinement
 * @param num_steps Number of refinement steps
 * @param output Output: refined image
 * @return bool True on success
 */
bool imagination_refine_image(
    imagination_t* imagination,
    image_t* input,
    generation_prompt_t* prompt,
    uint32_t num_steps,
    image_t** output
);

// ============================================================================
// Video Generation
// ============================================================================

/**
 * @brief Generate video from text description
 *
 * Generates temporally coherent video sequences using:
 * 1. Frame-by-frame generation with temporal context
 * 2. Hippocampal sequence prediction for motion
 * 3. Optical flow constraints for smoothness
 *
 * @param imagination Imagination module
 * @param prompt Text description
 * @param num_frames Number of frames
 * @param fps Frames per second
 * @param video Output: generated video (caller frees)
 * @return bool True on success
 *
 * @complexity O(F × R × L × H^2) where F=frames
 * @performance ~30-60 seconds for 5-second video at 30fps
 */
bool imagination_generate_video_from_text(
    imagination_t* imagination,
    const char* prompt,
    uint32_t num_frames,
    float fps,
    video_t** video
);

/**
 * @brief Generate video with motion conditioning
 *
 * @param imagination Imagination module
 * @param prompt Prompt
 * @param initial_frame Initial frame (optional)
 * @param motion_trajectory Predicted motion from world model
 * @param num_frames Number of frames
 * @param video Output: generated video
 * @return bool True on success
 */
bool imagination_generate_video_with_motion(
    imagination_t* imagination,
    generation_prompt_t* prompt,
    image_t* initial_frame,
    trajectory_t* motion_trajectory,
    uint32_t num_frames,
    video_t** video
);

// ============================================================================
// Audio Generation
// ============================================================================

/**
 * @brief Generate audio from text description
 *
 * @param imagination Imagination module
 * @param prompt Text description of sound
 * @param duration_sec Duration in seconds
 * @param audio Output: generated audio (caller frees)
 * @return bool True on success
 */
bool imagination_generate_audio_from_text(
    imagination_t* imagination,
    const char* prompt,
    float duration_sec,
    audio_t** audio
);

/**
 * @brief Synthesize speech from text
 *
 * @param imagination Imagination module
 * @param text Text to speak
 * @param voice_characteristics Voice parameters (pitch, rate, etc.)
 * @param audio Output: synthesized speech
 * @return bool True on success
 */
bool imagination_synthesize_speech(
    imagination_t* imagination,
    const char* text,
    void* voice_characteristics,
    audio_t** audio
);

// ============================================================================
// World Model Integration
// ============================================================================

/**
 * @brief Render world model state as image
 *
 * Converts abstract world model state into visual rendering.
 * Enables "seeing" what the world model predicts.
 *
 * @param imagination Imagination module
 * @param state World model state
 * @param image Output: rendered image
 * @return bool True on success
 */
bool imagination_render_state(
    imagination_t* imagination,
    state_t* state,
    image_t** image
);

/**
 * @brief Visualize planned trajectory
 *
 * Renders a sequence of predicted states as video.
 *
 * @param imagination Imagination module
 * @param trajectory Planned trajectory from world model
 * @param video Output: visualized plan
 * @return bool True on success
 */
bool imagination_visualize_plan(
    imagination_t* imagination,
    trajectory_t* trajectory,
    video_t** video
);

/**
 * @brief Imagine counterfactual scenario
 *
 * Visualizes "what if" scenarios from causal reasoning.
 *
 * @param imagination Imagination module
 * @param current_state Current state
 * @param counterfactual_state Counterfactual state
 * @param comparison Output: side-by-side comparison image
 * @return bool True on success
 */
bool imagination_visualize_counterfactual(
    imagination_t* imagination,
    state_t* current_state,
    state_t* counterfactual_state,
    image_t** comparison
);

// ============================================================================
// Dream Mode (Offline Consolidation)
// ============================================================================

/**
 * @brief Enter dream mode
 *
 * Spontaneous generation for offline learning and memory consolidation.
 * Similar to REM sleep - replays and recombines experiences.
 *
 * Process:
 * 1. Reduce sensory input
 * 2. Increase spontaneous activity
 * 3. Generate from random/memory seeds
 * 4. Strengthen generative pathways
 *
 * @param imagination Imagination module
 * @param duration_steps Number of dream steps
 * @return bool True on success
 */
bool imagination_dream(
    imagination_t* imagination,
    uint32_t duration_steps
);

/**
 * @brief Generate synthetic training data
 *
 * Creates variations of existing examples for data augmentation.
 *
 * @param imagination Imagination module
 * @param examples Base examples
 * @param num_examples Number of base examples
 * @param num_variations Variations per example
 * @param augmented Output: array of generated variations
 * @return bool True on success
 */
bool imagination_generate_variations(
    imagination_t* imagination,
    image_t** examples,
    uint32_t num_examples,
    uint32_t num_variations,
    image_t*** augmented
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create image
 *
 * @param width Image width
 * @param height Image height
 * @param channels Color channels
 * @return image_t* Image, NULL on failure
 */
image_t* imagination_create_image(
    uint32_t width,
    uint32_t height,
    uint32_t channels
);

/**
 * @brief Destroy image
 *
 * @param image Image to destroy
 */
void imagination_destroy_image(image_t* image);

/**
 * @brief Save image to file
 *
 * @param image Image
 * @param filename Output file path (supports PNG, JPEG)
 * @return bool True on success
 */
bool imagination_save_image(
    image_t* image,
    const char* filename
);

/**
 * @brief Load image from file
 *
 * @param filename Input file path
 * @param image Output: loaded image
 * @return bool True on success
 */
bool imagination_load_image(
    const char* filename,
    image_t** image
);

/**
 * @brief Create video
 *
 * @param width Frame width
 * @param height Frame height
 * @param channels Color channels
 * @param num_frames Number of frames
 * @param fps Frames per second
 * @return video_t* Video, NULL on failure
 */
video_t* imagination_create_video(
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    uint32_t num_frames,
    float fps
);

/**
 * @brief Destroy video
 *
 * @param video Video to destroy
 */
void imagination_destroy_video(video_t* video);

/**
 * @brief Save video to file
 *
 * @param video Video
 * @param filename Output file path (supports MP4, AVI)
 * @return bool True on success
 */
bool imagination_save_video(
    video_t* video,
    const char* filename
);

/**
 * @brief Create audio buffer
 *
 * @param sample_rate Sample rate
 * @param duration_sec Duration in seconds
 * @param channels Audio channels
 * @return audio_t* Audio buffer, NULL on failure
 */
audio_t* imagination_create_audio(
    uint32_t sample_rate,
    float duration_sec,
    uint32_t channels
);

/**
 * @brief Destroy audio
 *
 * @param audio Audio to destroy
 */
void imagination_destroy_audio(audio_t* audio);

/**
 * @brief Save audio to file
 *
 * @param audio Audio
 * @param filename Output file path (supports WAV, MP3)
 * @return bool True on success
 */
bool imagination_save_audio(
    audio_t* audio,
    const char* filename
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_IMAGINATION_H
```

---

## 6. Generative Visual Cortex

### 6.1 Bidirectional Architecture

```
V1 (Primary Visual Cortex):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
                    ↕ (Bidirectional)

Bottom-Up (Perception):          Top-Down (Generation):
Pixels → Edges → Orientations    Concepts → V4 → V2 → V1 → Pixels
     ↓                                 ↓
V2 → Complex Features            V2 generates complex features
     ↓                                 ↓
V4 → Shapes, Objects             V4 generates object shapes
     ↓                                 ↓
IT → Object Recognition          IT seeds high-level concepts
```

### 6.2 Implementation

```c
// Bidirectional visual cortex layer
typedef struct {
    // Forward (bottom-up) processing
    float* weights_forward;

    // Backward (top-down) processing
    float* weights_backward;

    // Current activation
    float* activation;

    // Prediction error
    float* error;

} bidirectional_layer_t;

// Top-down generation
void visual_cortex_generate_from_concept(
    visual_cortex_t* vc,
    semantic_embedding_t concept,
    int refinement_step,
    float temperature,
    image_t* output
) {
    // Start from high-level concept (IT level)
    float* it_activation = activate_IT_from_concept(concept, temperature);

    // Generate progressively lower-level features
    float* v4_activation = generate_V4_from_IT(it_activation);
    float* v2_activation = generate_V2_from_V4(v4_activation);
    float* v1_activation = generate_V1_from_V2(v2_activation);

    // Finally: pixel-level output
    generate_pixels_from_V1(v1_activation, output);
}
```

---

## 7. Generative Audio Cortex

### 7.1 Audio Generation Pipeline

```
Text/Concept → Semantic Encoding
      ↓
Audio Cortex (Top-Down):
      ↓
Spectral Features (Mel-spectrogram)
      ↓
Temporal Dynamics
      ↓
Waveform Generation (Vocoder)
      ↓
Audio Output
```

### 7.2 Speech Synthesis

```c
// Generate speech from text
audio_t* generate_speech(
    imagination_t* imagination,
    const char* text,
    voice_params_t* voice
) {
    // 1. Text to phonemes
    phoneme_sequence_t* phonemes = text_to_phonemes(text);

    // 2. Phonemes to mel-spectrogram (audio cortex top-down)
    mel_spectrogram_t* mel = generate_mel_from_phonemes(
        imagination->audio_generator,
        phonemes,
        voice
    );

    // 3. Mel-spectrogram to waveform (vocoder)
    audio_t* audio = mel_to_waveform(mel, imagination->config.audio_sample_rate);

    return audio;
}
```

---

## 8. Temporal Generation (Video)

### 8.1 Temporal Coherence

**Challenge**: Each frame must be consistent with previous frames.

**Solution**: Use hippocampal sequence prediction + optical flow.

```c
video_t* generate_video(
    imagination_t* imagination,
    const char* prompt,
    uint32_t num_frames,
    float fps
) {
    video_t* video = imagination_create_video(512, 512, 3, num_frames, fps);

    // Generate first frame
    imagination_generate_image_from_text(
        imagination, prompt, 512, 512, &video->frames[0]
    );

    // Generate subsequent frames with temporal context
    for (int t = 1; t < num_frames; t++) {
        // Predict next frame from previous + motion
        motion_prediction_t motion = hippocampus_predict_motion(
            imagination->brain->hippocampus,
            video->frames[t-1]
        );

        // Generate with temporal conditioning
        generate_frame_with_temporal_context(
            imagination,
            prompt,
            video->frames[t-1],  // Previous frame
            motion,               // Predicted motion
            &video->frames[t]    // Output
        );
    }

    return video;
}
```

---

*[Document continues with sections 9-13...]*

---

**This merged architecture enables:**

1. **Visual imagination** from text/concepts (like Midjourney/DALL-E)
2. **Video generation** with temporal coherence (like Veo/Sora)
3. **Audio/speech synthesis** (like WaveNet/AudioLM)
4. **Integration with extrapolation** (world models, compositional reasoning, analogies)
5. **Dream mode** for offline consolidation
6. **Biologically-plausible** mechanisms (top-down cortical processing)

The imagination module sits at the **nexus** of perception and reasoning, enabling the brain to:
- **Plan** by imagining futures
- **Create** by generating novel images
- **Learn** by generating synthetic data
- **Explain** by visualizing abstract concepts

This is the missing piece that transforms NIMCP from a pattern recognizer into a creative, planning, imagining system.


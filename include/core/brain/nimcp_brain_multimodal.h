//=============================================================================
// nimcp_brain_multimodal.h - Multimodal Processing and Integration
//=============================================================================
/**
 * @file nimcp_brain_multimodal.h
 * @brief Multimodal sensory processing and cognitive integration API
 *
 * This module provides the multimodal processing pipeline for brain inputs:
 * - Visual, audio, speech feature extraction
 * - Cross-modal integration with attention
 * - Hierarchical brain regions processing
 * - Cognitive assessments (confidence, ethics, salience)
 * - Memory consolidation and decision formatting
 *
 * EXTRACTED FROM: nimcp_brain.c (SRP refactoring)
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_MULTIMODAL_H
#define NIMCP_BRAIN_MULTIMODAL_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Multimodal Processing API
//=============================================================================

/**
 * @brief Apply multihead attention to integrated features
 *
 * WHAT: Selective feature processing using cortical column-inspired attention
 * WHY:  Focus on salient features, reduce computation, improve accuracy
 * HOW:  Apply multihead attention if enabled, use guard clauses for clean code
 *
 * @param brain Brain handle with attention system
 * @return true on success (or if attention skipped), false only on fatal error
 */
bool apply_attention_to_features(brain_t brain);

/**
 * @brief Process features through hierarchical brain regions
 *
 * WHAT: Process features through hierarchical brain regions
 * WHY:  Enable specialized cortical processing with realistic layer dynamics
 * HOW:  Feed features to brain regions, collect output from multiple regions
 *
 * @param brain Brain instance with brain_regions module
 * @return true on success or if brain_regions disabled, false on fatal error
 */
bool process_brain_regions(brain_t brain);

/**
 * @brief Process multi-modal input through unified cognitive architecture
 *
 * WHAT: Process multi-modal input through unified cognitive architecture
 * WHY:  Enable coordinated multi-modal perception and cognition
 * HOW:  Sensory -> Integration -> Neural -> Cognitive -> Output
 *
 * ARCHITECTURE:
 * 1. Sensory stage: Extract features from visual/audio/direct
 * 2. Integration: Fuse multi-modal features
 * 3. Neural processing: Feed to network with STDP/glial/oscillations
 * 4. Cognitive processing: Introspection/ethics/salience/curiosity
 * 5. Output integration: Consolidate and extract final decision
 *
 * COMPLEXITY: O(sensory + neural + cognitive)
 * PERFORMANCE: ~10-50ms typical (medium brain + camera + audio)
 * THREAD SAFETY: Not thread-safe (modifies brain state)
 *
 * @param brain Brain handle (required, must support multi-modal)
 * @param input Multi-modal input bundle (required, at least one modality)
 * @param output Pre-allocated output structure (required)
 * @return true on success, false on failure
 */
bool brain_process_multimodal(brain_t brain,
                              const brain_multimodal_input_t* input,
                              brain_multimodal_output_t* output);

/**
 * @brief Convenience wrapper: process features + optional text through the
 *        multimodal pipeline.
 *
 * WHAT: Constructs a minimal brain_multimodal_input_t from a direct feature
 *       vector + optional UTF-8 text and dispatches to brain_process_multimodal.
 * WHY:  brain_process_multimodal expects a fully-populated multimodal_input_t
 *       with visual/audio/language/direct slots. Most callers only have a
 *       feature vector (and sometimes text) — this wrapper builds the input
 *       struct for them so the multimodal pipeline has a convenient entry
 *       point without hand-constructing the full bundle.
 * HOW:  Populates direct_data + (optional) language_text + timestamp on the
 *       stack-allocated input, zeroes the output, then calls
 *       brain_process_multimodal. Output owned by caller per the existing
 *       multimodal API contract.
 *
 * @param brain        Brain handle
 * @param features     Feature vector (required)
 * @param num_features Feature count (required, > 0)
 * @param text         Optional UTF-8 text (may be NULL for feature-only calls)
 * @param output       Pre-allocated output struct (required)
 * @return true on success, false on error
 */
bool brain_process_features_with_text(brain_t brain,
                                      const float* features,
                                      uint32_t num_features,
                                      const char* text,
                                      brain_multimodal_output_t* output);

/**
 * @brief Feed raw audio into the brain's cochlea — populates
 *        `brain->cochlea_output_buffer` so the 11 cochlea consumer
 *        bridges see live data instead of the zero-init placeholder.
 *
 * WHY:  The brain owns a single shared cochlea_output_t buffer that all
 *       downstream bridges read from. Without a caller pushing audio in,
 *       the buffer stays zeroed and bridges process silence forever.
 *       This is the designed entry point — the Python binding or
 *       training script calls it whenever audio is available, and the
 *       next region_cycles tick pass propagates the output.
 *
 * HOW:  Invokes `cochlea_process(brain->cochlea, audio, num_samples,
 *       brain->cochlea_output_buffer)`. Null-safe on missing cochlea or
 *       buffer. Does NOT trigger bridge ticks — those fire on the
 *       coordinator's cadence.
 *
 * @param brain        Brain handle
 * @param audio        Mono float audio (required, length=num_samples)
 * @param num_samples  Sample count
 * @return true on success, false if audio/cochlea/buffer absent
 */
bool brain_feed_audio(brain_t brain,
                      const float* audio,
                      uint32_t num_samples);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_MULTIMODAL_H

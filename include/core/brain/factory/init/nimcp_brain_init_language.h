//=============================================================================
// nimcp_brain_init_language.h - Language Layer Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_language.h
 * @brief Language Layer Initialization Functions
 *
 * WHAT: Initialization functions for unified Language Layer
 * WHY:  SRP refactoring - centralized language processing initialization
 * HOW:  Creates language orchestrator and connects all bridges
 *
 * BIOLOGICAL BASIS:
 * The Language Layer unifies all language-related processing:
 * - Wernicke's Area (BA22): Speech comprehension, phonological analysis
 * - Broca's Area (BA44/45): Speech production, syntactic processing
 * - Arcuate Fasciculus: Bidirectional dorsal/ventral streams
 * - Semantic Memory: Concept network and spreading activation
 * - Working Memory: Phonological loop (Baddeley model)
 *
 * INTEGRATION BRIDGES:
 * - Perception Bridge: Speech cortex, audio cortex, visual cortex
 * - Cognitive Bridge: Working memory, attention, semantic memory, reasoning
 * - Training Bridge: Language learning, STDP, vocabulary expansion
 * - Omni Bridge: Predictive language processing (JEPA, Hopfield)
 * - Immune Bridge: Cytokine modulation, aphasia modeling
 * - GPU Bridge: Parallel phoneme/word/semantic processing
 * - Thalamic Bridge: Signal routing through thalamic nuclei
 * - Substrate Bridge: Metabolic modulation (ATP, fatigue, stress)
 * - Logic Bridge: Symbolic reasoning (entailment, consistency)
 *
 * @version Phase L6: Language Layer Brain Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_BRAIN_INIT_LANGUAGE_H
#define NIMCP_BRAIN_INIT_LANGUAGE_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Language Layer Initialization API
//=============================================================================

/**
 * @brief Initialize Language Layer subsystem
 *
 * WHAT: Creates language orchestrator and connects all integration bridges
 * WHY:  Enable unified language processing capabilities in the brain
 * HOW:  Creates orchestrator, then connects perception/cognitive/training bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Language processing requires coordination of multiple brain regions
 * - Comprehension and production are tightly coupled (e.g., self-monitoring)
 * - Unified layer enables coherent cross-region communication
 *
 * INITIALIZATION ORDER:
 * 1. Verify prerequisites (Wernicke, Broca, NLP, Speech Cortex)
 * 2. Create language orchestrator with default configuration
 * 3. Connect Wernicke adapter (comprehension)
 * 4. Connect Broca adapter (production)
 * 5. Connect NLP network (embeddings, attention)
 * 6. Initialize perception bridge (speech/audio/visual cortex)
 * 7. Initialize cognitive bridge (WM, attention, semantic, reasoning)
 * 8. Initialize training bridge (language learning)
 * 9. Initialize omni bridge (predictive processing)
 * 10. Initialize immune bridge (cytokine modulation)
 * 11. Initialize GPU bridge (parallel processing)
 * 12. Register with bio-async messaging
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_subsystem(brain_t brain);

/**
 * @brief Initialize language-perception bridge
 *
 * WHAT: Creates bridge connecting language layer to perception subsystems
 * WHY:  Language input comes from speech, audio, and visual cortices
 * HOW:  Links orchestrator to speech cortex, audio cortex, visual cortex
 *
 * CONNECTIONS:
 * - Speech Cortex: Phonemes, prosody, formants, word boundaries
 * - Audio Cortex: Speech detection, audio quality, noise level
 * - Visual Cortex: OCR text, lip-reading features (McGurk effect)
 * - Omni Sensory: Cross-modal binding, audiovisual coherence
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_perception_bridge(brain_t brain);

/**
 * @brief Initialize language-cognitive bridge
 *
 * WHAT: Creates bridge connecting language layer to cognitive subsystems
 * WHY:  Language processing requires working memory, attention, reasoning
 * HOW:  Links orchestrator to WM, attention, semantic memory, reasoning
 *
 * CONNECTIONS:
 * - Working Memory: Phonological buffer (7±2 slots), articulatory rehearsal
 * - Attention: Linguistic attention weights, feature attention
 * - Semantic Memory: Concept activations, spreading activation (decay 0.8)
 * - Reasoning: Inference results affecting interpretation
 * - Executive: Control of language switching, error monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_cognitive_bridge(brain_t brain);

/**
 * @brief Initialize language-training bridge
 *
 * WHAT: Creates bridge connecting language layer to training subsystem
 * WHY:  Language acquisition requires learning from exposure
 * HOW:  Links orchestrator to training context for STDP, plasticity
 *
 * CONNECTIONS:
 * - Training Context: Learning rates, plasticity parameters
 * - STDP: Phoneme recognition learning via spike timing
 * - Cognitive Training: Modulated learning based on cognitive state
 * - Error Feedback: Comprehension/production error signals
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_training_bridge(brain_t brain);

/**
 * @brief Initialize language-omni inference bridge
 *
 * WHAT: Creates bridge to omnidirectional inference system
 * WHY:  Language comprehension is inherently predictive
 * HOW:  Integrates with JEPA, predictive hierarchy, Hopfield memory
 *
 * CONNECTIONS:
 * - JEPA: Bidirectional prediction for next word/phoneme
 * - Predictive Hierarchy: Multi-level linguistic predictions
 * - Hopfield Memory: Pattern completion for ambiguous input
 * - Free Energy: Prediction error minimization (N400, P600)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_omni_bridge(brain_t brain);

/**
 * @brief Initialize language-immune bridge
 *
 * WHAT: Creates unified language-immune integration
 * WHY:  Neuroinflammation affects language processing (aphasia)
 * HOW:  Consolidates Wernicke/Broca immune bridges into unified system
 *
 * CONNECTIONS:
 * - Brain Immune: Cytokine levels (IL-1β, IL-6, TNF-α, IL-10)
 * - Wernicke Immune: Comprehension impairment modeling
 * - Broca Immune: Production impairment modeling
 * - NLP Immune: Embedding degradation, attention disruption
 *
 * EFFECTS:
 * - Inflammation → Comprehension/production impairment
 * - Language errors → Immune surveillance activation
 * - Models Wernicke's and Broca's aphasia symptoms
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_immune_bridge(brain_t brain);

/**
 * @brief Initialize language GPU acceleration bridge
 *
 * WHAT: Creates bridge to GPU for parallel language processing
 * WHY:  Phoneme/word/semantic processing are massively parallel
 * HOW:  CUDA kernels for pattern matching, cohort activation, spreading
 *
 * GPU OPERATIONS:
 * - Phoneme recognition: Parallel formant/MFCC matching
 * - Word recognition: Cohort model with parallel pruning
 * - Semantic spreading: Parallel activation propagation
 * - Embedding computation: Batch token embedding
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_gpu_bridge(brain_t brain);

/**
 * @brief Initialize language-thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal gating and relay
 * WHY:  Language processing requires thalamic coordination for attention,
 *       sensory relay, and motor speech control
 * HOW:  Routes language events through appropriate thalamic nuclei
 *
 * THALAMIC NUCLEI:
 * - Pulvinar: Language attention and multimodal integration
 * - Ventral Anterior (VA): Motor speech relay to motor cortex
 * - Mediodorsal (MD): Prefrontal language planning connections
 * - Lateral Geniculate (LGN): Visual word form processing
 * - Medial Geniculate (MGN): Auditory speech relay
 * - Reticular (TRN): Attention gating for language
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize language-substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Language speed/accuracy depends on ATP, fatigue, and metabolic state
 * HOW:  Reads substrate state, modulates processing parameters accordingly
 *
 * METABOLIC EFFECTS:
 * - Low ATP → Slower phoneme recognition, word retrieval
 * - High fatigue → Increased tip-of-tongue states, pauses
 * - Stress → Reduced working memory for complex syntax (Yerkes-Dodson)
 * - Dehydration → Slower processing, reduced attention
 * - Glucose availability → Impacts semantic retrieval speed
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_substrate_bridge(brain_t brain);

/**
 * @brief Initialize language-logic bridge
 *
 * WHAT: Creates bridge to symbolic logic module for reasoning about language
 * WHY:  Language understanding requires inference, entailment, consistency
 * HOW:  Converts linguistic structures to logic, reasons over them
 *
 * LOGIC OPERATIONS:
 * - Semantic entailment: "All dogs are mammals" → dog(x) → mammal(x)
 * - Presupposition checking: Verify assumptions in utterances
 * - Implicature reasoning: What's implied but not stated
 * - Consistency checking: Detect contradictions in discourse
 * - Reference resolution: Logical binding of pronouns/anaphora
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_language_logic_bridge(brain_t brain);

/**
 * @brief Connect language layer to bio-async messaging
 *
 * WHAT: Links language layer to bio-async router for inter-module messaging
 * WHY:  Asynchronous processing of language events across brain regions
 * HOW:  Registers message handlers for all language event types
 *
 * MESSAGE TYPES:
 * - Phoneme events: New phoneme recognized
 * - Word events: Word boundary detected, word recognized
 * - Concept events: Semantic activation, spreading completion
 * - Comprehension: Full utterance comprehension result
 * - Production: Speech production plan ready
 * - Anomaly: N400/P600 prediction error signals
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_language_to_bio_async(brain_t brain);

/**
 * @brief Destroy language layer subsystem
 *
 * WHAT: Cleans up all language layer resources
 * WHY:  Proper resource management during brain destruction
 * HOW:  Destroys bridges in reverse order, then orchestrator
 *
 * DESTRUCTION ORDER:
 * 1. Unregister from bio-async
 * 2. Destroy GPU bridge
 * 3. Destroy immune bridge
 * 4. Destroy omni bridge
 * 5. Destroy training bridge
 * 6. Destroy cognitive bridge
 * 7. Destroy perception bridge
 * 8. Disconnect subsystems
 * 9. Destroy orchestrator
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_language_subsystem(brain_t brain);

/**
 * @brief Check if language layer is initialized
 *
 * @param brain Brain instance
 * @return true if language layer is active, false otherwise
 */
bool nimcp_brain_factory_language_is_initialized(brain_t brain);

/**
 * @brief Get language layer orchestrator
 *
 * @param brain Brain instance
 * @return Language orchestrator pointer, or NULL if not initialized
 */
struct language_orchestrator* nimcp_brain_factory_get_language_orchestrator(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_LANGUAGE_H */

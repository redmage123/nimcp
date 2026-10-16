//=============================================================================
// nimcp_brain_init_wernicke.h - Wernicke's Region Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_wernicke.h
 * @brief Wernicke's Region Initialization Functions
 *
 * WHAT: Initialization functions for Wernicke's region (language comprehension)
 * WHY:  SRP refactoring - separate Wernicke initialization logic
 * HOW:  Creates Wernicke adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Posterior Superior Temporal Gyrus (STG), Brodmann Area 22
 * - Phonological analysis, lexical access, semantic integration
 * - Connections to Broca's area (arcuate fasciculus), auditory cortex, semantic memory
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of comprehension speed
 * - Quantum Bridge: Grover-accelerated lexical and semantic search
 * - Broca Bridge: Arcuate fasciculus for speech production feedback
 * - Omni Bridge: Omnidirectional predictive processing
 * - GPU Bridge: Parallel phoneme/word recognition
 *
 * @version Phase W6: Wernicke Full Brain Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_BRAIN_INIT_WERNICKE_H
#define NIMCP_BRAIN_INIT_WERNICKE_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Wernicke's Region Initialization API
//=============================================================================

/**
 * @brief Initialize Wernicke's region subsystem
 *
 * WHAT: Creates Wernicke adapter and connects all integration bridges
 * WHY:  Enable language comprehension capabilities in the brain
 * HOW:  Creates adapter, then substrate/quantum/broca/omni/gpu bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Wernicke's area is essential for speech comprehension
 * - Damage causes Wernicke's aphasia (fluent but meaningless speech)
 * - Integrates phonology, lexicon, semantics, and syntax
 *
 * INITIALIZATION ORDER:
 * 1. Create Wernicke adapter (core phonological/lexical/semantic)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create quantum bridge (accelerated search)
 * 4. Create Broca bridge (arcuate fasciculus)
 * 5. Create omni bridge (predictive processing)
 * 6. Create GPU bridge (parallel processing)
 * 7. Connect to semantic memory
 * 8. Connect to training system
 * 9. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_subsystem(brain_t brain);

/**
 * @brief Initialize Wernicke's substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Comprehension speed depends on metabolic state (ATP, fatigue)
 * HOW:  Links Wernicke to substrate for biologically-realistic processing
 *
 * EFFECTS:
 * - Low ATP -> Reduced word recognition speed
 * - High fatigue -> Slower semantic integration
 * - Stress -> Phonological processing disruption
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_substrate_bridge(brain_t brain);

/**
 * @brief Initialize Wernicke's quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Lexical search in large vocabularies benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) lexical and semantic access
 *
 * APPLICATIONS:
 * - Lexical access: Find word candidates from phoneme patterns
 * - Semantic search: Locate concepts in semantic network
 * - Disambiguation: Quantum superposition for polysemous words
 * - Spreading activation: Quantum walk on semantic graph
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_quantum_bridge(brain_t brain);

/**
 * @brief Initialize Wernicke-Broca bridge (arcuate fasciculus)
 *
 * WHAT: Creates bridge connecting Wernicke's and Broca's areas
 * WHY:  Language comprehension and production require bidirectional flow
 * HOW:  Implements dorsal (phonological) and ventral (semantic) streams
 *
 * PATHWAYS:
 * - Dorsal: Wernicke STG -> Arcuate -> Broca (phonological loop)
 * - Ventral: Wernicke -> MTG -> ATL -> Broca (semantic pathway)
 * - Efference copy: Broca -> Wernicke (self-monitoring)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_broca_bridge(brain_t brain);

/**
 * @brief Initialize Wernicke omnidirectional inference bridge
 *
 * WHAT: Creates bridge to omnidirectional inference system
 * WHY:  Language comprehension is inherently predictive
 * HOW:  Integrates with JEPA, predictive hierarchy, Hopfield memory
 *
 * APPLICATIONS:
 * - Phoneme prediction: Anticipate next sound from context
 * - Word prediction: Complete words from partial input
 * - Semantic prediction: Activate related concepts
 * - N400 generation: Prediction error for semantic violations
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_omni_bridge(brain_t brain);

/**
 * @brief Initialize Wernicke GPU acceleration bridge
 *
 * WHAT: Creates bridge to GPU for parallel language processing
 * WHY:  Phoneme and word recognition are massively parallel
 * HOW:  CUDA kernels for phoneme matching, cohort activation, spreading
 *
 * GPU OPERATIONS:
 * - Phoneme recognition: Parallel pattern matching
 * - Word recognition: Cohort model with parallel pruning
 * - Spreading activation: Parallel semantic network propagation
 * - Batch processing: Multiple frames in single dispatch
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_gpu_bridge(brain_t brain);

/**
 * @brief Connect Wernicke's region to semantic memory
 *
 * WHAT: Links Wernicke to semantic memory for concept access
 * WHY:  Language comprehension requires concept network access
 * HOW:  Registers Wernicke as semantic memory consumer
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_wernicke_to_semantic_memory(brain_t brain);

/**
 * @brief Connect Wernicke's region to working memory
 *
 * WHAT: Links Wernicke to working memory for lexical access
 * WHY:  Comprehension requires active maintenance of word candidates
 * HOW:  Registers Wernicke as WM consumer for phonological loop
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_wernicke_to_working_memory(brain_t brain);

/**
 * @brief Connect Wernicke's region to training system
 *
 * WHAT: Links Wernicke to training for language comprehension learning
 * WHY:  Vocabulary and semantic associations improve through exposure
 * HOW:  Registers Wernicke adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_wernicke_to_training(brain_t brain);

/**
 * @brief Connect Wernicke's region to immune system
 *
 * WHAT: Links Wernicke to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects language comprehension
 * HOW:  Registers for cytokine signals that modulate processing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_wernicke_to_immune(brain_t brain);

/**
 * @brief Connect Wernicke's region to bio-async messaging
 *
 * WHAT: Links Wernicke to bio-async router for inter-module messaging
 * WHY:  Asynchronous processing of language events
 * HOW:  Registers message handlers for language comprehension events
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_wernicke_to_bio_async(brain_t brain);

/**
 * @brief Initialize Wernicke immune bridge
 *
 * WHAT: Creates bidirectional Wernicke-immune integration
 * WHY:  Neuroinflammation affects language comprehension (Wernicke's aphasia)
 * HOW:  Creates immune bridge, registers for cytokine signals
 *
 * EFFECTS:
 * - Inflammation -> Comprehension impairment
 * - Comprehension errors -> Immune surveillance
 * - Models Wernicke's aphasia symptoms
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_immune_bridge(brain_t brain);

/**
 * @brief Initialize Wernicke NLP bridge
 *
 * WHAT: Creates comprehensive NLP integration for Wernicke's area
 * WHY:  Language comprehension requires coordination with all NLP modules
 * HOW:  Connects Wernicke to speech cortex, NLP network, semantic memory,
 *       multimodal bridge, knowledge graph, and working memory
 *
 * CONNECTIONS:
 * - Speech Cortex: Phoneme input from auditory processing
 * - NLP Network: Token embedding and attention processing
 * - Semantic Memory: Concept activation and spreading
 * - Multimodal Bridge: Audiovisual integration (McGurk effect)
 * - Knowledge Graph: Self-awareness and concept registration
 * - Working Memory: Phonological loop for active maintenance
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_wernicke_nlp_bridge(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_WERNICKE_H */

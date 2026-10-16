//=============================================================================
// nimcp_brain_init_temporal.h - Temporal Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_temporal.h
 * @brief Temporal Cortex Initialization Functions
 *
 * WHAT: Initialization functions for temporal cortex (auditory, object recognition, semantic memory)
 * WHY:  SRP refactoring - separate temporal initialization logic
 * HOW:  Creates temporal adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) for auditory processing
 * - Brodmann areas 41/42 (A1/A2) for primary/secondary auditory cortex
 * - Inferotemporal cortex (IT) for object/face recognition
 * - Anterior temporal lobe for semantic memory
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of perception accuracy
 * - Thalamic Bridge: MGN routing of auditory signals
 * - Quantum Bridge: Grover-accelerated object/concept search
 *
 * @version Phase T1: Temporal Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_TEMPORAL_H
#define NIMCP_BRAIN_INIT_TEMPORAL_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Temporal Cortex Initialization API
//=============================================================================

/**
 * @brief Initialize temporal cortex subsystem
 *
 * WHAT: Creates temporal adapter and connects all integration bridges
 * WHY:  Enable auditory processing, object recognition, and semantic memory
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Temporal cortex is essential for perception and semantic knowledge
 * - Damage causes agnosia (object), prosopagnosia (face), semantic dementia
 * - Integrates auditory, visual, and conceptual processing
 *
 * INITIALIZATION ORDER:
 * 1. Create temporal adapter (core auditory/object/semantic)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (MGN signal routing)
 * 4. Create quantum bridge (accelerated search)
 * 5. Connect to working memory
 * 6. Connect to training system
 * 7. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_temporal_subsystem(brain_t brain);

/**
 * @brief Initialize temporal substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Perception depends on metabolic state (ATP, fatigue)
 * HOW:  Links temporal to substrate for biologically-realistic processing
 *
 * EFFECTS:
 * - Low ATP -> Reduced auditory acuity, slower object recognition
 * - High fatigue -> Impaired semantic retrieval, reduced spreading
 * - Metabolic stress -> Perception errors, missed speech
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_temporal_substrate_bridge(brain_t brain);

/**
 * @brief Initialize temporal thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Auditory signals flow through MGN to A1
 * HOW:  Routes auditory through MGN, visual through pulvinar
 *
 * PATHWAYS:
 * - Auditory: Cochlea -> MGN -> A1 (primary auditory cortex)
 * - Visual: V4/IT -> Pulvinar -> Temporal association areas
 * - Semantic: Prefrontal -> MD -> Temporal (top-down)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_temporal_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize temporal quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Object search in large prototype libraries benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) object/concept access
 *
 * APPLICATIONS:
 * - Object recognition: Quantum search through visual prototypes
 * - Semantic retrieval: Grover search for concept by embedding
 * - Multimodal binding: Quantum interference for audio-visual integration
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_temporal_quantum_bridge(brain_t brain);

/**
 * @brief Connect temporal cortex to working memory
 *
 * WHAT: Links temporal to working memory for conceptual access
 * WHY:  Semantic processing requires active maintenance of concepts
 * HOW:  Registers temporal as WM consumer for conceptual slots
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_temporal_to_working_memory(brain_t brain);

/**
 * @brief Connect temporal cortex to training system
 *
 * WHAT: Links temporal to training for perception/memory learning
 * WHY:  Object recognition and semantic associations improve through learning
 * HOW:  Registers temporal adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_temporal_to_training(brain_t brain);

/**
 * @brief Connect temporal cortex to immune system
 *
 * WHAT: Links temporal to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects perception and memory
 * HOW:  Registers for cytokine signals that modulate processing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_temporal_to_immune(brain_t brain);

/**
 * @brief Connect temporal cortex to hippocampus
 *
 * WHAT: Links temporal cortex to hippocampal memory system
 * WHY:  Semantic memories are encoded/retrieved via hippocampus
 * HOW:  Bidirectional connections for memory consolidation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_temporal_to_hippocampus(brain_t brain);

/**
 * @brief Connect temporal cortex to frontal cortex
 *
 * WHAT: Links temporal to prefrontal for executive control
 * WHY:  Top-down semantic processing requires prefrontal modulation
 * HOW:  Feedback connections for attention and goal-directed retrieval
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_temporal_to_frontal(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_TEMPORAL_H */

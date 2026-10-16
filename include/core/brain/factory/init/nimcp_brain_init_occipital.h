//=============================================================================
// nimcp_brain_init_occipital.h - Occipital Cortex (Visual Cortex) Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_occipital.h
 * @brief Occipital Cortex (Visual Cortex) Initialization Functions
 *
 * WHAT: Initialization functions for Occipital Cortex (visual processing)
 * WHY:  SRP refactoring - separate occipital initialization logic
 * HOW:  Creates occipital adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 17 (V1), 18 (V2), 19 (V3, V4, V5/MT)
 * - Primary visual processing: edge detection, orientation selectivity
 * - Visual hierarchy: V1 -> V2 -> V3 -> (V4 | V5/MT)
 * - Dorsal "where" stream: motion and spatial processing
 * - Ventral "what" stream: object and color processing
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of visual processing
 * - Thalamic Bridge: LGN routing of visual input signals
 * - Quantum Bridge: Grover-accelerated visual search and feature binding
 * - Audiovisual Bridge: Lip reading, gesture-speech binding
 * - Cortical Bridge: Orientation hypercolumns, retinotopic mapping
 * - Logic Bridge: Visual predicates, scene reasoning
 * - Cognitive Bridge: Unified connection to all cognitive modules
 *
 * @version Phase O1: Occipital Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_OCCIPITAL_H
#define NIMCP_BRAIN_INIT_OCCIPITAL_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Occipital Cortex Initialization API
//=============================================================================

/**
 * @brief Initialize Occipital Cortex subsystem
 *
 * WHAT: Creates occipital adapter and connects all integration bridges
 * WHY:  Enable visual processing capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Occipital cortex is essential for visual perception
 * - Damage causes cortical blindness, visual agnosia
 * - Integrates edge detection, motion, color, and form processing
 *
 * INITIALIZATION ORDER:
 * 1. Create occipital adapter (core V1-V5 processing)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (LGN signal routing)
 * 4. Create quantum bridge (accelerated visual search)
 * 5. Connect to visual cortex (perception layer)
 * 6. Connect to parietal cortex (dorsal stream)
 * 7. Connect to temporal cortex (ventral stream)
 * 8. Connect to training system
 * 9. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_occipital_subsystem(brain_t brain);

/**
 * @brief Initialize Occipital's substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Visual processing depends on metabolic state (ATP, fatigue)
 * HOW:  Links occipital to substrate for biologically-realistic processing
 *
 * EFFECTS:
 * - Low ATP -> Reduced edge detection sensitivity
 * - High fatigue -> Impaired motion perception
 * - Stress -> Color perception disruption
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_occipital_substrate_bridge(brain_t brain);

/**
 * @brief Initialize Occipital's thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for LGN signal routing
 * WHY:  Visual input is relayed through LGN before reaching V1
 * HOW:  Routes visual signals through LGN, Pulvinar for attention
 *
 * PATHWAYS:
 * - Retina -> LGN -> V1 (primary visual pathway)
 * - V1 <-> Pulvinar (attention modulation)
 * - Superior Colliculus -> Pulvinar -> V5/MT (saccade-related motion)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_occipital_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize Occipital's quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Visual search in complex scenes benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) visual search
 *
 * APPLICATIONS:
 * - Visual search: Find target among distractors
 * - Feature binding: Group features into coherent objects
 * - Scene segmentation: Optimal figure-ground assignment
 * - Motion integration: Combine local motions into global flow
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_occipital_quantum_bridge(brain_t brain);

/**
 * @brief Connect Occipital cortex to parietal cortex (dorsal stream)
 *
 * WHAT: Links occipital to parietal for "where" pathway
 * WHY:  Dorsal stream handles spatial processing and action guidance
 * HOW:  V5/MT motion signals to parietal for spatial awareness
 *
 * PATHWAY:
 * - V1 -> V2 -> V3 -> V5/MT -> Parietal (posterior parietal cortex)
 * - Handles: motion perception, spatial attention, visually-guided action
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_parietal(brain_t brain);

/**
 * @brief Connect Occipital cortex to temporal cortex (ventral stream)
 *
 * WHAT: Links occipital to temporal for "what" pathway
 * WHY:  Ventral stream handles object recognition and categorization
 * HOW:  V4 color/form signals to temporal for object identification
 *
 * PATHWAY:
 * - V1 -> V2 -> V4 -> Temporal (IT cortex, fusiform face area)
 * - Handles: object recognition, face recognition, scene categorization
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_temporal(brain_t brain);

/**
 * @brief Connect Occipital cortex to dragonfly system
 *
 * WHAT: Links occipital to dragonfly for target tracking
 * WHY:  Dragonfly target tracking needs visual motion input
 * HOW:  V5/MT motion signals feed dragonfly TSDN/CSTMD1
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_dragonfly(brain_t brain);

/**
 * @brief Connect Occipital cortex to training system
 *
 * WHAT: Links occipital to training for visual learning
 * WHY:  Visual processing improves through experience and feedback
 * HOW:  Registers occipital adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_training(brain_t brain);

/**
 * @brief Connect Occipital cortex to immune system
 *
 * WHAT: Links occipital to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects visual processing
 * HOW:  Registers for cytokine signals that modulate visual sensitivity
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_immune(brain_t brain);

/**
 * @brief Connect Occipital cortex to logic module
 *
 * WHAT: Links occipital to symbolic/neural logic for visual reasoning
 * WHY:  Visual perception informs logical reasoning (object grounding)
 * HOW:  Creates visual-logic bridge for perception-to-predicate conversion
 *
 * BIOLOGICAL BASIS:
 * - Inferotemporal cortex links visual features to semantic concepts
 * - Prefrontal cortex integrates visual evidence with logical reasoning
 * - Top-down attention from logic modulates visual processing
 *
 * PATHWAYS:
 * - Visual -> Logic: Feature extraction, object recognition -> predicate grounding
 * - Logic -> Visual: Conclusion-based attention guidance, expectation priming
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_logic(brain_t brain);

/**
 * @brief Connect Occipital cortex to cognitive modules
 *
 * WHAT: Links occipital to salience, curiosity, introspection, attention
 * WHY:  Cognitive systems modulate and are informed by visual processing
 * HOW:  Registers callbacks for visual novelty, salience, attention
 *
 * INTEGRATED MODULES:
 * - Salience: Visual salience detection for attention guidance
 * - Curiosity: Novel visual stimuli drive exploration
 * - Introspection: Visual processing state for self-awareness
 * - Attention: Visual attention allocation and gating
 * - Global Workspace: Conscious visual awareness broadcasting
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_cognitive(brain_t brain);

/**
 * @brief Connect Occipital cortex to cortical columns
 *
 * WHAT: Links occipital to cortical column architecture
 * WHY:  V1 is organized in orientation columns and hypercolumns
 * HOW:  Creates columnar organization for orientation selectivity
 *
 * BIOLOGICAL BASIS:
 * - V1 hypercolumns: Complete orientation coverage (0-180 degrees)
 * - Orientation columns: ~0.5mm spacing, ~15-20 degree intervals
 * - Ocular dominance columns: Left/right eye preference
 * - Color blobs: CO-rich regions for color processing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_cortical_columns(brain_t brain);

/**
 * @brief Connect Occipital cortex to swarm system
 *
 * WHAT: Links occipital to swarm intelligence for distributed visual processing
 * WHY:  Complex visual scenes benefit from parallel distributed processing
 * HOW:  Registers visual processing nodes with swarm coordination
 *
 * APPLICATIONS:
 * - Distributed object detection: Multiple swarm nodes process scene regions
 * - Consensus perception: Aggregate visual interpretations across nodes
 * - Fault-tolerant vision: Visual processing continues if nodes fail
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_swarm(brain_t brain);

/**
 * @brief Connect Occipital cortex to Broca's region
 *
 * WHAT: Links occipital to Broca for audiovisual speech processing
 * WHY:  Visual lip reading and gesture recognition inform speech production
 * HOW:  Creates audiovisual bridge for lip-to-phoneme and gesture-speech binding
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Sulcus (STS) integrates visual and auditory speech
 * - Lip movements precede audio by ~150ms, enabling prediction
 * - McGurk effect demonstrates visual-audio speech integration
 * - Mirror neurons in Broca link gesture observation to motor plans
 *
 * PATHWAYS:
 * - Occipital V4/V5 -> STS -> Broca (visual speech cues)
 * - Occipital face area -> STS -> Broca (lip reading)
 * - Occipital V5/MT -> Broca (gesture motion -> articulation)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_occipital_to_broca(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_OCCIPITAL_H */

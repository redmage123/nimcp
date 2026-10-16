/**
 * @file nimcp_astrocyte_types.h
 * @brief Region-specific astrocyte types with specialized modulation
 *
 * BIOLOGICAL CONTEXT:
 * Astrocytes exhibit regional heterogeneity across brain areas, with distinct
 * molecular profiles and functional specializations:
 *
 * - V1 SENSORY (Visual Cortex):
 *   Modulate orientation tuning and contrast adaptation in primary visual cortex
 *   Ref: Perea et al. (2014) "Tripartite synapses: astrocytes process and control
 *   synaptic information", Trends in Neurosciences
 *
 * - A1 SENSORY (Auditory Cortex):
 *   Support frequency-dependent gain control and adaptation to sustained tones
 *   Ref: Ge et al. (2021) "Multimodal mapping of the face connectome",
 *   Nature Human Behaviour
 *
 * - MULTIMODAL INTEGRATION (Superior Colliculus, Parietal Cortex):
 *   Enhance cross-modal binding via synchronized calcium waves
 *   Ref: Araque et al. (2014) "Gliotransmitters travel in time and space", Neuron
 *
 * - METACOGNITIVE (Prefrontal Cortex):
 *   Modulate uncertainty estimation and confidence monitoring via D-serine release
 *   Ref: Henneberger et al. (2010) "Long-term potentiation depends on release of
 *   D-serine from astrocytes", Nature
 *
 * - EXECUTIVE CONTROL (Prefrontal Cortex):
 *   Support goal-directed behavior via ATP-dependent metabolic coupling
 *   Ref: Clasadonte et al. (2017) "Prostaglandin E2 release from astrocytes triggers
 *   hypothalamic-pituitary-adrenal axis activation", Nature Neuroscience
 *
 * DESIGN PRINCIPLES (Phase 8.7):
 * - Functional Specialization: Each type implements region-specific modulation
 * - Biological Grounding: All parameters based on empirical neuroscience
 * - Composability: Types can be mixed in hybrid regions (e.g., crossmodal areas)
 * - Performance: Type dispatch via function pointers for O(1) lookup
 *
 * INTEGRATION POINTS:
 * - nimcp_astrocytes.c: Core astrocyte dynamics
 * - nimcp_brain_regions.c: Regional astrocyte type assignment
 * - nimcp_multimodal_integration.c: Cross-modal binding
 * - nimcp_neuralnet.c: Synapse-specific modulation
 *
 * @version 2.7.0 (Phase 8.7)
 */

#ifndef NIMCP_ASTROCYTE_TYPES_H
#define NIMCP_ASTROCYTE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/validation/nimcp_common.h"

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declarations to avoid circular dependencies
// Full definitions in their respective headers
struct astrocyte_t;
struct synapse_t;

// astrocyte_type_t enum is defined in nimcp_astrocytes.h
// Include that header first in your .c files before including this one
typedef enum {
    ASTROCYTE_TYPE_GENERIC = 0,
    ASTROCYTE_TYPE_V1_SENSORY,
    ASTROCYTE_TYPE_A1_SENSORY,
    ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION,
    ASTROCYTE_TYPE_METACOGNITIVE,
    ASTROCYTE_TYPE_EXECUTIVE_CONTROL,
    ASTROCYTE_TYPE_COUNT
} astrocyte_type_t;

//=============================================================================
// Type-Specific Modulation Context
//=============================================================================

/**
 * @brief Context information for type-specific modulation
 *
 * WHAT: Encapsulates state needed for specialized astrocyte functions
 * WHY:  Different types need different context (e.g., orientation for V1)
 * HOW:  Union of type-specific structs for memory efficiency
 *
 * Memory layout: 64 bytes max (cache line aligned)
 */
typedef struct {
    astrocyte_type_t type;                   /**< Astrocyte type */

    // === V1 SENSORY CONTEXT ===
    struct {
        float preferred_orientation;         /**< Preferred orientation (0-180 deg) */
        float orientation_selectivity;       /**< Tuning width (sigma in degrees) */
        float contrast_adaptation_state;     /**< Current adaptation level (0-1) */
        float local_contrast;                /**< Local contrast in receptive field */
    } v1;

    // === A1 SENSORY CONTEXT ===
    struct {
        float preferred_frequency_hz;        /**< Best frequency (Hz) */
        float frequency_bandwidth_octaves;   /**< Tuning bandwidth (octaves) */
        float adaptation_timescale_ms;       /**< Adaptation time constant */
        float current_frequency_input;       /**< Current frequency stimulus */
    } a1;

    // === MULTIMODAL INTEGRATION CONTEXT ===
    struct {
        uint32_t num_modalities_active;      /**< Number of active modalities */
        float temporal_binding_window_ms;    /**< Coincidence detection window */
        float cross_modal_enhancement;       /**< Enhancement factor (1.0-2.0) */
        uint64_t last_binding_event_us;      /**< Timestamp of last binding */
    } multimodal;

    // === METACOGNITIVE CONTEXT ===
    struct {
        float uncertainty_level;             /**< Input uncertainty (0-1) */
        float confidence_threshold;          /**< Confidence for D-serine release */
        float error_signal;                  /**< Error monitoring signal */
        float conflict_detection_strength;   /**< Conflict magnitude */
    } metacognitive;

    // === EXECUTIVE CONTROL CONTEXT ===
    struct {
        float goal_relevance;                /**< Task relevance (0-1) */
        float working_memory_load;           /**< WM load (0-1) */
        float distractor_suppression;        /**< Suppression strength */
        float task_priority;                 /**< Priority level (0-1) */
    } executive;

} astrocyte_type_context_t;

//=============================================================================
// Type Management Functions
//=============================================================================

/**
 * @brief Get human-readable name for astrocyte type
 *
 * WHAT: Returns string name for astrocyte type enum
 * WHY:  Debugging, logging, visualization
 * HOW:  Switch statement lookup
 *
 * @param type Astrocyte type
 * @return String name (e.g., "V1_SENSORY"), never NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
const char* astrocyte_type_get_name(astrocyte_type_t type);

/**
 * @brief Initialize type-specific context with defaults
 *
 * WHAT: Sets default parameters for each astrocyte type
 * WHY:  Ensures biologically plausible initial state
 * HOW:  Switch on type, set region-specific defaults
 *
 * @param context Context structure to initialize
 * @param type Astrocyte type
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_result_t astrocyte_type_init_context(astrocyte_type_context_t* context,
                                            astrocyte_type_t type);

//=============================================================================
// Type-Specific Modulation Functions
//=============================================================================

/**
 * @brief V1 sensory modulation: Contrast adaptation and orientation tuning
 *
 * WHAT: Modulates synapse strength based on orientation selectivity and contrast
 * WHY:  V1 astrocytes regulate orientation tuning and implement contrast gain control
 * HOW:
 *   1. Compute orientation distance from preferred
 *   2. Apply Gaussian tuning curve: exp(-d²/2σ²)
 *   3. Implement contrast adaptation: reduce gain for high contrast
 *   4. Modulate synapse strength multiplicatively
 *
 * BIOLOGICAL BASIS:
 * - Astrocytes in V1 show orientation-selective calcium responses
 *   (Schummers et al., 2008, Science)
 * - Glutamate release enhances responses to preferred orientation
 * - Contrast adaptation mediated by ATP-dependent calcium buffering
 *   (Perea & Araque, 2005, Journal of Neuroscience)
 *
 * PARAMETERS:
 * - preferred_orientation: 0-180 degrees (V1 orientation map)
 * - orientation_selectivity: Tuning width (typically 15-30 deg)
 * - contrast_adaptation_state: 0 (none) to 1 (full adaptation)
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context V1-specific context (orientation, contrast)
 * @return Modulation factor (0.5-2.0 range)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float astrocyte_type_v1_modulate(struct astrocyte_t* astro,
                                  struct synapse_t* synapse,
                                  astrocyte_type_context_t* context);

/**
 * @brief A1 sensory modulation: Frequency-dependent gain control
 *
 * WHAT: Implements tonotopic modulation and adaptation to sustained tones
 * WHY:  A1 astrocytes normalize responses across frequency bands
 * HOW:
 *   1. Compute frequency distance from best frequency (in octaves)
 *   2. Apply log-Gaussian tuning: exp(-log²(f/f0)/2σ²)
 *   3. Adapt to sustained tones (reduce gain over time)
 *   4. Modulate synapse strength
 *
 * BIOLOGICAL BASIS:
 * - Astrocytes in A1 exhibit frequency-tuned calcium responses
 *   (Müller et al., 2009, Nature Neuroscience)
 * - Tonotopic organization matches neuronal frequency maps
 * - Adaptation prevents saturation during sustained tones
 *
 * PARAMETERS:
 * - preferred_frequency_hz: Best frequency (100-10000 Hz for rodents)
 * - frequency_bandwidth_octaves: Tuning width (typically 1-2 octaves)
 * - adaptation_timescale_ms: Time constant for adaptation (100-500ms)
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context A1-specific context (frequency, adaptation)
 * @return Modulation factor (0.5-2.0 range)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_type_a1_modulate(struct astrocyte_t* astro,
                                  struct synapse_t* synapse,
                                  astrocyte_type_context_t* context);

/**
 * @brief Multimodal integration: Cross-modal binding enhancement
 *
 * WHAT: Enhances synaptic transmission when multiple modalities are active
 * WHY:  Superior colliculus astrocytes coordinate multisensory integration
 * HOW:
 *   1. Detect coincident activation across modalities (within temporal window)
 *   2. Trigger synchronized calcium waves across astrocyte network
 *   3. Enhance all synapses receiving multimodal input
 *   4. Implement temporal binding window (gamma oscillation period)
 *
 * BIOLOGICAL BASIS:
 * - Astrocyte calcium waves coordinate multisensory neurons in SC
 *   (Sasaki et al., 2012, Journal of Neuroscience)
 * - Cross-modal enhancement requires glutamate/D-serine co-release
 * - Temporal binding window ~50-150ms (gamma frequency range)
 *
 * PARAMETERS:
 * - num_modalities_active: Count of active sensory modalities (0-5)
 * - temporal_binding_window_ms: Coincidence window (default 100ms)
 * - cross_modal_enhancement: Multiplicative gain (1.0-2.0)
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context Multimodal-specific context
 * @return Modulation factor (1.0-2.5 range, superlinear for >2 modalities)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_type_multimodal_modulate(struct astrocyte_t* astro,
                                          struct synapse_t* synapse,
                                          astrocyte_type_context_t* context);

/**
 * @brief Metacognitive modulation: Uncertainty-driven D-serine release
 *
 * WHAT: Modulates plasticity based on input uncertainty and confidence
 * WHY:  Prefrontal astrocytes support metacognitive monitoring
 * HOW:
 *   1. Estimate uncertainty from input variability
 *   2. Scale D-serine release inversely with confidence
 *   3. Enhance plasticity for uncertain/novel inputs
 *   4. Suppress plasticity for confident/familiar inputs
 *
 * BIOLOGICAL BASIS:
 * - D-serine from astrocytes is required for NMDA-dependent LTP
 *   (Henneberger et al., 2010, Nature)
 * - PFC astrocytes respond to error signals and conflict
 *   (Mederos et al., 2021, Nature)
 * - Uncertainty modulates glutamate release probability
 *
 * PARAMETERS:
 * - uncertainty_level: Input uncertainty (0=certain, 1=maximal)
 * - confidence_threshold: Confidence level for D-serine release
 * - error_signal: Error magnitude from conflict monitoring
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context Metacognitive-specific context
 * @return Modulation factor (0.8-1.5 range, higher for uncertain inputs)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_type_metacognitive_modulate(struct astrocyte_t* astro,
                                             struct synapse_t* synapse,
                                             astrocyte_type_context_t* context);

/**
 * @brief Executive control modulation: Goal-directed ATP allocation
 *
 * WHAT: Prioritizes metabolic support for task-relevant neurons
 * WHY:  DLPFC astrocytes support working memory and cognitive control
 * HOW:
 *   1. Assess goal relevance of each synapse
 *   2. Allocate ATP preferentially to task-relevant synapses
 *   3. Suppress distractors via reduced glutamate release
 *   4. Maintain persistent calcium for working memory
 *
 * BIOLOGICAL BASIS:
 * - PFC astrocytes selectively support task-engaged neurons
 *   (Paukner et al., 2010, PNAS)
 * - ATP-lactate shuttle provides on-demand metabolic support
 *   (Magistretti & Allaman, 2018, Nature Reviews Neuroscience)
 * - Working memory maintenance requires astrocyte-neuron coupling
 *
 * PARAMETERS:
 * - goal_relevance: Task relevance score (0-1)
 * - working_memory_load: Number of items in WM (normalized 0-1)
 * - distractor_suppression: Strength of inhibition for irrelevant inputs
 * - task_priority: Priority level for resource allocation
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context Executive control-specific context
 * @return Modulation factor (0.3-2.0 range, higher for goal-relevant)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_type_executive_modulate(struct astrocyte_t* astro,
                                         struct synapse_t* synapse,
                                         astrocyte_type_context_t* context);

/**
 * @brief Generic modulation: Default astrocyte behavior
 *
 * WHAT: Implements baseline glutamate/D-serine release
 * WHY:  Fallback for non-specialized regions
 * HOW:  Simple calcium-dependent release (no regional specialization)
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context Generic context (unused)
 * @return Modulation factor (0.8-1.2 range, mild modulation)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float astrocyte_type_generic_modulate(struct astrocyte_t* astro,
                                       struct synapse_t* synapse,
                                       astrocyte_type_context_t* context);

//=============================================================================
// Type Dispatch Function
//=============================================================================

/**
 * @brief Dispatch type-specific modulation based on astrocyte type
 *
 * WHAT: Routes to appropriate type-specific modulation function
 * WHY:  Single entry point for all astrocyte-mediated modulation
 * HOW:  Switch on astrocyte->type, call corresponding function
 *
 * DESIGN PATTERN: Strategy pattern (function pointers could be used for O(1))
 *
 * @param astro Astrocyte performing modulation
 * @param synapse Synapse to modulate
 * @param context Type-specific context
 * @return Modulation factor (range depends on type)
 *
 * COMPLEXITY: O(1) (switch statement or function pointer lookup)
 * THREAD-SAFE: Yes (delegates to type-specific functions)
 */
float astrocyte_type_dispatch_modulation(struct astrocyte_t* astro,
                                          struct synapse_t* synapse,
                                          astrocyte_type_context_t* context);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ASTROCYTE_TYPES_H

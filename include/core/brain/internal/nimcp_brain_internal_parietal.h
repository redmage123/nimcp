//=============================================================================
// nimcp_brain_internal_parietal.h - Parietal & Intuition Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_parietal.h
 * @brief Internal brain_struct fields for parietal lobe and intuition system
 *
 * WHAT: Defines brain_struct fields for mathematical/scientific reasoning
 * WHY:  Modularize brain_internal.h - separate parietal/intuition fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SUBSYSTEMS:
 * - Parietal Lobe: Mathematical and scientific reasoning (Phase 7.2)
 * - Intuition System: Creative and intuitive reasoning (Phase 6)
 * - Knowledge Graph Reader: Structural self-awareness
 *
 * CAPABILITIES:
 * - Number Sense: Weber-Fechner law, subitizing, magnitude
 * - Spatial Reasoning: Mental rotation, coordinate transforms
 * - Mathematical Intuition: Pattern detection, analogy
 * - Scientific Reasoning: Hypothesis testing, causal inference
 * - Intuitive Reasoning: Hunches, analogies, insights
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Parietal Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_PARIETAL_H
#define NIMCP_BRAIN_INTERNAL_PARIETAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Parietal Types
//=============================================================================

// Parietal Lobe
struct parietal_lobe;
typedef struct parietal_lobe parietal_lobe_t;

// Intuition System (Phase 6 integration)
struct intuition_system;
typedef struct intuition_system intuition_system_t;

// Knowledge Graph Reader (self-awareness)
struct kg_reader;

//=============================================================================
// Parietal & Intuition Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining parietal/intuition fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. PARIETAL LOBE (Phase 7.2: Mathematical/Scientific Reasoning)
 *    - Number Sense: Weber-Fechner, subitizing, magnitude
 *    - Spatial Reasoning: Mental rotation, symmetry
 *    - Mathematical Intuition: Patterns, analogy, extrapolation
 *    - Scientific Reasoning: Hypothesis testing, dimensional analysis
 *    - Equation Manipulation: Symbolic math, differentiation
 *
 * 2. INTUITION SYSTEM (Phase 6: Creative/Intuitive Reasoning)
 *    - Intuitive Reasoning: Pattern-based hunch generation
 *    - Analogical Reasoning: Cross-domain mapping
 *    - Insight Discovery: Aha! moments, restructuring
 *    - Hypothesis Generation: Abductive reasoning
 *    - Conceptual Blending: Novel concept synthesis
 *    - Counterfactual Reasoning: What-if scenarios
 *    - Meta-Reasoning: Reasoning about reasoning
 *
 * 3. KNOWLEDGE GRAPH READER (Self-Awareness)
 *    - Entities: Modules, integrations, conventions
 *    - Relations: Component interactions
 *    - Observations: Capabilities, file locations
 */
#define BRAIN_INTERNAL_FIELDS_PARIETAL                                         \
    /* === PARIETAL LOBE (Mathematical/Scientific Reasoning) === */            \
    parietal_lobe_t* parietal;                  /* Parietal lobe */            \
    bool parietal_enabled;                      /* Parietal enabled */         \
    uint64_t last_parietal_update_us;           /* Last parietal update */     \
                                                                               \
    /* === INTUITION SYSTEM (Creative/Intuitive Reasoning) === */              \
    intuition_system_t* intuition_system;       /* Phase 6 intuition */        \
    bool intuition_system_enabled;              /* Intuition enabled */        \
    uint64_t last_intuition_update_us;          /* Last intuition update */    \
                                                                               \
    /* === KNOWLEDGE GRAPH READER (Self-Awareness) === */                      \
    struct kg_reader* kg_reader;                /* KG reader for introspection */ \
    bool kg_reader_enabled;                     /* KG reader enabled */        \
    char kg_file_path[512];                     /* Path to KG file */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_PARIETAL_H */

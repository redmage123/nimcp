//=============================================================================
// nimcp_brain_fwd.h - Forward Declarations for Brain Types
//=============================================================================
/**
 * @file nimcp_brain_fwd.h
 * @brief Forward declarations for brain-related types
 *
 * WHAT: Forward declarations to break circular dependencies
 * WHY:  Headers that only need pointers can include this instead of full headers
 * HOW:  Provides opaque pointer typedefs without struct definitions
 *
 * USAGE:
 *   // In headers that only need pointer types:
 *   #include "core/brain/nimcp_brain_fwd.h"
 *
 *   // Later in .c files that need full definitions:
 *   #include "core/brain/nimcp_brain_internal.h"
 *
 * BENEFIT: Reduces compilation dependencies and speeds up builds
 */

#ifndef NIMCP_BRAIN_FWD_H
#define NIMCP_BRAIN_FWD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Core Brain Types (opaque)
//=============================================================================

/** @brief Opaque brain handle */
typedef struct brain_struct* brain_t;

/** @brief Forward declaration for brain configuration */
typedef struct brain_config brain_config_t;

/** @brief Forward declaration for brain decision */
typedef struct brain_decision brain_decision_t;

/** @brief Forward declaration for brain statistics */
typedef struct brain_stats brain_stats_t;

/** @brief Forward declaration for task strategy */
typedef struct task_strategy task_strategy_t;

//=============================================================================
// Network Types (opaque)
//=============================================================================

/** @brief Forward declaration for adaptive network */
typedef struct adaptive_network adaptive_network_t;

/** @brief Forward declaration for neural network */
typedef struct nimcp_neural_network nimcp_neural_network_t;

//=============================================================================
// Cognitive Module Types (opaque pointers)
//=============================================================================

/** @brief Forward declaration for introspection context */
typedef struct introspection_ctx* introspection_context_t;

/** @brief Forward declaration for ethics engine */
typedef struct ethics_engine* ethics_engine_t;

/** @brief Forward declaration for salience evaluator */
typedef struct salience_evaluator* salience_evaluator_t;

/** @brief Forward declaration for consolidation handle */
typedef struct consolidation_ctx* consolidation_handle_t;

/** @brief Forward declaration for curiosity engine */
typedef struct curiosity_engine* curiosity_engine_t;

/** @brief Forward declaration for knowledge system */
typedef struct knowledge_system* knowledge_system_t;

/** @brief Forward declaration for working memory */
typedef struct working_memory working_memory_t;

/** @brief Forward declaration for executive controller */
typedef struct executive_controller executive_controller_t;

/** @brief Forward declaration for emotional system */
typedef struct emotional_system emotional_system_t;

/** @brief Forward declaration for theory of mind */
typedef struct theory_of_mind_s* theory_of_mind_t;

/** @brief Forward declaration for global workspace */
typedef struct global_workspace global_workspace_t;

//=============================================================================
// Perception Types (opaque)
//=============================================================================

/** @brief Forward declaration for visual cortex */
typedef struct visual_cortex visual_cortex_t;

/** @brief Forward declaration for audio cortex */
typedef struct audio_cortex audio_cortex_t;

/** @brief Forward declaration for speech cortex */
typedef struct speech_cortex speech_cortex_t;

//=============================================================================
// Security Types (opaque)
//=============================================================================

/** @brief Forward declaration for BBB system */
typedef struct bbb_system* bbb_system_t;

/** @brief Forward declaration for security integration */
typedef struct nimcp_sec_integration nimcp_sec_integration_t;

//=============================================================================
// Subsystem Types (opaque)
//=============================================================================

/** @brief Forward declaration for brain immune system */
typedef struct brain_immune_system brain_immune_system_t;

/** @brief Forward declaration for FEP orchestrator */
struct fep_orchestrator;

/** @brief Forward declaration for collective cognition */
struct collective_cognition;

/** @brief Forward declaration for medulla */
typedef struct medulla medulla_t;

/** @brief Forward declaration for parietal lobe */
typedef struct parietal_lobe parietal_lobe_t;

/** @brief Forward declaration for dragonfly system */
typedef struct dragonfly_system dragonfly_system_t;

/** @brief Forward declaration for basal ganglia */
typedef struct bg_enhanced bg_enhanced_t;

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_FWD_H

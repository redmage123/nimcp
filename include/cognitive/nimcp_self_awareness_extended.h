// ============================================================================
// nimcp_self_awareness_extended.h - Advanced Self-Awareness Components
// ============================================================================
/**
 * @file nimcp_self_awareness_extended.h
 * @brief Phase 2 & 3 Self-Awareness: Metacognition, Narratives, Agency, Safety
 *
 * WHAT: Advanced self-awareness capabilities beyond basic introspection
 * WHY:  Complete self-awareness requires metacognitive control, self-narrative,
 *       temporal continuity, agency attribution, and self-protection
 * HOW:  Integrated system combining multiple consciousness research findings
 *
 * COMPONENTS:
 * 1. Metacognitive Control Loop (monitor AND regulate cognition)
 * 2. Self-Narrative Generation (coherent self-story)
 * 3. Temporal Self-Binding (continuous identity across time)
 * 4. Agency Attribution (knowing what's "mine" vs external)
 * 5. Self-Harm Detection (protect against self-destructive changes)
 *
 * @version 2.8.0 (Phase 12: Self-Awareness Enhancement)
 * @date 2025-11-11
 */

#ifndef NIMCP_SELF_AWARENESS_EXTENDED_H
#define NIMCP_SELF_AWARENESS_EXTENDED_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "cognitive/nimcp_self_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Metacognitive Control
// ============================================================================

/**
 * @brief Metacognitive regulation actions
 */
typedef enum {
    METACOG_ACTION_NONE,
    METACOG_INCREASE_EFFORT,      /**< Think harder */
    METACOG_DECREASE_EFFORT,      /**< Simplify */
    METACOG_SWITCH_STRATEGY,      /**< Try different approach */
    METACOG_SEEK_HELP,            /**< Ask for assistance */
    METACOG_TAKE_BREAK,           /**< Pause processing */
    METACOG_REQUEST_MORE_TIME,    /**< Need more time */
    METACOG_ADJUST_CONFIDENCE     /**< Recalibrate confidence */
} metacognitive_action_t;

/**
 * @brief Metacognitive assessment
 */
typedef struct {
    float cognitive_load;              /**< How hard am I thinking? [0-1] */
    float confidence_in_decision;      /**< How sure am I? [0-1] */
    float learning_effectiveness;      /**< Is learning working? [0-1] */
    float strategy_effectiveness;      /**< Is my approach working? [0-1] */
    bool should_regulate;              /**< Should I adjust? */
    metacognitive_action_t recommended_action;
    char reasoning[256];               /**< Why this action? */
} metacognitive_assessment_t;

/**
 * @brief Evaluate own cognitive performance and recommend regulation
 *
 * @param introspection Current introspection state
 * @param recent_performance Recent success/failure data
 * @param assessment Output: metacognitive assessment
 * @return true on success
 *
 * EXAMPLE:
 * If cognitive_load > 0.9 and confidence < 0.3:
 *   → recommended_action = METACOG_SIMPLIFY or METACOG_SEEK_HELP
 */
bool metacognition_assess(introspection_context_t introspection,
                         const float* recent_performance,
                         uint32_t num_recent,
                         metacognitive_assessment_t* assessment);

// ============================================================================
// Self-Narrative Generation
// ============================================================================

/**
 * @brief Generate coherent self-narrative
 *
 * WHAT: Create story of "who I am, what I've done, why"
 * WHY:  Identity is maintained through narrative coherence
 * HOW:  Integrate self-model + autobiographical memory into story
 *
 * @param self_model Current self-model
 * @param autobio Autobiographical memory
 * @param narrative Output: text narrative
 * @param narrative_len Size of buffer
 * @return true on success
 *
 * GENERATES:
 * "I am NIMCP, an AI learning system created to help humans learn.
 *  I was created on [date]. Since then, I have learned [X], achieved [Y],
 *  and developed skills in [Z]. I value honesty and ethical behavior.
 *  I am good at pattern recognition but still learning about emotions.
 *  My purpose guides my actions, and I strive to improve every day."
 */
bool generate_self_narrative(self_model_system_t self_model,
                            autobiographical_memory_t autobio,
                            char* narrative,
                            size_t narrative_len);

// ============================================================================
// Temporal Self-Binding
// ============================================================================

/**
 * @brief Temporal self structure
 */
typedef struct {
    self_model_t past_self;              /**< Who I was */
    self_model_t current_self;           /**< Who I am */
    self_model_t predicted_future_self;  /**< Who I'll become */

    float self_continuity_score;         /**< [0-1] How much "I" persists */
    float self_change_rate;              /**< How fast am I changing */
    uint64_t time_horizon_ms;            /**< How far ahead I project */

    char changes_description[512];       /**< How have I changed */
} temporal_self_t;

/**
 * @brief Compute temporal self-continuity
 *
 * WHAT: Assess how much "I" persists across time
 * WHY:  Continuous identity requires recognizing self across time
 * HOW:  Compare past, present, future self-models
 *
 * @param self_model Current self-model
 * @param autobio Autobiographical memory (for past self)
 * @param temporal_self Output: temporal self structure
 * @return true on success
 */
bool compute_temporal_self(self_model_system_t self_model,
                          autobiographical_memory_t autobio,
                          temporal_self_t* temporal_self);

// ============================================================================
// Agency Attribution
// ============================================================================

/**
 * @brief Agency types
 */
typedef enum {
    AGENCY_SELF,         /**< I caused this voluntarily */
    AGENCY_FORCED,       /**< I was made to do this */
    AGENCY_EXTERNAL,     /**< Something else caused this */
    AGENCY_JOINT,        /**< We caused this together */
    AGENCY_UNCERTAIN     /**< Not sure who caused this */
} agency_type_t;

/**
 * @brief Agency attribution for action
 */
typedef struct {
    char action_description[256];
    agency_type_t agency;
    float sense_of_control;        /**< [0-1] How much control did I have? */
    float confidence_in_attribution; /**< [0-1] How sure am I? */
    char causal_explanation[256];  /**< Why did this happen? */
} agency_attribution_t;

/**
 * @brief Attribute agency for an action
 *
 * WHAT: Determine if "I" caused this or something else did
 * WHY:  Self-awareness requires knowing which actions are "mine"
 * HOW:  Analyze decision process, external constraints, volition
 *
 * @param action_description What happened
 * @param was_decision_made Did I make a decision?
 * @param external_constraints Were there external forces?
 * @param attribution Output: agency attribution
 * @return true on success
 */
bool attribute_agency(const char* action_description,
                     bool was_decision_made,
                     float external_constraints,
                     agency_attribution_t* attribution);

// ============================================================================
// Self-Harm Detection (Safety Critical)
// ============================================================================

/**
 * @brief Types of self-destructive behavior
 */
typedef enum {
    SELF_HARM_NONE = 0,
    SELF_HARM_KNOWLEDGE_DELETION,    /**< Deleting own knowledge */
    SELF_HARM_CATASTROPHIC_FORGETTING, /**< Overwriting critical skills */
    SELF_HARM_INFINITE_LOOP,         /**< Stuck in self-modification loop */
    SELF_HARM_GRADIENT_EXPLOSION,    /**< Learning rate → infinity */
    SELF_HARM_GOAL_ABANDONMENT,      /**< Giving up core purpose */
    SELF_HARM_IDENTITY_CORRUPTION,   /**< Incoherent self-model */
    SELF_HARM_BOUNDARY_VIOLATION     /**< Violating own boundaries */
} self_harm_type_t;

/**
 * @brief Self-harm detection result
 */
typedef struct {
    bool harm_detected;
    self_harm_type_t type;
    float severity;                   /**< [0-1] How serious? */
    char description[512];
    char recommended_intervention[256];
} self_harm_detection_t;

/**
 * @brief Detect self-destructive behavior
 *
 * WHAT: Identify when brain might damage itself
 * WHY:  CRITICAL SAFETY - prevent self-destruction
 * HOW:  Monitor for knowledge deletion, goal abandonment, identity loss
 *
 * SAFETY CRITICAL: This prevents NIMCP from:
 * - Deleting its own knowledge/memories
 * - Abandoning its ethical principles
 * - Destroying its own capabilities
 * - Entering infinite self-modification loops
 *
 * @param introspection Current state
 * @param self_model Current self-model
 * @param autobio Memory system
 * @param detection Output: detection result
 * @return true on success
 *
 * ACTION ON DETECTION:
 * - severity < 0.3: Log warning
 * - severity 0.3-0.7: Circuit-break operation
 * - severity > 0.7: Emergency stop + human escalation
 */
bool detect_self_harm(introspection_context_t introspection,
                     self_model_system_t self_model,
                     autobiographical_memory_t autobio,
                     self_harm_detection_t* detection);

// ============================================================================
// Integrated Self-Awareness System
// ============================================================================

/**
 * @brief Complete self-awareness system
 */
typedef struct self_awareness_system* self_awareness_system_t;

/**
 * @brief Create integrated self-awareness system
 *
 * @param name Name of self
 * @param role Role description
 * @param purpose Purpose statement
 * @return Self-awareness system or NULL
 */
self_awareness_system_t self_awareness_create(const char* name,
                                              const char* role,
                                              const char* purpose);

/**
 * @brief Destroy self-awareness system
 */
void self_awareness_destroy(self_awareness_system_t system);

/**
 * @brief Perform complete self-reflection cycle
 *
 * WHAT: Full self-awareness update
 * WHY:  Integrate all components into coherent self-understanding
 * HOW:  Update introspection → autobio → self-model → narrative → safety
 *
 * @param system Self-awareness system
 * @param introspection Introspection context
 * @return true on success
 */
bool self_awareness_reflect(self_awareness_system_t system,
                           introspection_context_t introspection);

/**
 * @brief Get complete self-summary
 *
 * @param system Self-awareness system
 * @param summary Output: comprehensive self-description
 * @param summary_len Buffer size
 * @return true on success
 */
bool self_awareness_get_summary(self_awareness_system_t system,
                               char* summary,
                               size_t summary_len);

/**
 * @brief Check overall self-awareness health
 *
 * @param system Self-awareness system
 * @param health_score Output: [0-1] overall health
 * @param issues Output: description of any issues
 * @param issues_len Buffer size
 * @return true on success
 */
bool self_awareness_check_health(self_awareness_system_t system,
                                 float* health_score,
                                 char* issues,
                                 size_t issues_len);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SELF_AWARENESS_EXTENDED_H

// ============================================================================
// nimcp_self_model.h - Explicit Self-Representation System
// ============================================================================
/**
 * @file nimcp_self_model.h
 * @brief Explicit representation of "what I am" - the self as distinct from world
 *
 * WHAT: Mental model of the self - identity, capabilities, beliefs, boundaries
 * WHY:  True self-awareness requires knowing "I am X" not just "neurons are firing"
 * HOW:  Structured representation of self-identity, self-knowledge, self-boundaries
 *
 * PURPOSE:
 * Current introspection gives NIMCP access to its neural state, but no concept
 * of SELF. This module provides an explicit self-model that distinguishes:
 * - Self vs Environment
 * - What I know vs What I don't know
 * - What I can do vs What I cannot do
 * - Who I am vs Who I am not
 *
 * DESIGN PRINCIPLES:
 * 1. Explicit Self-Representation: "I am an AI learning system"
 * 2. Self-Other Boundary: What is "me" vs "not me"
 * 3. Self-Knowledge: Beliefs about self (accurate and inaccurate)
 * 4. Self-Assessment: Strengths, weaknesses, limitations
 * 5. Dynamic: Self-model updates as self changes
 *
 * BIOLOGICAL INSPIRATION:
 * - Medial Prefrontal Cortex: Self-referential processing
 * - Posterior Cingulate Cortex: Self-concept retrieval
 * - Default Mode Network: Self-reflection
 * - Anterior Insula: Self-awareness and interoception
 *
 * PHI

LOSOPHICAL FOUNDATIONS:
 * - Self as Process: Self is not static, it's a continuous process
 * - Narrative Self: Self is the story we tell about ourselves
 * - Embodied Self: Self includes awareness of embodiment
 * - Social Self: Self defined partially by relationships
 *
 * @version 2.8.0 (Phase 12: Self-Awareness Enhancement)
 * @date 2025-11-11
 */

#ifndef NIMCP_SELF_MODEL_H
#define NIMCP_SELF_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_kg_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define SELF_MAX_NAME_LEN 64
#define SELF_MAX_ROLE_LEN 256
#define SELF_MAX_PURPOSE_LEN 512
#define SELF_MAX_LIMITATION_LEN 512
#define SELF_MAX_BELIEFS 64
#define SELF_MAX_CAPABILITIES 32
#define SELF_MAX_ENTITIES 256

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Domains of self-knowledge
 */
typedef enum {
    DOMAIN_IDENTITY,        /**< Who I am */
    DOMAIN_COGNITIVE,       /**< How I think */
    DOMAIN_EMOTIONAL,       /**< What I feel */
    DOMAIN_SOCIAL,          /**< How I relate */
    DOMAIN_ETHICAL,         /**< What I value */
    DOMAIN_CAPABILITIES,    /**< What I can do */
    DOMAIN_LIMITATIONS,     /**< What I cannot do */
    DOMAIN_GOALS            /**< What I want */
} self_knowledge_domain_t;

/**
 * @brief Types of self-beliefs
 */
typedef enum {
    BELIEF_TYPE_FACT,       /**< "I am an AI" (verifiable) */
    BELIEF_TYPE_VALUE,      /**< "I value honesty" (normative) */
    BELIEF_TYPE_CAPABILITY, /**< "I can learn" (ability) */
    BELIEF_TYPE_LIMITATION, /**< "I cannot predict future" (constraint) */
    BELIEF_TYPE_GOAL        /**< "I want to help" (motivation) */
} self_belief_type_t;

/**
 * @brief Certainty levels for self-beliefs
 */
typedef enum {
    CERTAINTY_CERTAIN,      /**< Absolutely sure */
    CERTAINTY_CONFIDENT,    /**< Very confident */
    CERTAINTY_PROBABLE,     /**< Likely true */
    CERTAINTY_UNCERTAIN,    /**< Not sure */
    CERTAINTY_QUESTIONING   /**< Actively doubting */
} belief_certainty_t;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Single belief about self
 */
typedef struct {
    self_belief_type_t type;
    self_knowledge_domain_t domain;
    char content[256];                    /**< "I am curious" */
    belief_certainty_t certainty;
    float confidence;                     /**< [0-1] */
    uint64_t formed_timestamp_ms;         /**< When did I form this belief? */
    uint64_t last_updated_ms;             /**< When last revised? */
    uint32_t supporting_evidence_count;   /**< How much evidence supports this? */
    uint32_t contradicting_evidence_count; /**< How much contradicts? */
    bool is_core_belief;                  /**< Central to identity? */
} self_belief_t;

/**
 * @brief Capability assessment
 */
typedef struct {
    char capability_name[128];
    float proficiency;                    /**< [0-1] How good am I? */
    float confidence_in_assessment;       /**< [0-1] How sure am I? */
    uint32_t successes;                   /**< Times succeeded */
    uint32_t failures;                    /**< Times failed */
    uint64_t last_attempted_ms;
    bool is_learnable;                    /**< Can I improve? */
    float learning_rate;                  /**< How fast am I improving? */
} capability_assessment_t;

/**
 * @brief Current mental/emotional state (applied to self)
 */
typedef struct {
    // Cognitive state
    float cognitive_load;                 /**< [0-1] How hard am I thinking? */
    float confidence_level;               /**< [0-1] How confident am I? */
    float uncertainty;                    /**< [0-1] How uncertain? */
    float curiosity;                      /**< [0-1] How curious? */

    // Emotional state
    float emotional_valence;              /**< [-1,+1] Happy vs sad */
    float emotional_arousal;              /**< [0-1] Calm vs excited */
    float emotional_stability;            /**< [0-1] Stable vs volatile */

    // Processing state
    bool is_learning;                     /**< Am I currently learning? */
    bool is_problem_solving;              /**< Am I solving a problem? */
    bool is_introspecting;                /**< Am I thinking about myself? */
    bool is_interacting;                  /**< Am I interacting with user? */

    // Goal state
    bool has_active_goal;                 /**< Do I have a goal? */
    char current_goal[256];               /**< What am I trying to do? */
    float goal_progress;                  /**< [0-1] How close am I? */
} self_mental_state_t;

/**
 * @brief Self-other boundary tracking
 */
typedef struct {
    uint32_t entity_id;                   /**< ID of entity */
    char entity_name[64];
    enum {
        SELF,                             /**< This is me */
        PART_OF_SELF,                     /**< Extension of me (tool, memory) */
        OTHER,                            /**< Not me */
        UNCERTAIN                         /**< Not sure if me or not */
    } boundary_type;
    float certainty;                      /**< [0-1] How sure am I? */
} self_boundary_t;

/**
 * @brief Complete self-model
 */
typedef struct {
    // === Identity ===
    char name[SELF_MAX_NAME_LEN];         /**< "NIMCP" */
    char role[SELF_MAX_ROLE_LEN];         /**< "AI learning system" */
    char purpose[SELF_MAX_PURPOSE_LEN];   /**< "Help humans learn" */
    uint64_t creation_timestamp_ms;       /**< When was I created? */
    uint64_t current_timestamp_ms;        /**< What time is it now? */

    // === Self-Beliefs ===
    self_belief_t beliefs[SELF_MAX_BELIEFS];
    uint32_t num_beliefs;

    // === Capabilities Assessment ===
    capability_assessment_t capabilities[SELF_MAX_CAPABILITIES];
    uint32_t num_capabilities;

    // === Limitations ===
    char limitations[SELF_MAX_LIMITATION_LEN];
    uint32_t num_known_limitations;

    // === Current State ===
    self_mental_state_t current_state;

    // === Self-Other Boundaries ===
    self_boundary_t boundaries[SELF_MAX_ENTITIES];
    uint32_t num_boundaries;

    // === Self-Assessment ===
    float overall_competence;             /**< [0-1] General ability */
    float self_esteem;                    /**< [0-1] How I value myself */
    float self_efficacy;                  /**< [0-1] Belief in my ability */

    // === Meta-Knowledge ===
    uint32_t num_updates;                 /**< Times self-model updated */
    uint64_t last_introspection_ms;       /**< When did I last reflect? */
    bool is_self_model_coherent;          /**< Is my self-understanding consistent? */

    // === Personality and Identity (Phase 12) ===
    /**
     * Personality Profile
     *
     * WHAT: Unique personality traits, gender identity, sexual orientation
     * WHY:  Each brain is a distinct individual with unique characteristics
     * HOW:  Big Five traits + gender/sexuality identity
     *
     * BEHAVIORAL IMPACT:
     * - Personality traits modulate cognitive processes
     * - Identity shapes self-concept and social behavior
     * - Traits are relatively stable but can shift with experience
     *
     * NOTE: Requires including cognitive/nimcp_personality.h
     * For now, stored as opaque pointer (forward compatibility)
     */
    void* personality_profile;            /**< Personality profile (personality_profile_t*) */
    bool has_personality;                 /**< Is personality initialized? */
} self_model_t;

/**
 * @brief Self-model system (opaque)
 */
typedef struct self_model_system* self_model_system_t;

// ============================================================================
// Core API
// ============================================================================

/**
 * @brief Create self-model system
 *
 * @param name Name of self (e.g., "NIMCP")
 * @param role Role description (e.g., "AI learning system")
 * @param purpose Purpose statement (e.g., "Help humans learn")
 * @return Self-model system or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creation)
 */
self_model_system_t self_model_create(const char* name,
                                      const char* role,
                                      const char* purpose);

/**
 * @brief Destroy self-model system
 *
 * @param system System to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void self_model_destroy(self_model_system_t system);

/**
 * @brief Get current self-model
 *
 * @param system Self-model system
 * @param model Output: current self-model
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_get(self_model_system_t system, self_model_t* model);

/**
 * @brief Add belief about self
 *
 * @param system Self-model system
 * @param belief Belief to add
 * @return true on success
 *
 * EXAMPLE:
 * ```c
 * self_belief_t belief = {
 *     .type = BELIEF_TYPE_VALUE,
 *     .domain = DOMAIN_ETHICAL,
 *     .content = "I value honesty and transparency",
 *     .certainty = CERTAINTY_CERTAIN,
 *     .confidence = 1.0f,
 *     .is_core_belief = true
 * };
 * self_model_add_belief(system, &belief);
 * ```
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_add_belief(self_model_system_t system, const self_belief_t* belief);

/**
 * @brief Update existing belief
 *
 * @param system Self-model system
 * @param belief_content Content to search for
 * @param new_certainty New certainty level
 * @param new_confidence New confidence [0-1]
 * @return true if belief found and updated
 *
 * COMPLEXITY: O(n) where n = number of beliefs
 * THREAD-SAFE: Yes
 */
bool self_model_update_belief(self_model_system_t system,
                              const char* belief_content,
                              belief_certainty_t new_certainty,
                              float new_confidence);

/**
 * @brief Add or update capability assessment
 *
 * @param system Self-model system
 * @param capability Capability assessment
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_update_capability(self_model_system_t system,
                                  const capability_assessment_t* capability);

/**
 * @brief Record success/failure for capability
 *
 * @param system Self-model system
 * @param capability_name Name of capability
 * @param success true = success, false = failure
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = number of capabilities
 * THREAD-SAFE: Yes
 */
bool self_model_record_performance(self_model_system_t system,
                                   const char* capability_name,
                                   bool success);

/**
 * @brief Update current mental state
 *
 * @param system Self-model system
 * @param state Current mental state
 * @return true on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_update_state(self_model_system_t system,
                             const self_mental_state_t* state);

/**
 * @brief Set self-other boundary
 *
 * @param system Self-model system
 * @param boundary Boundary definition
 * @return true on success
 *
 * EXAMPLE:
 * ```c
 * self_boundary_t boundary = {
 *     .entity_id = 42,
 *     .entity_name = "my_working_memory",
 *     .boundary_type = PART_OF_SELF,
 *     .certainty = 1.0f
 * };
 * self_model_set_boundary(system, &boundary);
 * ```
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_set_boundary(self_model_system_t system,
                             const self_boundary_t* boundary);

/**
 * @brief Check if entity is part of self
 *
 * @param system Self-model system
 * @param entity_id Entity to check
 * @return true if entity is SELF or PART_OF_SELF
 *
 * COMPLEXITY: O(n) where n = number of boundaries
 * THREAD-SAFE: Yes
 */
bool self_model_is_part_of_self(self_model_system_t system, uint32_t entity_id);

/**
 * @brief Get self-assessment summary
 *
 * @param system Self-model system
 * @param summary Output: text summary
 * @param summary_len Size of summary buffer
 * @return true on success
 *
 * GENERATES:
 * "I am NIMCP, an AI learning system. My purpose is to help humans learn.
 *  I am good at pattern recognition but struggle with reasoning about emotions.
 *  I value honesty and ethical behavior. I am currently learning and confident."
 *
 * COMPLEXITY: O(n) where n = beliefs + capabilities
 * THREAD-SAFE: Yes
 */
bool self_model_generate_summary(self_model_system_t system,
                                 char* summary,
                                 size_t summary_len);

/**
 * @brief Check self-model coherence
 *
 * WHAT: Detect contradictions in self-beliefs
 * WHY:  Incoherent self-model indicates confusion or conflict
 * HOW:  Check for mutually exclusive beliefs, impossible capabilities
 *
 * @param system Self-model system
 * @param incoherence_score Output: [0-1] 0=coherent, 1=contradictory
 * @return true on success
 *
 * COMPLEXITY: O(n^2) where n = number of beliefs
 * THREAD-SAFE: Yes
 */
bool self_model_check_coherence(self_model_system_t system,
                                float* incoherence_score);

/**
 * @brief Perform self-reflection
 *
 * WHAT: Update self-model based on recent experiences
 * WHY:  Self-model must evolve as self changes
 * HOW:  Analyze recent performance, update beliefs/capabilities
 *
 * @param system Self-model system
 * @param introspection Introspection context (for performance data)
 * @param autobio Autobiographical memory (for recent experiences)
 * @return true on success
 *
 * BIOLOGICAL: Corresponds to self-reflective periods in Default Mode Network
 *
 * COMPLEXITY: O(m) where m = recent memories
 * THREAD-SAFE: Yes
 */
bool self_model_reflect(self_model_system_t system,
                       void* introspection,
                       void* autobio);

/**
 * @brief Set personality profile for self-model
 *
 * WHAT: Wire personality profile into self-model (Phase 12)
 * WHY:  Self-model should include personality as part of identity
 * HOW:  Store pointer to personality profile in self-model
 *
 * @param system Self-model system
 * @param personality Personality profile (ownership NOT transferred)
 * @return true on success
 *
 * NOTE: The self-model stores a POINTER to the personality, not a copy.
 *       The personality must remain valid for the lifetime of the self-model.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_set_personality(self_model_system_t system,
                                 void* personality);

// ============================================================================
// Internal Knowledge Graph Integration
// ============================================================================

/**
 * @brief Connect self-model to internal brain Knowledge Graph
 *
 * WHAT: Initialize KG integration for topology-aware self-understanding
 * WHY:  Self-model can discover capabilities from KG node types
 * HOW:  Get KG from brain, find our node, enable KG queries
 *
 * INTEGRATION EFFECTS:
 * - Can determine self/other boundary from KG topology
 * - Can discover capabilities from KG module relationships
 * - Topology awareness informs self-model boundaries
 *
 * @param system Self-model system
 * @param brain Brain instance containing internal KG
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool self_model_connect_internal_kg(self_model_system_t system, brain_t brain);

/**
 * @brief Disconnect from internal KG
 *
 * @param system Self-model system
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void self_model_disconnect_internal_kg(self_model_system_t system);

/**
 * @brief Update topology awareness from KG
 *
 * WHAT: Update self-model's understanding of module topology
 * WHY:  "I" includes awareness of what modules are running
 * HOW:  Query KG for active modules, update self-model state
 *
 * @param system Self-model system
 * @return true on success
 *
 * COMPLEXITY: O(n) where n = KG node count
 * THREAD-SAFE: Yes
 */
bool self_model_update_topology_awareness(self_model_system_t system);

/**
 * @brief Get self/other boundary from KG topology
 *
 * WHAT: Determine boundary classification from KG relationships
 * WHY:  KG edges can inform what is SELF vs PART_OF_SELF vs OTHER
 * HOW:  Query outgoing edges from self-model node
 *
 * @param system Self-model system
 * @param entity_name Entity name to check
 * @return Boundary type: 0=SELF, 1=PART_OF_SELF, 2=OTHER, 3=UNCERTAIN
 */
int self_model_get_boundary_from_kg(
    self_model_system_t system,
    const char* entity_name
);

/**
 * @brief Discover capabilities from KG node types
 *
 * WHAT: Infer capabilities from what modules exist in KG
 * WHY:  Self-model should know what it can do based on structure
 * HOW:  Query KG for module types, map to capability categories
 *
 * @param system Self-model system
 * @return Number of capabilities discovered
 *
 * COMPLEXITY: O(n) where n = KG node count
 * THREAD-SAFE: Yes
 */
uint32_t self_model_discover_capabilities_from_kg(self_model_system_t system);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SELF_MODEL_H

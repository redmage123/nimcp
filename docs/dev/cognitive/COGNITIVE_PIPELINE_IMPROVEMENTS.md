# NIMCP Cognitive Pipeline Improvements - Comprehensive Analysis

**Document Version:** 1.0
**Date:** 2025-11-11
**Status:** Recommendations for Enhancement

---

## Executive Summary

NIMCP currently has a sophisticated cognitive architecture with 20+ cognitive modules including working memory, executive functions, predictive processing, theory of mind, ethics, and more. This document identifies **12 major areas for improvement** to create a more unified, efficient, and biologically-realistic cognitive pipeline.

**Current Strengths:**
- ✅ Multimodal sensory integration (visual, audio, speech)
- ✅ Working memory with capacity limits (Miller's 7±2)
- ✅ Executive functions (task switching, planning, inhibition)
- ✅ Predictive processing (Free Energy Principle)
- ✅ Theory of mind and empathy
- ✅ Ethics and wellbeing monitoring
- ✅ Salience evaluation (novelty, surprise, urgency)
- ✅ Curiosity and exploration
- ✅ Meta-learning and introspection
- ✅ Memory consolidation and sleep-wake cycles

**Key Gaps Identified:**
1. ❌ No unified **Global Workspace** for information integration
2. ❌ Limited **attention mechanisms** (no top-down goal-directed attention)
3. ❌ No explicit **cognitive control loop** (monitoring, error detection, adjustment)
4. ❌ Missing **episodic memory** (autobiographical experiences with context)
5. ❌ No **goal/motivation system** (beyond curiosity)
6. ❌ Limited **metacognition** (confidence, error awareness)
7. ❌ No explicit **decision-making architecture**
8. ❌ Missing **language generation** (only input processing)
9. ❌ No **cognitive load management** system
10. ❌ No **context management** (context stack, schema activation)
11. ❌ Missing **timing and synchronization** mechanisms
12. ❌ No **causal reasoning** module

---

## Part J: Cognitive Pipeline Enhancements

### Category J1: Global Workspace Architecture ⭐⭐⭐ CRITICAL

**Priority:** HIGHEST (P0) - Foundational for unified cognition
**Effort:** 10-12 weeks
**Impact:** Enables conscious access, information integration, flexible control

#### Current Problem

NIMCP has many specialized cognitive modules (working memory, executive, theory of mind, ethics, etc.) but **no central integration hub** where information from these modules is:
- Combined into unified representations
- Made available to all other modules (broadcast)
- Selected for conscious awareness
- Used for flexible, adaptive behavior

This creates "siloed" processing where modules don't easily share information.

#### Proposed Solution: Global Workspace Theory (Baars, 1988; Dehaene & Changeux, 2011)

**Concept:** A "stage" or "blackboard" where information from any module can be posted, broadcast to all modules, and compete for attention.

**Architecture:**

```
┌─────────────────────────────────────────────────────────────┐
│                    GLOBAL WORKSPACE                          │
│  ┌────────────────────────────────────────────────────┐     │
│  │  Broadcast Buffer (Working Memory Integration)     │     │
│  │  - Current attended content                        │     │
│  │  - Available to ALL cognitive modules              │     │
│  │  - Winner-take-all competition for access          │     │
│  └────────────────────────────────────────────────────┘     │
│         ↑ Compete for access    ↓ Read broadcast           │
└─────────┼────────────────────────┼────────────────────────┘
          │                        │
    ┌─────┴────────┬───────────────┴──────┬──────────────┐
    │              │                      │              │
┌───┴───┐  ┌──────┴──────┐  ┌───────────┴──┐  ┌────────┴─────┐
│Percept│  │ Working Mem │  │  Executive   │  │   Theory of  │
│Module │  │   System    │  │   Control    │  │     Mind     │
└───────┘  └─────────────┘  └──────────────┘  └──────────────┘
```

**Key Components:**

```c
/**
 * @file nimcp_global_workspace.h
 * @brief Global Workspace Architecture for unified cognitive access
 *
 * THEORY: Bernard Baars' Global Workspace Theory + Dehaene's conscious access
 * WHY: Enable flexible information sharing between all cognitive modules
 * HOW: Broadcast buffer + competition for access + winner-take-all
 */

typedef struct {
    // Broadcast buffer (limited capacity ~1 "conscious chunk")
    float* current_content;           /**< Current broadcast content */
    uint32_t content_dim;             /**< Content dimensionality */

    // Source information
    cognitive_module_t source_module; /**< Which module won competition */
    float source_strength;            /**< Strength of winning signal */

    // Subscribers (modules listening to broadcast)
    cognitive_module_t* subscribers[32]; /**< Up to 32 listening modules */
    uint32_t num_subscribers;

    // Competition state
    float* competitor_signals[32];    /**< Competing signals */
    float competitor_strengths[32];   /**< Signal strengths */
    uint32_t num_competitors;

    // Access history
    cognitive_module_t recent_winners[10]; /**< Recent broadcast winners */
    uint64_t broadcast_timestamps[10];

    // Ignition threshold (for conscious access)
    float ignition_threshold;         /**< Threshold for workspace access */
    bool ignition_achieved;           /**< Was threshold crossed? */

} global_workspace_t;

typedef enum {
    MODULE_PERCEPTION,
    MODULE_WORKING_MEMORY,
    MODULE_EXECUTIVE,
    MODULE_THEORY_OF_MIND,
    MODULE_ETHICS,
    MODULE_EPISODIC_MEMORY,
    MODULE_SEMANTIC_MEMORY,
    MODULE_LANGUAGE,
    MODULE_EMOTION,
    MODULE_SALIENCE,
    MODULE_MOTOR,
    // ... other modules
} cognitive_module_t;

/**
 * @brief Compete for global workspace access
 *
 * WHAT: Modules submit candidate content for broadcast
 * WHY: Limited capacity requires competition (attention bottleneck)
 * HOW: Winner-take-all based on signal strength + current priorities
 *
 * @param workspace Global workspace handle
 * @param module Which module is submitting
 * @param content Content to broadcast (if wins)
 * @param strength Signal strength (0-1, salience/urgency)
 * @return true if won competition and content was broadcast
 *
 * ALGORITHM:
 * 1. Add to competitor pool
 * 2. Evaluate all competitor strengths
 * 3. Select winner (highest strength above threshold)
 * 4. Broadcast winner's content to all subscribers
 * 5. Update broadcast history
 */
bool global_workspace_compete(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength
);

/**
 * @brief Subscribe to global broadcasts
 *
 * @param workspace Global workspace handle
 * @param module Which module wants to listen
 * @return true if subscription successful
 */
bool global_workspace_subscribe(
    global_workspace_t* workspace,
    cognitive_module_t module
);

/**
 * @brief Read current broadcast content
 *
 * @param workspace Global workspace handle
 * @param content Output buffer (caller allocates)
 * @param max_dim Maximum dimensions to read
 * @param actual_dim Actual dimensions read
 * @param source Which module produced this broadcast
 * @return true if content available
 */
bool global_workspace_read_broadcast(
    global_workspace_t* workspace,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    cognitive_module_t* source
);
```

**Integration with Existing Modules:**

```c
// Example: Working memory submits to workspace when item is highly salient
if (item_salience > 0.8) {
    bool won = global_workspace_compete(
        brain->global_workspace,
        MODULE_WORKING_MEMORY,
        item_content,
        item_dim,
        item_salience
    );
    if (won) {
        // Content is now globally available - other modules can access it
    }
}

// Example: Executive controller reads workspace to inform decisions
float workspace_content[256];
uint32_t content_dim;
cognitive_module_t source;
if (global_workspace_read_broadcast(brain->global_workspace,
                                     workspace_content, 256,
                                     &content_dim, &source)) {
    // Use broadcast content for decision-making
    executive_plan_task(exec, workspace_content, content_dim);
}
```

**Benefits:**
- ✅ Unified information access across all modules
- ✅ Natural bottleneck (attention) emerges from competition
- ✅ Conscious access = winning global workspace competition
- ✅ Flexible control flow (not hard-coded pipelines)
- ✅ Explains cognitive phenomena (attentional blink, change blindness)

**Implementation Effort:** 10-12 weeks
- Week 1-2: Design architecture and interfaces
- Week 3-5: Implement workspace and competition mechanism
- Week 6-8: Integrate with existing modules (working memory, executive, etc.)
- Week 9-10: Test and optimize winner-take-all dynamics
- Week 11-12: Documentation and validation

---

### Category J2: Enhanced Attention Mechanisms ⭐⭐⭐ HIGH

**Priority:** HIGH (P1)
**Effort:** 8-10 weeks
**Impact:** Better focus, resource allocation, and cognitive control

#### Current State

NIMCP has:
- ✅ Salience evaluation (novelty, surprise, urgency)
- ✅ Attention weights in multimodal integration
- ❌ But missing: Goal-directed attention, inhibition of return, sustained vs transient attention

#### Proposed Enhancements

**J2.1: Top-Down (Goal-Directed) Attention**

```c
/**
 * @brief Top-down attention biasing
 *
 * WHAT: Goals modulate sensory processing ("look for red objects")
 * WHY: Enable task-relevant filtering
 * HOW: Bias activations in early sensory layers based on task goals
 *
 * BIOLOGICAL: Prefrontal → V4/IT feedback connections
 */
typedef struct {
    float* attention_template;  /**< What to look for (e.g., "red", "faces") */
    uint32_t template_dim;
    float bias_strength;        /**< How strongly to bias (0-1) */
    float decay_rate;           /**< Attention fades without refresh */
} top_down_attention_t;

/**
 * @brief Apply top-down attention bias to sensory processing
 *
 * ALGORITHM:
 * 1. Compare sensory features to attention template
 * 2. Compute similarity (dot product or cosine)
 * 3. Boost matching features, suppress non-matching
 * 4. Result: Task-relevant inputs are amplified
 */
void attention_apply_top_down_bias(
    top_down_attention_t* attention,
    float* sensory_features,
    uint32_t num_features,
    float* biased_features  // Output
);
```

**J2.2: Inhibition of Return**

```c
/**
 * @brief Inhibition of return (IOR)
 *
 * WHAT: Recently attended locations are suppressed
 * WHY: Encourages novelty-seeking, prevents perseveration
 * HOW: Tag recent attention targets, temporarily reduce their salience
 *
 * BIOLOGICAL: Superior colliculus, parietal cortex
 * TIMESCALE: ~300ms suppression after attention shift
 */
typedef struct {
    uint32_t recent_targets[8];     /**< Recently attended items (ring buffer) */
    uint64_t target_timestamps[8];  /**< When each was attended */
    uint32_t buffer_index;          /**< Ring buffer index */
    uint32_t ior_duration_ms;       /**< How long to suppress (default: 300ms) */
    float suppression_factor;       /**< How much to reduce salience (default: 0.5) */
} inhibition_of_return_t;

/**
 * @brief Apply inhibition of return to salience scores
 *
 * Recently attended items get reduced salience (encourages exploration)
 */
void attention_apply_ior(
    inhibition_of_return_t* ior,
    uint32_t* item_ids,
    float* salience_scores,
    uint32_t num_items,
    uint64_t current_time_ms
);
```

**J2.3: Sustained vs Transient Attention**

```c
/**
 * @brief Dual attention modes
 *
 * SUSTAINED: Vigilance, maintained focus (minutes)
 * TRANSIENT: Orienting, brief shifts (milliseconds)
 *
 * BIOLOGICAL:
 * - Sustained: Right hemisphere, noradrenergic system
 * - Transient: Superior colliculus, parietal cortex
 */
typedef enum {
    ATTENTION_TRANSIENT,  /**< Brief orienting (<500ms) */
    ATTENTION_SUSTAINED   /**< Prolonged vigilance (seconds-minutes) */
} attention_mode_t;

typedef struct {
    attention_mode_t current_mode;
    uint64_t mode_start_time;
    float vigilance_level;           /**< Decreases over time (fatigue) */
    uint32_t transient_shifts_count; /**< Number of rapid shifts */
} attention_state_t;

/**
 * @brief Update attention mode and vigilance
 *
 * VIGILANCE DECAY: Sustained attention decreases over time
 * SHIFT COST: Frequent transient shifts are cognitively expensive
 */
void attention_update_state(
    attention_state_t* state,
    uint64_t current_time_ms,
    bool shift_occurred
);
```

**J2.4: Attentional Blink**

```c
/**
 * @brief Attentional blink phenomenon
 *
 * WHAT: After detecting target T1, miss target T2 if within ~200-500ms
 * WHY: Consolidation into working memory creates temporary bottleneck
 * HOW: Lock workspace access for brief period after target detection
 *
 * EXPERIMENT: T1...T2 with varying lag → miss T2 at lag 2-5 (200-500ms)
 * MECHANISM: Working memory gate is closed during T1 consolidation
 */
typedef struct {
    bool blink_active;              /**< Currently in attentional blink? */
    uint64_t blink_start_time;      /**< When blink started */
    uint32_t blink_duration_ms;     /**< How long blink lasts (200-500ms) */
} attentional_blink_t;

/**
 * @brief Check if new target can be detected (or missed due to blink)
 *
 * Returns false if target occurs during attentional blink period
 */
bool attention_can_detect_target(
    attentional_blink_t* blink,
    uint64_t current_time_ms
);
```

**Benefits:**
- ✅ Task-relevant information is prioritized
- ✅ Novelty-seeking through inhibition of return
- ✅ Realistic attention limitations (vigilance decay, attentional blink)
- ✅ Dual attention systems (transient + sustained)

**Implementation Effort:** 8-10 weeks

---

### Category J3: Cognitive Control Loop ⭐⭐ MEDIUM-HIGH

**Priority:** MEDIUM-HIGH (P2)
**Effort:** 6-8 weeks
**Impact:** Error detection, performance monitoring, adaptive control

#### Proposed Architecture

```c
/**
 * @file nimcp_cognitive_control.h
 * @brief Cognitive control loop (monitoring, conflict, adjustment)
 *
 * THEORY: Anterior Cingulate Cortex (ACC) conflict monitoring
 * WHY: Detect errors, conflicts, and adjust processing strategy
 * HOW: Monitor prediction errors, response conflicts, performance
 *
 * COMPONENTS:
 * 1. Error Detection: Compare expected vs actual outcomes
 * 2. Conflict Detection: Detect competing responses (Stroop effect)
 * 3. Performance Monitoring: Track accuracy, speed, confidence
 * 4. Control Adjustment: Modulate processing (slow down, recruit resources)
 */

typedef struct {
    // Error monitoring
    float error_signal;              /**< Magnitude of current error */
    float cumulative_error;          /**< Recent error accumulation */
    uint32_t error_count_recent;     /**< Errors in last N trials */

    // Conflict monitoring
    float conflict_level;            /**< Current response conflict */
    bool high_conflict_detected;     /**< Above threshold? */

    // Performance metrics
    float accuracy_recent;           /**< Recent accuracy (0-1) */
    float response_time_avg;         /**< Average response time */
    float confidence_avg;            /**< Average confidence */

    // Control signals (outputs)
    float control_intensity;         /**< How much control to exert */
    bool slow_down_processing;       /**< Increase deliberation */
    bool recruit_resources;          /**< Engage additional modules */
    bool escalate_to_metacognition;  /**< Need strategic rethinking */

} cognitive_control_state_t;

/**
 * @brief Detect response conflict (e.g., Stroop task)
 *
 * WHAT: Multiple response options activated simultaneously
 * WHY: Indicates need for increased control
 * HOW: Compare activation levels of competing responses
 *
 * EXAMPLE: Stroop task
 * - Word "RED" in GREEN ink
 * - Competing responses: "red" (word) vs "green" (ink color)
 * - High conflict → ACC activation → increased control
 *
 * @param response_activations Activation levels for each response option
 * @param num_responses Number of response options
 * @return Conflict level (0-1)
 *
 * ALGORITHM:
 * Conflict = entropy of response distribution (high when multiple strong responses)
 */
float cognitive_control_detect_conflict(
    const float* response_activations,
    uint32_t num_responses
);

/**
 * @brief Monitor prediction error
 *
 * WHAT: Difference between predicted and actual outcome
 * WHY: Large errors indicate model failure, need for learning
 * HOW: Compare prediction to outcome, accumulate recent errors
 */
void cognitive_control_update_error(
    cognitive_control_state_t* control,
    float predicted_value,
    float actual_value,
    uint64_t current_time
);

/**
 * @brief Adjust control intensity based on errors and conflicts
 *
 * WHAT: Modulate processing based on performance
 * WHY: Adaptive control - slow down when making errors
 * HOW: High error/conflict → increase control_intensity
 *
 * EFFECTS:
 * - Slow down processing (allow more time for deliberation)
 * - Recruit additional resources (working memory, attention)
 * - Increase inhibition (suppress prepotent responses)
 * - Engage metacognition (strategic rethinking)
 */
void cognitive_control_adjust(
    cognitive_control_state_t* control
);
```

**Integration with Executive Functions:**

```c
// In executive controller, check cognitive control signals
if (control->slow_down_processing) {
    // Add deliberation time before committing to decision
    executive_set_deliberation_time(exec, 500); // ms
}

if (control->recruit_resources) {
    // Engage working memory, attention, metacognition
    working_memory_increase_capacity(wm);
    attention_increase_focus(attn);
}

if (control->escalate_to_metacognition) {
    // Strategy isn't working - need to rethink approach
    metacognition_reassess_strategy(meta);
}
```

**Benefits:**
- ✅ Adaptive control (slow down when making errors)
- ✅ Conflict detection (Stroop-like phenomena)
- ✅ Performance monitoring (track accuracy, confidence)
- ✅ Error-driven learning (update models when errors occur)

**Implementation Effort:** 6-8 weeks

---

### Category J4: Episodic Memory System ⭐⭐ MEDIUM-HIGH

**Priority:** MEDIUM-HIGH (P2)
**Effort:** 10-12 weeks
**Impact:** Autobiographical memory, personal experiences, temporal context

#### Current Gap

NIMCP has:
- ✅ Semantic memory (knowledge module - facts, concepts)
- ✅ Working memory (temporary active representations)
- ❌ Missing: **Episodic memory** (personal experiences with "what, where, when" context)

**Difference:**
- **Semantic:** "Paris is the capital of France" (timeless fact)
- **Episodic:** "I visited Paris in summer 2019, saw the Eiffel Tower with my family" (personal experience with context)

#### Proposed Episodic Memory Architecture

```c
/**
 * @file nimcp_episodic_memory.h
 * @brief Episodic memory system (autobiographical experiences)
 *
 * THEORY: Tulving's episodic memory (1972), hippocampal memory systems
 * WHY: Store personal experiences with spatiotemporal context
 * HOW: Bind content + context, support pattern completion and mental time travel
 *
 * BIOLOGICAL: Hippocampus, medial temporal lobe
 * PROPERTIES:
 * - What (content): Sensory features, events, actions
 * - Where (space): Location, scene context
 * - When (time): Temporal context, sequence
 * - Who (social): People involved, social context
 * - How (emotional): Emotional state, arousal, valence
 */

typedef struct {
    uint32_t episode_id;           /**< Unique episode identifier */

    // Content (WHAT)
    float* content_features;       /**< What happened (sensory, conceptual) */
    uint32_t content_dim;

    // Spatial context (WHERE)
    float* spatial_context;        /**< Location, scene, environment */
    uint32_t spatial_dim;

    // Temporal context (WHEN)
    uint64_t timestamp_absolute;   /**< Absolute time (Unix ms) */
    float* temporal_context;       /**< Relative temporal context */
    uint32_t temporal_dim;

    // Social context (WHO)
    uint32_t* people_present;      /**< IDs of people involved */
    uint32_t num_people;

    // Emotional context (HOW)
    emotion_state_t emotional_state; /**< Emotional state during episode */
    float arousal;                 /**< Arousal level */
    float valence;                 /**< Positive/negative valence */

    // Retrieval information
    float salience;                /**< Memorability (high = more likely retrieved) */
    uint32_t retrieval_count;      /**< How many times retrieved (reconsolidation) */
    uint64_t last_retrieval_time;  /**< When last retrieved */

    // Linkages
    uint32_t* associated_episodes; /**< Linked episodes (narrative chains) */
    uint32_t num_associations;

} episodic_memory_t;

/**
 * @brief Encode new episodic memory
 *
 * WHAT: Store experience with full context binding
 * WHY: Create retrievable autobiographical memory
 * HOW: Bind content + spatiotemporal + emotional context
 *
 * HIPPOCAMPAL ENCODING:
 * - Pattern separation: Orthogonalize similar experiences
 * - Context binding: Link content to context
 * - Consolidation: Gradual transfer to neocortex (overnight)
 *
 * @param episodic_system Episodic memory system
 * @param content What happened
 * @param spatial_context Where it happened
 * @param emotional_context How you felt
 * @param salience How memorable (high salience = better encoding)
 * @return Episode ID for future retrieval
 */
uint32_t episodic_memory_encode(
    episodic_memory_system_t* system,
    const float* content,
    uint32_t content_dim,
    const float* spatial_context,
    uint32_t spatial_dim,
    const emotion_state_t* emotional_context,
    float salience
);

/**
 * @brief Retrieve episodic memory via content-based cue
 *
 * WHAT: Recall episode from partial cue
 * WHY: "Remembering" - re-experience past events
 * HOW: Pattern completion from hippocampal traces
 *
 * RETRIEVAL PROCESS:
 * 1. Compare cue to stored episodes (similarity)
 * 2. Select best match above threshold
 * 3. Pattern completion: Reconstruct full episode from cue
 * 4. Update retrieval count (reconsolidation)
 *
 * @param episodic_system Episodic memory system
 * @param cue Partial memory cue (content or context)
 * @param cue_dim Cue dimensionality
 * @param retrieved_episode Output: reconstructed episode
 * @return true if episode retrieved (above threshold)
 */
bool episodic_memory_retrieve(
    episodic_memory_system_t* system,
    const float* cue,
    uint32_t cue_dim,
    episodic_memory_t* retrieved_episode
);

/**
 * @brief Retrieve episodes from specific time period
 *
 * WHAT: "What did I do yesterday?"
 * WHY: Temporal organization of autobiographical memory
 * HOW: Filter by timestamp range
 */
uint32_t episodic_memory_retrieve_time_range(
    episodic_memory_system_t* system,
    uint64_t start_time,
    uint64_t end_time,
    episodic_memory_t* episodes,    // Output array
    uint32_t max_episodes
);

/**
 * @brief Retrieve emotionally charged episodes
 *
 * WHAT: "Flashbulb memories" - highly emotional events
 * WHY: Emotional arousal enhances encoding and retrieval
 * HOW: Filter by emotional arousal + valence
 *
 * EXAMPLES:
 * - High arousal + negative valence: Traumatic events
 * - High arousal + positive valence: Peak experiences
 */
uint32_t episodic_memory_retrieve_emotional(
    episodic_memory_system_t* system,
    float min_arousal,
    float valence_min,
    float valence_max,
    episodic_memory_t* episodes,
    uint32_t max_episodes
);

/**
 * @brief Consolidate episodic memories (sleep-dependent)
 *
 * WHAT: Transfer from hippocampus to neocortex during sleep
 * WHY: Long-term storage, integration with semantic memory
 * HOW: Replay episodes, strengthen important ones, prune unimportant
 *
 * INTEGRATION: Call during sleep_wake_cycle (slow-wave sleep)
 */
void episodic_memory_consolidate(
    episodic_memory_system_t* system,
    consolidation_t* consolidation_module
);
```

**Benefits:**
- ✅ Autobiographical memory (personal experiences)
- ✅ Mental time travel (remember past, imagine future)
- ✅ Context-rich retrieval ("where was I when...")
- ✅ Flashbulb memories (emotional events)
- ✅ Narrative construction (linked episode chains)

**Implementation Effort:** 10-12 weeks

---

### Category J5: Goal and Motivation System ⭐⭐ MEDIUM

**Priority:** MEDIUM (P2)
**Effort:** 8-10 weeks
**Impact:** Goal-directed behavior, motivation, value-based learning

#### Current Gap

NIMCP has:
- ✅ Curiosity (novelty-seeking motivation)
- ❌ Missing: General goal system, motivation signals, drive reduction, reward prediction error

#### Proposed Architecture

```c
/**
 * @file nimcp_goal_motivation.h
 * @brief Goal hierarchy and motivation system
 *
 * THEORY: Hierarchical goal representations (Botvinick & Plaut, 2004)
 * WHY: Enable purposeful, goal-directed behavior
 * HOW: Goal stack, motivation signals, reward prediction error
 *
 * BIOLOGICAL: Prefrontal cortex (goals), striatum (motivation), dopamine (RPE)
 */

typedef struct {
    uint32_t goal_id;              /**< Unique goal identifier */
    char goal_description[256];    /**< Human-readable goal */

    // Goal hierarchy
    uint32_t parent_goal_id;       /**< Parent (superordinate) goal */
    uint32_t* subgoal_ids;         /**< Children (subordinate) goals */
    uint32_t num_subgoals;
    uint32_t goal_level;           /**< Depth in hierarchy (0=top) */

    // Goal state
    goal_status_t status;          /**< Pending, active, achieved, abandoned */
    float progress;                /**< How close to achievement (0-1) */
    float expected_value;          /**< Expected reward for achieving goal */

    // Motivation
    float motivation_level;        /**< Current drive to pursue goal (0-1) */
    float urgency;                 /**< Time pressure */

    // Context
    float* goal_context;           /**< Context in which goal is relevant */
    uint32_t context_dim;

} goal_t;

typedef enum {
    GOAL_PENDING,      /**< Not yet started */
    GOAL_ACTIVE,       /**< Currently pursuing */
    GOAL_ACHIEVED,     /**< Successfully completed */
    GOAL_ABANDONED,    /**< Gave up */
    GOAL_FAILED        /**< Attempted but failed */
} goal_status_t;

/**
 * @brief Reward Prediction Error (RPE)
 *
 * WHAT: Difference between expected and actual reward
 * WHY: TD learning, dopamine signal, motivation update
 * HOW: RPE = actual_reward - expected_reward
 *
 * RPE > 0: Better than expected (positive surprise) → increase motivation
 * RPE < 0: Worse than expected (disappointment) → decrease motivation
 * RPE = 0: As expected (learning converged)
 *
 * BIOLOGICAL: Dopamine neurons in VTA/SNc encode RPE
 */
typedef struct {
    float rpe;                     /**< Reward prediction error */
    float expected_reward;         /**< What was predicted */
    float actual_reward;           /**< What actually received */
    uint64_t timestamp;
} reward_prediction_error_t;

/**
 * @brief Create new goal
 */
uint32_t goal_system_create_goal(
    goal_system_t* system,
    const char* description,
    uint32_t parent_goal_id,
    float expected_value
);

/**
 * @brief Decompose goal into subgoals
 *
 * WHAT: Hierarchical planning - break complex goal into steps
 * WHY: Make abstract goals actionable
 * HOW: Recursive decomposition until atomic actions
 */
uint32_t* goal_system_decompose(
    goal_system_t* system,
    uint32_t goal_id,
    uint32_t* num_subgoals  // Output
);

/**
 * @brief Select next goal to pursue
 *
 * WHAT: Goal selection based on motivation, urgency, value
 * WHY: Prioritize among competing goals
 * HOW: Weighted combination of factors
 *
 * FACTORS:
 * - Expected value (high value goals preferred)
 * - Motivation level (current drive)
 * - Urgency (deadlines)
 * - Progress (continue vs switch)
 * - Context fit (is context appropriate for this goal?)
 */
uint32_t goal_system_select_goal(
    goal_system_t* system,
    const float* current_context,
    uint32_t context_dim
);

/**
 * @brief Update goal progress and motivation
 *
 * WHAT: Track progress, adjust motivation based on RPE
 * WHY: Adaptive goal pursuit (persist vs abandon)
 * HOW: Progress from actions, motivation from RPE
 */
void goal_system_update(
    goal_system_t* system,
    uint32_t goal_id,
    float progress_increment,
    const reward_prediction_error_t* rpe
);
```

**Benefits:**
- ✅ Hierarchical goal decomposition (abstract → concrete)
- ✅ Motivation dynamics (RPE-based learning)
- ✅ Goal selection and prioritization
- ✅ Adaptive goal pursuit (persist vs abandon based on feedback)

**Implementation Effort:** 8-10 weeks

---

### Category J6: Enhanced Metacognition ⭐ MEDIUM

**Priority:** MEDIUM (P3)
**Effort:** 6-8 weeks
**Impact:** Confidence monitoring, error awareness, strategic control

#### Current State

NIMCP has:
- ✅ Introspection module (self-awareness)
- ✅ Meta-learning (learning to learn)
- ❌ Missing: Confidence monitoring, error awareness, feeling of knowing

#### Proposed Enhancements

```c
/**
 * @file nimcp_metacognition_extended.h
 * @brief Extended metacognitive monitoring and control
 *
 * METACOGNITIVE MONITORING:
 * - Confidence: How certain am I?
 * - Error awareness: Did I make a mistake?
 * - Feeling of knowing: Do I know the answer (even if can't recall)?
 * - Tip-of-tongue: Almost remember, but can't quite access it
 *
 * METACOGNITIVE CONTROL:
 * - Strategy selection: Choose problem-solving approach
 * - Resource allocation: How much effort to invest?
 * - Termination: When to stop trying?
 */

typedef struct {
    // Confidence monitoring
    float confidence;              /**< Certainty in current decision (0-1) */
    float confidence_calibration;  /**< How well-calibrated is confidence? */

    // Error awareness
    bool error_detected;           /**< Did I make a mistake? */
    float error_likelihood;        /**< Probability of error (0-1) */

    // Knowledge states
    feeling_of_knowing_t fok;      /**< Do I know the answer? */
    bool tip_of_tongue;            /**< Almost have it... */

    // Control signals
    strategy_t selected_strategy;  /**< Problem-solving strategy */
    float effort_level;            /**< How hard to try (0-1) */
    bool should_terminate;         /**< Give up on this problem? */

} metacognitive_state_t;

/**
 * @brief Confidence monitoring
 *
 * WHAT: Assess certainty in decision or memory retrieval
 * WHY: Know when to trust your cognition vs seek verification
 * HOW: Calibrated confidence from response consistency, fluency, conflict
 *
 * ALGORITHM:
 * - High consistency across trials → high confidence
 * - High processing fluency → high confidence
 * - Low response conflict → high confidence
 * - Match to past calibration → well-calibrated confidence
 */
float metacognition_estimate_confidence(
    metacognitive_state_t* meta,
    const float* response_activations,
    uint32_t num_responses,
    float processing_fluency,
    float response_conflict
);

/**
 * @brief Error detection (post-response monitoring)
 *
 * WHAT: Realize you made a mistake after responding
 * WHY: Enable error correction, adaptive control
 * HOW: Compare response to internal models, detect conflict/uncertainty
 *
 * EXAMPLE: Stroop task
 * - Say "red" (wrong) for word "BLUE" in red ink
 * - Immediately detect error (high conflict signal)
 * - Trigger "oops" response, attempt correction
 */
bool metacognition_detect_error(
    metacognitive_state_t* meta,
    const float* expected_outcome,
    const float* actual_outcome,
    uint32_t dim
);

/**
 * @brief Feeling of knowing (FOK)
 *
 * WHAT: Sense that you know answer even if can't currently recall
 * WHY: Guides memory search ("keep trying" vs "give up")
 * HOW: Partial activation of target memory trace
 *
 * EXAMPLE:
 * Q: "Who was the 23rd US president?"
 * Response: "I know this... it's on the tip of my tongue... starts with H?"
 * FOK: High (feel like you know it)
 * Actual retrieval: Failed (can't access it yet)
 */
float metacognition_feeling_of_knowing(
    metacognitive_state_t* meta,
    const float* retrieval_cue,
    uint32_t cue_dim,
    episodic_memory_system_t* episodic,
    semantic_memory_system_t* semantic
);

/**
 * @brief Select problem-solving strategy
 *
 * WHAT: Choose between strategies (e.g., algorithmic vs heuristic)
 * WHY: Adaptive strategy use (hard problems need algorithms)
 * HOW: Metacognitive assessment of problem difficulty + time pressure
 */
strategy_t metacognition_select_strategy(
    metacognitive_state_t* meta,
    float problem_difficulty,
    float time_pressure
);

typedef enum {
    STRATEGY_ALGORITHMIC,    /**< Slow, accurate, systematic */
    STRATEGY_HEURISTIC,      /**< Fast, approximate, intuitive */
    STRATEGY_RANDOM_SEARCH,  /**< Exploratory */
    STRATEGY_ASK_FOR_HELP    /**< Escalate to external resource */
} strategy_t;
```

**Benefits:**
- ✅ Know when to trust your decisions (confidence)
- ✅ Detect and correct errors in real-time
- ✅ Adaptive strategy selection (algorithmic vs heuristic)
- ✅ Efficient resource allocation (know when to give up)

**Implementation Effort:** 6-8 weeks

---

## Summary of Recommendations

### Highest Priority (P0-P1): Core Architecture

1. **J1: Global Workspace Architecture** (10-12 weeks) ⭐⭐⭐
   - CRITICAL for unified cognition
   - Enables flexible information sharing
   - Foundation for consciousness

2. **J2: Enhanced Attention Mechanisms** (8-10 weeks) ⭐⭐⭐
   - Top-down goal-directed attention
   - Inhibition of return
   - Sustained vs transient attention
   - Attentional blink

3. **J3: Cognitive Control Loop** (6-8 weeks) ⭐⭐
   - Error and conflict detection
   - Performance monitoring
   - Adaptive control adjustment

### Medium Priority (P2): Rich Cognition

4. **J4: Episodic Memory System** (10-12 weeks) ⭐⭐
   - Autobiographical experiences
   - Spatiotemporal context binding
   - Mental time travel

5. **J5: Goal and Motivation System** (8-10 weeks) ⭐⭐
   - Hierarchical goal representations
   - Reward prediction error
   - Motivation dynamics

6. **J6: Enhanced Metacognition** (6-8 weeks) ⭐
   - Confidence monitoring
   - Error awareness
   - Strategy selection

### Additional Improvements (Brief Descriptions)

**J7: Explicit Decision-Making Architecture** (8 weeks)
- Option generation, evaluation, choice
- Value-based decision making
- Regret and counterfactual reasoning

**J8: Language Generation Pipeline** (10 weeks)
- Semantic → syntactic → phonological
- Already have speech input, add output

**J9: Cognitive Load Management** (6 weeks)
- Dynamic load assessment
- Graceful degradation under load
- Priority-based processing

**J10: Context Management System** (6 weeks)
- Context stack (hierarchical)
- Schema activation and switching
- Context-dependent processing

**J11: Timing and Synchronization** (6 weeks)
- Time perception and estimation
- Temporal coordination between modules
- Event ordering

**J12: Causal Reasoning Module** (8 weeks)
- Causal models and inference
- Interventional reasoning
- Counterfactual thinking

---

## Implementation Roadmap

### Phase 1: Core Architecture (Months 1-4)
**Focus: Foundational integration improvements**
- J1: Global Workspace (10-12 weeks) - START FIRST
- J2: Enhanced Attention (8-10 weeks) - Parallel with J1

### Phase 2: Control and Memory (Months 5-8)
**Focus: Cognitive control and rich memory**
- J3: Cognitive Control Loop (6-8 weeks)
- J4: Episodic Memory (10-12 weeks) - Parallel with J3

### Phase 3: Goals and Metacognition (Months 9-11)
**Focus: Motivation and self-awareness**
- J5: Goal/Motivation System (8-10 weeks)
- J6: Enhanced Metacognition (6-8 weeks) - Parallel with J5

### Phase 4: Advanced Features (Months 12-15)
**Focus: Decision-making and additional modules**
- J7, J8, J9, J10, J11, J12 (based on priority and resources)

---

## Integration Strategy

**Key Principle:** Incremental integration with existing modules

Each improvement should:
1. **Extend** existing modules (not replace)
2. **Integrate** through clearly defined interfaces
3. **Maintain** backward compatibility
4. **Test** in isolation before full integration
5. **Document** integration points

**Example Integration:**
```c
// brain_struct extended with new modules
struct brain_struct {
    // Existing modules
    working_memory_t* working_memory;
    executive_controller_t* executive;
    theory_of_mind_t* theory_of_mind;
    ethics_engine_t* ethics;
    salience_evaluator_t* salience;
    // ... existing modules ...

    // NEW Phase J modules
    global_workspace_t* global_workspace;           // J1
    attention_system_t* attention_system;           // J2
    cognitive_control_state_t* cognitive_control;   // J3
    episodic_memory_system_t* episodic_memory;      // J4
    goal_system_t* goal_system;                     // J5
    metacognitive_state_t* metacognition;           // J6
};
```

---

## Expected Benefits

**Architectural:**
- ✅ Unified information integration (Global Workspace)
- ✅ Flexible control flow (not hard-coded pipelines)
- ✅ Natural cognitive bottlenecks (attention, workspace capacity)

**Cognitive Capabilities:**
- ✅ Goal-directed behavior (not just reactive)
- ✅ Autobiographical memory (personal experiences)
- ✅ Error detection and correction (adaptive control)
- ✅ Confidence monitoring (know when to trust yourself)
- ✅ Motivation dynamics (persist vs abandon goals)

**Emergent Phenomena:**
- ✅ Conscious access (global workspace competition)
- ✅ Attentional limitations (vigilance decay, blink)
- ✅ Strategic thinking (metacognitive strategy selection)
- ✅ Mental time travel (episodic memory retrieval)

---

## Total Effort Estimate

**Sequential Implementation:** ~80-100 weeks (20-25 months)
**Parallel (3 teams):** ~35-40 weeks (9-10 months)

**Resources Required:**
- 3-4 senior cognitive scientists/AI researchers
- 2-3 software engineers (C/C++)
- 1 systems integration engineer
- 1 QA/testing engineer

---

**Document Status:** Ready for Review and Discussion
**Next Steps:** Prioritize based on project goals, resources, and timeline

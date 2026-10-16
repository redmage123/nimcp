# Society of Thought Integration Plan

**Version**: 1.0.0
**Created**: 2026-01-26
**Status**: Proposed
**Reference Paper**: Kim et al., "Reasoning Models Generate Societies of Thought" (arXiv:2601.10825)

---

## Executive Summary

This plan implements the core findings from Kim et al. (2026), which demonstrates that advanced reasoning models achieve superior performance through internal multi-agent dialogue with diverse, adversarial perspectives rather than linear chain-of-thought computation. NIMCP already contains most of the required building blocks (global workspace, game theory, Theory of Mind, RCOG delegation, personality, salience, FEP). This plan adds the **orchestration layer** that explicitly treats reasoning as a social process.

The paper's single highest-impact finding: amplifying a "surprise/realization" feature nearly doubled reasoning accuracy (27.1% to 54.8%). Phase 1 targets this mechanism directly.

---

## Table of Contents

1. [Goals and Objectives](#1-goals-and-objectives)
2. [Architecture Overview](#2-architecture-overview)
3. [Phase 1: Surprise Amplifier](#3-phase-1-surprise-amplifier)
4. [Phase 2: Cognitive Agent Profiles](#4-phase-2-cognitive-agent-profiles)
5. [Phase 3: Adversarial Perspective Protocol](#5-phase-3-adversarial-perspective-protocol)
6. [Phase 4: Social Scaling Controller](#6-phase-4-social-scaling-controller)
7. [Phase 5: Society of Thought Engine](#7-phase-5-society-of-thought-engine)
8. [Bio-Async Integration](#8-bio-async-integration)
9. [File Structure](#9-file-structure)
10. [Dependencies](#10-dependencies)
11. [Testing Strategy](#11-testing-strategy)
12. [Risk Mitigation](#12-risk-mitigation)

---

## 1. Goals and Objectives

### Primary Goals

| Goal | Description | Success Metric |
|------|-------------|----------------|
| **G1** | Implement surprise amplification | Measurable increase in reasoning accuracy on benchmark tasks |
| **G2** | Enable personality-diverse reasoning | RCOG delegation produces diverse conclusions with variance in Big Five traits |
| **G3** | Enforce adversarial evaluation | Every reasoning conclusion is challenged by at least one contrarian agent |
| **G4** | Dynamic social scaling | Agent count scales with problem difficulty (metacognitive estimate) |
| **G5** | Unified orchestration | Society of Thought engine coordinates all components through global workspace |

### Design Principles

1. **Reuse Over Rebuild**: Wire existing modules (GW, game theory, ToM, personality, RCOG) into new orchestration patterns
2. **Backward Compatibility**: Non-society reasoning paths continue to work unchanged
3. **Graceful Degradation**: On constrained platforms, reduce agent count but preserve surprise amplification
4. **Biological Plausibility**: All mechanisms map to known neuroscience (Minsky's Society of Mind, Baars' GWT, predictive processing)
5. **Measurability**: Every component exposes metrics for diversity, conflict rate, and resolution quality

---

## 2. Architecture Overview

```
+================================================================+
|                  SOCIETY OF THOUGHT ENGINE                       |
|                    (Top-Level Orchestrator)                      |
+================================================================+
         |              |              |              |
         v              v              v              v
+----------------+ +-----------+ +-----------+ +------------------+
| SOCIAL SCALING | | SURPRISE  | | ADVERSAR. | | DIVERSITY        |
| CONTROLLER     | | AMPLIFIER | | PROTOCOL  | | METRICS          |
| (metacognition,| | (salience,| | (ToM,     | | (variance,       |
|  executive)    | |  FEP, attn| |  game thy)| |  conflict rate)  |
+-------+--------+ +-----+-----+ +-----+-----+ +--------+---------+
        |                |              |                 |
        +--------+-------+------+-------+---------+------+
                 |              |                  |
                 v              v                  v
        +--------+------+ +----+----+ +-----------+----------+
        | RCOG DELEG.   | | GLOBAL  | | GAME THEORY          |
        | POOL          | | WORK-   | | BARGAINING           |
        | (personality- | | SPACE   | | (conflict resolution)|
        |  diverse      | | (comp., | |                      |
        |  delegates)   | |  bcast) | |                      |
        +--------+------+ +----+----+ +-----------+----------+
                 |              |                  |
    +============+==============+==================+============+
    |                    EXISTING NIMCP MODULES                  |
    | Salience | FEP | Curiosity | Attention | Introspection   |
    | Personality | ToM | Executive | Ethics | Emotion          |
    +============================================================+
```

### Data Flow

1. **Problem arrives** at RCOG orchestrator
2. **Social Scaling Controller** assesses difficulty (via metacognition uncertainty estimate)
3. **Cognitive Agent Profiles** are generated with diverse personality traits
4. **RCOG delegation pool** spawns N diverse delegate workers
5. Each delegate reasons with its personality-biased perspective
6. **Surprise Amplifier** monitors for prediction errors and inter-agent contradictions
7. When surprise threshold is crossed: attention boost, perspective re-evaluation
8. **Adversarial Perspective Protocol** ensures minority viewpoints are explored
9. Conclusions compete in **Global Workspace** via game-theoretic auction
10. Winner is broadcast; **Diversity Metrics** track the reasoning quality

---

## 3. Phase 1: Surprise Amplifier

**Priority**: CRITICAL (highest impact, smallest scope)
**Dependencies**: Existing salience, FEP, curiosity, attention modules

### Rationale

The paper's mechanistic interpretability found a single sparse autoencoder feature (Layer 15, Feature 30939) that acts as a "surprise/realization" discourse marker. Amplifying this feature nearly doubled accuracy (27.1% -> 54.8%). This maps directly to NIMCP's prediction error -> salience -> attention pathway.

### 3.1 Surprise Amplifier Module

#### Files to Create

```
include/cognitive/salience/nimcp_surprise_amplifier.h
src/cognitive/salience/nimcp_surprise_amplifier.c
```

#### Core Data Structures

```c
// nimcp_surprise_amplifier.h

/**
 * Surprise source types - what triggered the surprise signal
 */
typedef enum {
    SURPRISE_SOURCE_FEP_PREDICTION_ERROR,     /**< FEP prediction violated */
    SURPRISE_SOURCE_INTER_AGENT_CONFLICT,     /**< Two agents contradicted each other */
    SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED,   /**< Evidence invalidated current best guess */
    SURPRISE_SOURCE_NOVELTY_DETECTION,        /**< Never-before-seen pattern */
    SURPRISE_SOURCE_EXPECTATION_VIOLATION,    /**< Bayesian surprise */
    SURPRISE_SOURCE_COUNT
} surprise_source_t;

/**
 * Surprise event - emitted when surprise threshold is exceeded
 */
typedef struct {
    surprise_source_t source;        /**< What caused the surprise */
    float magnitude;                 /**< Surprise intensity [0, 1] */
    float prediction_error;          /**< Raw prediction error that triggered it */
    float attention_boost;           /**< Recommended attention increase */
    float curiosity_boost;           /**< Recommended curiosity drive increase */
    uint64_t timestamp_ns;           /**< When the surprise occurred */
    uint32_t source_module_id;       /**< Bio-async module that produced the signal */
    uint32_t conflicting_module_id;  /**< If inter-agent conflict, the other module */
} surprise_event_t;

/**
 * Surprise amplifier configuration
 */
typedef struct {
    float base_threshold;           /**< Minimum prediction error to trigger [0.3] */
    float amplification_gain;       /**< Multiplier for surprise signal [2.0] */
    float attention_boost_factor;   /**< How much to boost attention [1.5] */
    float curiosity_boost_factor;   /**< How much to boost curiosity [1.2] */
    float decay_rate;               /**< Surprise signal decay per tick [0.05] */
    float conflict_weight;          /**< Weight for inter-agent conflicts [1.5] */
    float novelty_weight;           /**< Weight for novelty-based surprise [1.0] */
    uint32_t refractory_period_ms;  /**< Minimum time between surprise events [100] */
    uint32_t max_concurrent;        /**< Max simultaneous surprise signals [4] */
    bool enable_gw_broadcast;       /**< Broadcast surprise to global workspace [true] */
    bool enable_executive_interrupt; /**< Interrupt executive on high surprise [true] */
} surprise_amplifier_config_t;

/**
 * Surprise amplifier system
 */
typedef struct surprise_amplifier surprise_amplifier_t;
```

#### Core API

```c
// Lifecycle
surprise_amplifier_config_t surprise_amplifier_default_config(void);
surprise_amplifier_t* surprise_amplifier_create(const surprise_amplifier_config_t* config);
void surprise_amplifier_destroy(surprise_amplifier_t* amp);

// Connection to existing systems
int surprise_amplifier_connect_fep(surprise_amplifier_t* amp, fep_system_t* fep);
int surprise_amplifier_connect_salience(surprise_amplifier_t* amp, salience_evaluator_t* salience);
int surprise_amplifier_connect_attention(surprise_amplifier_t* amp, attention_system_t* attention);
int surprise_amplifier_connect_curiosity(surprise_amplifier_t* amp, curiosity_system_t* curiosity);
int surprise_amplifier_connect_gw(surprise_amplifier_t* amp, global_workspace_t* gw);
int surprise_amplifier_connect_executive(surprise_amplifier_t* amp, executive_system_t* executive);

// Input signals
int surprise_amplifier_on_prediction_error(surprise_amplifier_t* amp,
                                           float prediction_error,
                                           uint32_t source_module);
int surprise_amplifier_on_agent_conflict(surprise_amplifier_t* amp,
                                         uint32_t agent_a, float conclusion_a,
                                         uint32_t agent_b, float conclusion_b);
int surprise_amplifier_on_hypothesis_invalidated(surprise_amplifier_t* amp,
                                                  float prior_confidence,
                                                  float posterior_confidence);

// Tick / update
int surprise_amplifier_update(surprise_amplifier_t* amp, float dt_seconds);

// Query
float surprise_amplifier_get_current_level(const surprise_amplifier_t* amp);
bool surprise_amplifier_is_in_refractory(const surprise_amplifier_t* amp);
surprise_event_t surprise_amplifier_get_last_event(const surprise_amplifier_t* amp);

// Stats
typedef struct {
    uint64_t total_surprises;
    uint64_t fep_triggered;
    uint64_t conflict_triggered;
    uint64_t hypothesis_triggered;
    uint64_t novelty_triggered;
    float avg_magnitude;
    float max_magnitude;
    uint64_t gw_broadcasts;
    uint64_t executive_interrupts;
    uint64_t refractory_suppressed;
} surprise_amplifier_stats_t;

surprise_amplifier_stats_t surprise_amplifier_get_stats(const surprise_amplifier_t* amp);

// Bio-async
int surprise_amplifier_connect_bio_async(surprise_amplifier_t* amp, bio_router_t* router);
int surprise_amplifier_disconnect_bio_async(surprise_amplifier_t* amp);
```

### 3.2 Integration Bridges

#### Files to Create

```
include/cognitive/salience/nimcp_surprise_fep_bridge.h
src/cognitive/salience/nimcp_surprise_fep_bridge.c

include/cognitive/salience/nimcp_surprise_gw_bridge.h
src/cognitive/salience/nimcp_surprise_gw_bridge.c

include/cognitive/salience/nimcp_surprise_attention_bridge.h
src/cognitive/salience/nimcp_surprise_attention_bridge.c
```

### 3.3 Mechanism

```
FEP prediction error > threshold
        │
        ▼
Surprise Amplifier fires
        │
        ├──► Attention system: boost allocation to conflict zone
        ├──► Curiosity system: increase epistemic drive
        ├──► Global Workspace: broadcast surprise event to all modules
        ├──► Executive: interrupt current plan, force re-evaluation
        └──► RCOG: if in society mode, spawn additional adversarial agent
```

### 3.4 Biological Basis

- **Norepinephrine release**: Locus coeruleus fires on unexpected events, boosting attention globally
- **Anterior cingulate cortex**: Conflict monitoring triggers re-evaluation
- **Dopaminergic prediction errors**: Reward prediction error signals in VTA/substantia nigra
- **P300 ERP component**: Electrophysiological marker of surprise/context updating

---

## 4. Phase 2: Cognitive Agent Profiles

**Priority**: HIGH
**Dependencies**: Phase 1, existing personality and RCOG modules

### Rationale

The paper shows reasoning models exhibit high variance in Big Five traits across internal personas, especially Openness and Neuroticism. NIMCP's personality module already implements OCEAN traits. This phase creates dynamic personality profiles for RCOG delegates.

### 4.1 Cognitive Agent Profile Module

#### Files to Create

```
include/cognitive/society/nimcp_cognitive_agent_profile.h
src/cognitive/society/nimcp_cognitive_agent_profile.c
```

#### Core Data Structures

```c
// nimcp_cognitive_agent_profile.h

/**
 * Reasoning bias - predisposition in how an agent approaches conclusions
 */
typedef enum {
    REASONING_BIAS_NEUTRAL,          /**< No predisposition */
    REASONING_BIAS_OPTIMISTIC,       /**< Favors positive outcomes */
    REASONING_BIAS_PESSIMISTIC,      /**< Favors negative/risk outcomes */
    REASONING_BIAS_CONTRARIAN,       /**< Actively challenges majority view */
    REASONING_BIAS_CONSERVATIVE,     /**< Favors existing beliefs */
    REASONING_BIAS_EXPLORATORY,      /**< Favors novel hypotheses */
    REASONING_BIAS_COUNT
} reasoning_bias_t;

/**
 * Domain emphasis - which cognitive systems this agent weights higher
 */
typedef enum {
    DOMAIN_EMPHASIS_BALANCED,        /**< Equal weight to all domains */
    DOMAIN_EMPHASIS_ANALYTICAL,      /**< Parietal/logical emphasis */
    DOMAIN_EMPHASIS_EMOTIONAL,       /**< Emotional/empathetic emphasis */
    DOMAIN_EMPHASIS_SOCIAL,          /**< ToM/social reasoning emphasis */
    DOMAIN_EMPHASIS_ETHICAL,         /**< Ethics/value emphasis */
    DOMAIN_EMPHASIS_CREATIVE,        /**< Imagination/novelty emphasis */
    DOMAIN_EMPHASIS_PRAGMATIC,       /**< Executive/practical emphasis */
    DOMAIN_EMPHASIS_COUNT
} domain_emphasis_t;

/**
 * Complete cognitive agent profile for a reasoning perspective
 */
typedef struct {
    uint32_t agent_id;               /**< Unique identifier within society */
    big_five_traits_t personality;   /**< Big Five personality traits */
    reasoning_bias_t bias;           /**< Reasoning predisposition */
    domain_emphasis_t emphasis;      /**< Which cognitive domain to weight */
    float confidence_threshold;      /**< Min confidence to propose conclusion [0.5] */
    float stubbornness;              /**< Resistance to changing position [0-1] */
    float openness_to_evidence;      /**< Willingness to update on evidence [0-1] */
    float risk_tolerance;            /**< Tolerance for uncertain conclusions [0-1] */
    char label[64];                  /**< Human-readable agent label */
} cognitive_agent_profile_t;

/**
 * Profile generation strategy
 */
typedef enum {
    PROFILE_GEN_RANDOM,              /**< Random diverse profiles */
    PROFILE_GEN_MAXIMALLY_DIVERSE,   /**< Maximize trait variance (paper's finding) */
    PROFILE_GEN_ADVERSARIAL_PAIRS,   /**< Generate opposing pairs */
    PROFILE_GEN_CUSTOM               /**< User-specified profiles */
} profile_gen_strategy_t;

/**
 * Profile generator configuration
 */
typedef struct {
    profile_gen_strategy_t strategy;
    uint32_t num_agents;             /**< Number of profiles to generate */
    float min_trait_variance;        /**< Minimum variance across agents [0.15] */
    float contrarian_ratio;          /**< Fraction of contrarian agents [0.2] */
    bool ensure_adversarial;         /**< Guarantee at least one contrarian [true] */
} profile_generator_config_t;
```

#### Core API

```c
// Profile generation
profile_generator_config_t profile_generator_default_config(void);
int cognitive_agent_profile_generate(const profile_generator_config_t* config,
                                     cognitive_agent_profile_t* profiles_out,
                                     uint32_t max_profiles,
                                     uint32_t* num_generated);

// Diversity measurement
float cognitive_agent_profile_diversity_score(const cognitive_agent_profile_t* profiles,
                                              uint32_t num_profiles);

// Profile manipulation
cognitive_agent_profile_t cognitive_agent_profile_create_contrarian(
    const cognitive_agent_profile_t* target);

cognitive_agent_profile_t cognitive_agent_profile_create_devil_advocate(
    const cognitive_agent_profile_t* majority_profile);
```

### 4.2 RCOG Delegation Pool Integration

The existing `rcog_delegation_pool` spawns capability-tiered workers. This phase adds personality-diverse delegation:

#### Files to Modify

```
include/cognitive/recursive/nimcp_rcog_delegation_pool.h  (add profile field to worker config)
src/cognitive/recursive/nimcp_rcog_delegation_pool.c      (use profile during task execution)
```

#### Integration Points

- `rcog_worker_config_t` gains a `cognitive_agent_profile_t* profile` field
- When `rcog_delegation_pool_dispatch()` distributes subtasks, each delegate receives a unique profile
- Profile modulates: curiosity drive, attention allocation, risk assessment, confidence thresholds
- Workers with `REASONING_BIAS_CONTRARIAN` actively seek disconfirming evidence

---

## 5. Phase 3: Adversarial Perspective Protocol

**Priority**: HIGH
**Dependencies**: Phase 1, Phase 2, existing ToM and game theory modules

### Rationale

The paper identifies "Conflict of Perspectives" as the fundamental unit of reasoning, not individual tokens. Every conclusion must be challenged by informed skepticism.

### 5.1 Adversarial Perspective Module

#### Files to Create

```
include/cognitive/society/nimcp_adversarial_perspective.h
src/cognitive/society/nimcp_adversarial_perspective.c
```

#### Core Data Structures

```c
// nimcp_adversarial_perspective.h

/**
 * A reasoning perspective - one agent's conclusion about a problem
 */
typedef struct {
    uint32_t agent_id;                  /**< Which agent produced this */
    cognitive_agent_profile_t profile;  /**< Agent's personality/bias */
    float conclusion[256];              /**< Conclusion vector (workspace dim) */
    float confidence;                   /**< Agent's confidence in conclusion [0-1] */
    float evidence_strength;            /**< Strength of supporting evidence [0-1] */
    float explanatory_power;            /**< How much the conclusion explains [0-1] */
    uint32_t reasoning_steps;           /**< Number of steps to reach conclusion */
    uint64_t timestamp_ns;              /**< When conclusion was reached */
} reasoning_perspective_t;

/**
 * A conflict between two perspectives
 */
typedef struct {
    reasoning_perspective_t perspective_a;
    reasoning_perspective_t perspective_b;
    float conflict_magnitude;           /**< How much they disagree [0-1] */
    float resolution_confidence;        /**< Confidence in resolution [0-1] */
    uint32_t resolution_winner;         /**< Agent ID of winner (0 = unresolved) */
    bool surprise_triggered;            /**< Whether this conflict triggered surprise */
} perspective_conflict_t;

/**
 * Adversarial protocol configuration
 */
typedef struct {
    float conflict_threshold;           /**< Min disagreement to count as conflict [0.3] */
    uint32_t max_rounds;                /**< Max adversarial rounds before forced resolution [5] */
    float min_contrarian_strength;      /**< Min strength of contrarian argument [0.2] */
    bool use_tom_for_counterarguments;  /**< Use ToM to generate informed challenges [true] */
    bool require_evidence;              /**< Contrarian must provide evidence [true] */
    float convergence_threshold;        /**< Agreement level to stop debate [0.85] */
} adversarial_protocol_config_t;

/**
 * Adversarial perspective system
 */
typedef struct adversarial_perspective adversarial_perspective_t;
```

#### Core API

```c
// Lifecycle
adversarial_protocol_config_t adversarial_protocol_default_config(void);
adversarial_perspective_t* adversarial_perspective_create(
    const adversarial_protocol_config_t* config);
void adversarial_perspective_destroy(adversarial_perspective_t* adv);

// Connect to subsystems
int adversarial_perspective_connect_tom(adversarial_perspective_t* adv,
                                        theory_of_mind_system_t* tom);
int adversarial_perspective_connect_game_theory(adversarial_perspective_t* adv,
                                                 gt_system_t* gt);
int adversarial_perspective_connect_gw(adversarial_perspective_t* adv,
                                       global_workspace_t* gw);
int adversarial_perspective_connect_surprise(adversarial_perspective_t* adv,
                                             surprise_amplifier_t* surprise);

// Submit perspectives for adversarial evaluation
int adversarial_perspective_submit(adversarial_perspective_t* adv,
                                   const reasoning_perspective_t* perspective);

// Generate counterargument using ToM
int adversarial_perspective_generate_challenge(adversarial_perspective_t* adv,
                                               const reasoning_perspective_t* target,
                                               reasoning_perspective_t* challenge_out);

// Run adversarial debate round
int adversarial_perspective_debate_round(adversarial_perspective_t* adv,
                                         perspective_conflict_t* conflicts_out,
                                         uint32_t max_conflicts,
                                         uint32_t* num_conflicts);

// Resolve conflicts via game-theoretic bargaining
int adversarial_perspective_resolve(adversarial_perspective_t* adv,
                                    const perspective_conflict_t* conflict,
                                    reasoning_perspective_t* winner_out);

// Get final consensus
int adversarial_perspective_get_consensus(const adversarial_perspective_t* adv,
                                          reasoning_perspective_t* consensus_out,
                                          float* consensus_confidence);
```

### 5.2 Resolution Mechanism

```
Agent A proposes conclusion X (confidence: 0.8)
        │
        ▼
ToM generates informed counter-perspective Y
(models what a skeptic with domain expertise would challenge)
        │
        ▼
Conflict detected: magnitude = |X - Y| / max(|X|, |Y|)
        │
        ├── magnitude < threshold ──► No conflict, accept X
        │
        └── magnitude >= threshold ──► Adversarial debate
                │
                ▼
        Game-theoretic bargaining (Nash equilibrium)
        Payoff = f(evidence_strength, explanatory_power, confidence)
                │
                ├── Convergence reached ──► Accept winner
                │
                └── Max rounds exceeded ──►
                    ├── If surprise amplifier fired ──► Spawn new agent with fresh perspective
                    └── If not ──► Accept highest evidence_strength perspective
```

---

## 6. Phase 4: Social Scaling Controller

**Priority**: MEDIUM
**Dependencies**: Phases 1-3, existing metacognition and executive modules

### Rationale

Not all problems need a full society. Easy problems waste resources with too many agents. Hard problems need maximum diversity. The social scaling controller dynamically adjusts.

### 6.1 Social Scaling Module

#### Files to Create

```
include/cognitive/society/nimcp_social_scaling.h
src/cognitive/society/nimcp_social_scaling.c
```

#### Core Data Structures

```c
// nimcp_social_scaling.h

/**
 * Problem difficulty estimate (from metacognition)
 */
typedef enum {
    DIFFICULTY_TRIVIAL,    /**< 1-2 agents, low diversity */
    DIFFICULTY_EASY,       /**< 2-3 agents, mild diversity */
    DIFFICULTY_MODERATE,   /**< 3-4 agents, moderate diversity */
    DIFFICULTY_HARD,       /**< 4-6 agents, high diversity */
    DIFFICULTY_EXTREME     /**< 6-8 agents, maximum diversity + strong adversarial */
} problem_difficulty_t;

/**
 * Difficulty assessment based on metacognitive signals
 */
typedef struct {
    problem_difficulty_t level;
    float uncertainty;           /**< Metacognitive uncertainty [0-1] */
    float contradiction_rate;    /**< Rate of conflicting evidence [0-1] */
    float prediction_error_avg;  /**< Average recent prediction error [0-1] */
    float reasoning_depth;       /**< Estimated depth required [0-1] */
    uint32_t recommended_agents; /**< Recommended number of agents */
    float recommended_diversity; /**< Recommended trait variance [0-1] */
} difficulty_assessment_t;

/**
 * Social scaling configuration
 */
typedef struct {
    uint32_t min_agents;                   /**< Minimum agents even for trivial [2] */
    uint32_t max_agents;                   /**< Maximum agents for extreme [8] */
    float uncertainty_scale_threshold;     /**< Uncertainty above this adds agents [0.5] */
    float diminishing_returns_threshold;   /**< Stop adding when marginal value drops [0.1] */
    uint32_t scale_up_cooldown_ms;         /**< Min time between scale-up events [500] */
    bool auto_scale;                       /**< Automatically adjust agent count [true] */
} social_scaling_config_t;

/**
 * Social scaling controller
 */
typedef struct social_scaling_controller social_scaling_controller_t;
```

#### Core API

```c
// Lifecycle
social_scaling_config_t social_scaling_default_config(void);
social_scaling_controller_t* social_scaling_create(const social_scaling_config_t* config);
void social_scaling_destroy(social_scaling_controller_t* ctrl);

// Connect to metacognition
int social_scaling_connect_metacognition(social_scaling_controller_t* ctrl,
                                         metacognition_system_t* meta);
int social_scaling_connect_executive(social_scaling_controller_t* ctrl,
                                     executive_system_t* exec);
int social_scaling_connect_introspection(social_scaling_controller_t* ctrl,
                                         introspection_system_t* intro);

// Difficulty assessment
difficulty_assessment_t social_scaling_assess_difficulty(
    social_scaling_controller_t* ctrl,
    float current_uncertainty,
    float current_prediction_error,
    uint32_t current_conflicts);

// Dynamic scaling
int social_scaling_recommend_scale(social_scaling_controller_t* ctrl,
                                   const difficulty_assessment_t* assessment,
                                   uint32_t* agents_to_add,
                                   uint32_t* agents_to_remove);

// Convergence detection
bool social_scaling_should_converge(const social_scaling_controller_t* ctrl,
                                    float consensus_confidence,
                                    uint32_t rounds_elapsed);

// Marginal diversity value
float social_scaling_marginal_diversity_value(const social_scaling_controller_t* ctrl,
                                              const cognitive_agent_profile_t* current_profiles,
                                              uint32_t num_current,
                                              const cognitive_agent_profile_t* candidate);
```

---

## 7. Phase 5: Society of Thought Engine

**Priority**: MEDIUM
**Dependencies**: All previous phases

### Rationale

The top-level orchestrator that combines all components into a unified reasoning system. This is the "conductor" of the internal society.

### 7.1 Society of Thought Engine Module

#### Files to Create

```
include/cognitive/society/nimcp_society_of_thought.h
src/cognitive/society/nimcp_society_of_thought.c
```

#### Core Data Structures

```c
// nimcp_society_of_thought.h

/**
 * Society reasoning session - one complete deliberation
 */
typedef struct {
    uint64_t session_id;                           /**< Unique session ID */
    uint32_t num_agents;                           /**< Active agents in this session */
    cognitive_agent_profile_t agents[8];            /**< Agent profiles */
    reasoning_perspective_t perspectives[8];        /**< Each agent's conclusion */
    perspective_conflict_t conflicts[28];           /**< Pairwise conflicts (8C2 = 28 max) */
    uint32_t num_conflicts;                        /**< Active conflicts */
    uint32_t debate_rounds;                        /**< Rounds of adversarial debate */
    uint32_t surprise_events;                      /**< Surprise signals during session */
    reasoning_perspective_t consensus;             /**< Final consensus conclusion */
    float consensus_confidence;                    /**< Confidence in consensus */
    float diversity_score;                         /**< How diverse were the perspectives */
    uint64_t start_time_ns;                        /**< Session start */
    uint64_t end_time_ns;                          /**< Session end */
} society_session_t;

/**
 * Society of Thought configuration
 */
typedef struct {
    surprise_amplifier_config_t surprise_config;
    profile_generator_config_t profile_config;
    adversarial_protocol_config_t adversarial_config;
    social_scaling_config_t scaling_config;
    bool enable_gw_broadcast;                /**< Broadcast final consensus to GW [true] */
    bool enable_immune_modulation;           /**< Allow immune system to modulate society [true] */
    bool enable_metrics_tracking;            /**< Track per-session metrics [true] */
    uint32_t max_concurrent_sessions;        /**< Max parallel deliberations [4] */
} society_of_thought_config_t;

/**
 * Society of Thought engine
 */
typedef struct society_of_thought_engine society_of_thought_engine_t;
```

#### Core API

```c
// Lifecycle
society_of_thought_config_t society_of_thought_default_config(void);
society_of_thought_engine_t* society_of_thought_create(
    const society_of_thought_config_t* config);
void society_of_thought_destroy(society_of_thought_engine_t* engine);

// Connect subsystems
int society_of_thought_connect_rcog(society_of_thought_engine_t* engine,
                                     rcog_orchestrator_t* rcog);
int society_of_thought_connect_gw(society_of_thought_engine_t* engine,
                                   global_workspace_t* gw);
int society_of_thought_connect_fep(society_of_thought_engine_t* engine,
                                    fep_system_t* fep);
int society_of_thought_connect_personality(society_of_thought_engine_t* engine,
                                           personality_system_t* personality);
int society_of_thought_connect_tom(society_of_thought_engine_t* engine,
                                    theory_of_mind_system_t* tom);
int society_of_thought_connect_game_theory(society_of_thought_engine_t* engine,
                                            gt_system_t* gt);
int society_of_thought_connect_immune(society_of_thought_engine_t* engine,
                                       brain_immune_system_t* immune);

// Deliberation
int society_of_thought_deliberate(society_of_thought_engine_t* engine,
                                   const float* problem_representation,
                                   uint32_t problem_dim,
                                   reasoning_perspective_t* result_out,
                                   float* confidence_out);

// Session management
society_session_t* society_of_thought_get_current_session(
    const society_of_thought_engine_t* engine);
int society_of_thought_abort_session(society_of_thought_engine_t* engine,
                                     uint64_t session_id);

// Metrics
typedef struct {
    uint64_t total_sessions;
    uint64_t total_conflicts;
    uint64_t total_surprises;
    float avg_agents_per_session;
    float avg_diversity_score;
    float avg_debate_rounds;
    float avg_consensus_confidence;
    float conflict_resolution_rate;
    float surprise_improvement_rate;   /**< How much surprise improved accuracy */
} society_of_thought_stats_t;

society_of_thought_stats_t society_of_thought_get_stats(
    const society_of_thought_engine_t* engine);

// Bio-async
int society_of_thought_connect_bio_async(society_of_thought_engine_t* engine,
                                          bio_router_t* router);
int society_of_thought_disconnect_bio_async(society_of_thought_engine_t* engine);
```

### 7.2 Brain Integration

#### Files to Modify

```
include/core/brain/nimcp_brain.h           (add society_of_thought_engine_t* field)
src/core/brain/nimcp_brain.c               (init/destroy society engine)
src/core/brain/factory/init/               (add nimcp_brain_init_society_of_thought.c)
```

---

## 8. Bio-Async Integration

### Module IDs (Range: 0x1E00-0x1E0F)

```c
#define BIO_MODULE_SOCIETY_BASE                 0x1E00
#define BIO_MODULE_SOCIETY_ENGINE               (BIO_MODULE_SOCIETY_BASE + 0)
#define BIO_MODULE_SURPRISE_AMPLIFIER           (BIO_MODULE_SOCIETY_BASE + 1)
#define BIO_MODULE_ADVERSARIAL_PROTOCOL         (BIO_MODULE_SOCIETY_BASE + 2)
#define BIO_MODULE_SOCIAL_SCALING               (BIO_MODULE_SOCIETY_BASE + 3)
#define BIO_MODULE_AGENT_PROFILE_GEN            (BIO_MODULE_SOCIETY_BASE + 4)
#define BIO_MODULE_DIVERSITY_METRICS            (BIO_MODULE_SOCIETY_BASE + 5)
```

### Message Types (Range: 0x6E80-0x6E9F)

```c
#define BIO_MSG_SOCIETY_SPAWN_AGENT             0x6E80  /**< Spawn new reasoning agent */
#define BIO_MSG_SOCIETY_AGENT_CONCLUSION        0x6E81  /**< Agent reached a conclusion */
#define BIO_MSG_SOCIETY_CONFLICT_DETECTED       0x6E82  /**< Two agents disagree */
#define BIO_MSG_SOCIETY_SURPRISE_SIGNAL         0x6E83  /**< Surprise threshold exceeded */
#define BIO_MSG_SOCIETY_DEBATE_ROUND            0x6E84  /**< Adversarial debate round */
#define BIO_MSG_SOCIETY_RESOLUTION              0x6E85  /**< Conflict resolved */
#define BIO_MSG_SOCIETY_CONSENSUS_REACHED       0x6E86  /**< All agents converged */
#define BIO_MSG_SOCIETY_SCALE_UP                0x6E87  /**< Add more agents */
#define BIO_MSG_SOCIETY_SCALE_DOWN              0x6E88  /**< Remove agents */
#define BIO_MSG_SOCIETY_SESSION_START           0x6E89  /**< Deliberation session started */
#define BIO_MSG_SOCIETY_SESSION_END             0x6E8A  /**< Deliberation session ended */
#define BIO_MSG_SOCIETY_DIVERSITY_REPORT        0x6E8B  /**< Diversity metrics update */
#define BIO_MSG_SOCIETY_REALIZATION             0x6E8C  /**< Agent had realization (paper's key feature) */
```

### KG Wiring Relations

```jsonl
{"type": "entity", "name": "society_of_thought_engine", "entityType": "module", "observations": ["Top-level reasoning orchestrator implementing Kim et al. 2026 Society of Thought"]}
{"type": "entity", "name": "surprise_amplifier", "entityType": "module", "observations": ["Amplifies prediction error into attention/curiosity/executive signals"]}
{"type": "entity", "name": "adversarial_perspective", "entityType": "module", "observations": ["Enforces informed skepticism via ToM-generated counterarguments"]}
{"type": "entity", "name": "social_scaling_controller", "entityType": "module", "observations": ["Dynamically adjusts agent count based on problem difficulty"]}
{"type": "relation", "from": "surprise_amplifier", "to": "BIO_MSG_SOCIETY_SURPRISE_SIGNAL", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "adversarial_perspective", "to": "BIO_MSG_SOCIETY_CONFLICT_DETECTED", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "social_scaling_controller", "to": "BIO_MSG_SOCIETY_SCALE_UP", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "social_scaling_controller", "to": "BIO_MSG_SOCIETY_SCALE_DOWN", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "society_of_thought_engine", "to": "BIO_MSG_SOCIETY_SESSION_START", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "society_of_thought_engine", "to": "BIO_MSG_SOCIETY_SESSION_END", "relationType": "HANDLES_MESSAGE"}
{"type": "relation", "from": "society_of_thought_engine", "to": "BIO_MSG_SOCIETY_CONSENSUS_REACHED", "relationType": "HANDLES_MESSAGE"}
```

---

## 9. File Structure

### New Files (31 total)

```
include/cognitive/society/
├── nimcp_cognitive_agent_profile.h          # Phase 2
├── nimcp_adversarial_perspective.h          # Phase 3
├── nimcp_social_scaling.h                   # Phase 4
├── nimcp_society_of_thought.h              # Phase 5
└── nimcp_society_types.h                    # Shared types

include/cognitive/salience/
├── nimcp_surprise_amplifier.h              # Phase 1
├── nimcp_surprise_fep_bridge.h             # Phase 1
├── nimcp_surprise_gw_bridge.h              # Phase 1
└── nimcp_surprise_attention_bridge.h       # Phase 1

src/cognitive/society/
├── nimcp_cognitive_agent_profile.c          # Phase 2
├── nimcp_adversarial_perspective.c          # Phase 3
├── nimcp_social_scaling.c                   # Phase 4
└── nimcp_society_of_thought.c              # Phase 5

src/cognitive/salience/
├── nimcp_surprise_amplifier.c              # Phase 1
├── nimcp_surprise_fep_bridge.c             # Phase 1
├── nimcp_surprise_gw_bridge.c              # Phase 1
└── nimcp_surprise_attention_bridge.c       # Phase 1

src/core/brain/factory/init/
└── nimcp_brain_init_society_of_thought.c   # Phase 5

test/unit/cognitive/society/
├── test_surprise_amplifier.cpp             # Phase 1
├── test_cognitive_agent_profile.cpp        # Phase 2
├── test_adversarial_perspective.cpp        # Phase 3
├── test_social_scaling.cpp                 # Phase 4
└── test_society_of_thought.cpp             # Phase 5

test/integration/cognitive/society/
├── test_surprise_salience_integration.cpp  # Phase 1
├── test_society_rcog_integration.cpp       # Phase 2-5
└── test_society_gw_integration.cpp         # Phase 3-5

test/regression/cognitive/society/
└── test_society_regression.cpp             # Phase 5

test/e2e/cognitive/
└── test_society_of_thought_e2e.cpp         # Phase 5
```

### Files to Modify

```
include/async/nimcp_bio_messages.h          # Add module IDs and message types
include/core/brain/nimcp_brain.h            # Add society_of_thought_engine_t* field
src/core/brain/nimcp_brain.c                # Init/destroy society engine
src/lib/CMakeLists.txt                      # Add new source files
test/CMakeLists.txt                         # Add test targets
.aim/wiring/subsystems/cognition.jsonl      # Add KG wiring relations
include/cognitive/recursive/nimcp_rcog_delegation_pool.h  # Add profile field
src/cognitive/recursive/nimcp_rcog_delegation_pool.c      # Use profiles
```

---

## 10. Dependencies

### External Dependencies

None. All components use existing NIMCP infrastructure.

### Internal Module Dependencies

```
Phase 1 (Surprise Amplifier)
├── salience (nimcp_salience.h)
├── free_energy (nimcp_free_energy.h)
├── attention (nimcp_attention.h)  [optional]
├── curiosity (nimcp_curiosity.h)  [optional]
└── global_workspace (nimcp_global_workspace.h)  [optional]

Phase 2 (Agent Profiles)
├── personality (nimcp_personality.h)
├── rcog_delegation_pool (nimcp_rcog_delegation_pool.h)
└── Phase 1

Phase 3 (Adversarial Protocol)
├── theory_of_mind (nimcp_theory_of_mind.h)
├── game_theory (nimcp_game_theory.h, nimcp_bargaining.h)
├── global_workspace (nimcp_global_workspace.h)
├── Phase 1 (surprise_amplifier)
└── Phase 2 (agent profiles)

Phase 4 (Social Scaling)
├── introspection/metacognition (nimcp_introspection.h)
├── executive (nimcp_executive.h)
└── Phases 1-3

Phase 5 (Society Engine)
├── rcog_orchestrator (nimcp_rcog_orchestrator.h)
├── brain_immune (nimcp_brain_immune.h)  [optional]
├── bio_async (nimcp_bio_messages.h)
└── Phases 1-4
```

---

## 11. Testing Strategy

### Test Count Estimates

| Phase | Unit | Integration | Regression | E2E | Total |
|-------|------|-------------|------------|-----|-------|
| Phase 1 | 45 | 12 | 15 | 6 | 78 |
| Phase 2 | 30 | 8 | 10 | 4 | 52 |
| Phase 3 | 40 | 10 | 12 | 5 | 67 |
| Phase 4 | 25 | 8 | 10 | 4 | 47 |
| Phase 5 | 35 | 15 | 12 | 8 | 70 |
| **Total** | **175** | **53** | **59** | **27** | **314** |

### Key Test Scenarios

#### Phase 1 Tests
- Surprise fires when FEP prediction error exceeds threshold
- Surprise amplifies attention allocation measurably
- Refractory period prevents rapid re-firing
- Inter-agent conflict triggers surprise signal
- GW broadcast on surprise event
- Executive interrupt on high-magnitude surprise
- Surprise decay over time
- Multiple concurrent surprise sources

#### Phase 2 Tests
- Generated profiles have sufficient trait variance
- Contrarian agents have low agreeableness / high openness
- Diversity score increases with number of agents (up to diminishing returns)
- Devil's advocate profile opposes given majority profile
- RCOG delegates with different profiles produce different conclusions

#### Phase 3 Tests
- ToM generates informed counterarguments (not random noise)
- Conflicts detected when perspectives diverge beyond threshold
- Game-theoretic resolution selects highest evidence_strength
- Unresolved conflicts spawn new agents (with surprise amplifier)
- Convergence terminates debate early
- Max rounds enforced

#### Phase 4 Tests
- Trivial problems get 2 agents, extreme problems get 6-8
- Uncertainty increase triggers scale-up
- Diminishing marginal diversity triggers convergence
- Cooldown prevents rapid scale oscillation
- Metacognition uncertainty correlates with agent count

#### Phase 5 Tests
- Full deliberation session: spawn -> debate -> resolve -> consensus
- Consensus broadcast to global workspace
- Session metrics tracking (diversity, conflicts, rounds)
- Concurrent sessions don't interfere
- Immune modulation reduces society capacity during inflammation
- Bio-async message flow through complete pipeline

---

## 12. Risk Mitigation

### R1: Performance Overhead

**Risk**: Multi-agent reasoning is slower than single-path.
**Mitigation**: Social scaling controller defaults to 2 agents for trivial problems. Surprise amplifier alone (Phase 1) adds minimal overhead. Society mode is opt-in via `brain_config.enable_society_of_thought`.

### R2: Infinite Debate Loops

**Risk**: Adversarial protocol never converges.
**Mitigation**: Hard cap at `max_rounds` (default 5). Forced resolution via highest evidence_strength perspective. Timeout per session.

### R3: Personality Bias Amplification

**Risk**: Extreme personality profiles produce degenerate reasoning.
**Mitigation**: Profiles are bounded to [0.1, 0.9] range on all Big Five traits. Ethics module review of all conclusions regardless of society outcome.

### R4: Resource Exhaustion

**Risk**: Too many agents consume excessive memory/CPU.
**Mitigation**: `max_agents` cap (default 8). Agents share read-only brain state, only diverge on reasoning context. Health agent monitors society resource usage.

### R5: Conflict with Existing RCOG

**Risk**: Society engine fights with existing RCOG orchestrator.
**Mitigation**: Society engine sits above RCOG as an optional layer. RCOG delegation pool gains profile support but works identically without profiles. No breaking changes.

---

## Appendix A: Paper-to-NIMCP Mapping

| Paper Finding | NIMCP Component | Phase |
|---|---|---|
| Layer 15 Feature 30939 (surprise/realization) | `surprise_amplifier` + FEP prediction error | 1 |
| Conversation-primed > monologue | Personality-diverse RCOG delegation | 2 |
| High Big Five variance (Openness, Neuroticism) | `cognitive_agent_profile_t` with `MAXIMALLY_DIVERSE` strategy | 2 |
| Conflict of Perspectives as fundamental unit | `adversarial_perspective_t` with game-theoretic resolution | 3 |
| Social scaffolding for RL initialization | Profile-primed delegates as initial reasoning population | 2-3 |
| Internal personas with distinct traits | `cognitive_agent_profile_t` per RCOG worker | 2 |
| Reconciliation after conflict | `adversarial_perspective_resolve()` via Nash bargaining | 3 |
| Cognitive diversity in teams (org psych) | `cognitive_agent_profile_diversity_score()` | 2 |
| Social Scaling hypothesis | `social_scaling_controller_t` with difficulty assessment | 4 |
| Multi-agent primitives in pre-training | Society engine as architectural primitive in brain lifecycle | 5 |

## Appendix B: Biological Correlates

| Society of Thought Mechanism | Brain Region/System | Neurotransmitter |
|---|---|---|
| Surprise amplification | Locus coeruleus, anterior cingulate cortex | Norepinephrine |
| Prediction error signal | VTA, substantia nigra, basal ganglia | Dopamine |
| Perspective generation | Prefrontal cortex, default mode network | - |
| Conflict detection | Anterior cingulate cortex | - |
| Adversarial evaluation | Dorsolateral prefrontal cortex | - |
| Resolution/convergence | Orbitofrontal cortex, ventromedial PFC | Serotonin |
| Social scaling | Hypothalamus (arousal), reticular formation | Norepinephrine, Acetylcholine |
| Curiosity boost on surprise | Hippocampus, nucleus accumbens | Dopamine |
| Attention modulation | Parietal cortex, superior colliculus | Acetylcholine |

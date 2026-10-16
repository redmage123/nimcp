# Training Architecture Enhancements Plan

**Version**: 1.0.0
**Created**: 2026-01-17
**Status**: Proposed

---

## Executive Summary

This plan outlines enhancements to NIMCP's world model and theory of mind training architecture. The goal is to improve learning efficiency, biological plausibility, and cognitive capabilities while maintaining the project's core principles of scalability and ethical foundation.

---

## Table of Contents

1. [Goals and Objectives](#1-goals-and-objectives)
2. [Phase 1: Foundation Enhancements](#2-phase-1-foundation-enhancements)
3. [Phase 2: World Model Enhancements](#3-phase-2-world-model-enhancements)
4. [Phase 3: Theory of Mind Enhancements](#4-phase-3-theory-of-mind-enhancements)
5. [Phase 4: Integration Layer](#5-phase-4-integration-layer)
6. [Phase 5: Biological Plausibility](#6-phase-5-biological-plausibility)
7. [File Structure](#7-file-structure)
8. [Dependencies](#8-dependencies)
9. [Testing Strategy](#9-testing-strategy)
10. [Risk Mitigation](#10-risk-mitigation)

---

## 1. Goals and Objectives

### Primary Goals

| Goal | Description | Success Metric |
|------|-------------|----------------|
| **G1** | Improve long-horizon planning | 50% reduction in planning errors at horizon > 20 steps |
| **G2** | Prevent catastrophic forgetting | < 5% performance degradation on old tasks after learning new |
| **G3** | Accelerate social learning | 30% faster ToM inference convergence |
| **G4** | Enable compositional reasoning | Successfully predict novel object combinations |
| **G5** | Biological alignment | Pass neuroscience benchmark comparisons |

### Design Principles

1. **Backward Compatibility**: All enhancements must work with existing NIMCP modules
2. **Graceful Degradation**: Features scale down on constrained platforms (Portia tiers)
3. **Testability**: Each component has unit, integration, and regression tests
4. **Modularity**: Enhancements can be enabled/disabled independently

---

## 2. Phase 1: Foundation Enhancements

**Focus**: Training infrastructure improvements that benefit all subsequent phases

### 2.1 Continual Learning System

**Priority**: HIGH
**Effort**: Medium

#### Files to Create

```
include/training/continual/
├── nimcp_continual_learning.h      # Main API
├── nimcp_ewc.h                     # Elastic Weight Consolidation
├── nimcp_synaptic_intelligence.h   # Online importance estimation
└── nimcp_memory_replay.h           # Experience replay buffer

src/training/continual/
├── nimcp_continual_learning.c
├── nimcp_ewc.c
├── nimcp_synaptic_intelligence.c
└── nimcp_memory_replay.c
```

#### API Design

```c
// nimcp_continual_learning.h

typedef enum {
    CONTINUAL_METHOD_EWC,           // Elastic Weight Consolidation
    CONTINUAL_METHOD_SI,            // Synaptic Intelligence
    CONTINUAL_METHOD_MAS,           // Memory Aware Synapses
    CONTINUAL_METHOD_PACKNET,       // Progressive pruning
    CONTINUAL_METHOD_REPLAY         // Experience replay
} continual_method_t;

typedef struct {
    continual_method_t method;
    float consolidation_strength;   // Lambda for regularization
    uint32_t fisher_samples;        // Samples for Fisher estimation
    bool online_update;             // Update importance online
    float importance_decay;         // Decay rate for old importance
} continual_learning_config_t;

typedef struct continual_learning continual_learning_t;

// Lifecycle
continual_learning_t* continual_learning_create(continual_learning_config_t* config);
void continual_learning_destroy(continual_learning_t* cl);

// Task management
int continual_learning_begin_task(continual_learning_t* cl, const char* task_name);
int continual_learning_end_task(continual_learning_t* cl);

// Weight protection
int continual_learning_compute_importance(continual_learning_t* cl,
                                          float* weights, uint32_t num_weights);
float continual_learning_regularization_loss(continual_learning_t* cl,
                                             float* current_weights);

// Gradient modification
int continual_learning_modify_gradients(continual_learning_t* cl,
                                        float* gradients, uint32_t num_weights);
```

#### Integration Points

- `nimcp_training_module.h`: Add continual learning hooks
- `nimcp_training_dispatch.h`: Route consolidation calls
- `nimcp_brain_init_training.c`: Initialize during brain creation

### 2.2 Adaptive Curriculum System

**Priority**: HIGH
**Effort**: Low

#### Files to Create

```
include/training/curriculum/
├── nimcp_adaptive_curriculum.h
└── nimcp_competence_tracker.h

src/training/curriculum/
├── nimcp_adaptive_curriculum.c
└── nimcp_competence_tracker.c
```

#### API Design

```c
// nimcp_adaptive_curriculum.h

typedef struct {
    float target_success_rate;      // ZPD target (default: 0.7)
    float difficulty_step;          // Adjustment granularity
    uint32_t window_size;           // Rolling window for competence
    bool auto_promote;              // Auto-advance difficulty
} adaptive_curriculum_config_t;

typedef struct adaptive_curriculum adaptive_curriculum_t;

// Lifecycle
adaptive_curriculum_t* adaptive_curriculum_create(adaptive_curriculum_config_t* config);
void adaptive_curriculum_destroy(adaptive_curriculum_t* ac);

// Domain management
int adaptive_curriculum_register_domain(adaptive_curriculum_t* ac,
                                        const char* domain_name,
                                        float initial_difficulty);

// Example selection
training_example_t* adaptive_curriculum_sample(adaptive_curriculum_t* ac);
float adaptive_curriculum_estimate_difficulty(adaptive_curriculum_t* ac,
                                              training_example_t* ex);

// Feedback
int adaptive_curriculum_record_outcome(adaptive_curriculum_t* ac,
                                       training_example_t* ex,
                                       bool success,
                                       float loss);

// Competence queries
float adaptive_curriculum_get_competence(adaptive_curriculum_t* ac,
                                         const char* domain_name);
```

### 2.3 Meta-Learning Framework

**Priority**: MEDIUM
**Effort**: Medium

#### Files to Create

```
include/training/meta/
├── nimcp_meta_learning.h
├── nimcp_maml.h                    # Model-Agnostic Meta-Learning
└── nimcp_hypernetwork.h            # Weight generation networks

src/training/meta/
├── nimcp_meta_learning.c
├── nimcp_maml.c
└── nimcp_hypernetwork.c
```

#### API Design

```c
// nimcp_meta_learning.h

typedef enum {
    META_METHOD_MAML,               // Gradient-based
    META_METHOD_REPTILE,            // First-order approximation
    META_METHOD_HYPERNETWORK,       // Weight generation
    META_METHOD_MODULATION          // Learning rate adaptation
} meta_learning_method_t;

typedef struct {
    meta_learning_method_t method;
    float inner_lr;                 // Task-specific learning rate
    float outer_lr;                 // Meta learning rate
    uint32_t inner_steps;           // Adaptation steps per task
    uint32_t task_batch_size;       // Tasks per meta-update
} meta_learning_config_t;

typedef struct meta_learning meta_learning_t;

// Lifecycle
meta_learning_t* meta_learning_create(meta_learning_config_t* config);
void meta_learning_destroy(meta_learning_t* ml);

// Meta-training
int meta_learning_begin_task(meta_learning_t* ml, task_t* task);
int meta_learning_inner_step(meta_learning_t* ml, training_batch_t* batch);
int meta_learning_end_task(meta_learning_t* ml);
int meta_learning_meta_update(meta_learning_t* ml);

// Fast adaptation (at test time)
int meta_learning_adapt(meta_learning_t* ml,
                        training_example_t* support_set,
                        uint32_t num_examples);
```

---

## 3. Phase 2: World Model Enhancements

**Focus**: Hierarchical, object-centric, and causal world modeling

### 3.1 Hierarchical World Model

**Priority**: HIGH
**Effort**: High

#### Files to Create

```
include/cognitive/world_model/
├── nimcp_hierarchical_wm.h         # Multi-timescale world model
├── nimcp_temporal_abstraction.h    # Timescale management
└── nimcp_cross_level_prediction.h  # Top-down/bottom-up integration

src/cognitive/world_model/
├── nimcp_hierarchical_wm.c
├── nimcp_temporal_abstraction.c
└── nimcp_cross_level_prediction.c
```

#### API Design

```c
// nimcp_hierarchical_wm.h

typedef struct {
    uint32_t num_levels;            // Hierarchy depth (default: 4)
    uint32_t abstraction_ratios[8]; // Temporal ratios (e.g., {1,5,25,125})
    uint32_t state_dims[8];         // State dimension per level
    float top_down_weight;          // Prior strength from higher levels
    float bottom_up_weight;         // Error strength from lower levels
} hierarchical_wm_config_t;

typedef struct hierarchical_wm hierarchical_wm_t;

// Lifecycle
hierarchical_wm_t* hierarchical_wm_create(hierarchical_wm_config_t* config);
void hierarchical_wm_destroy(hierarchical_wm_t* hwm);

// Prediction at multiple timescales
int hierarchical_wm_predict(hierarchical_wm_t* hwm,
                            state_t* current,
                            action_t* action,
                            uint32_t horizon,
                            hierarchical_prediction_t* out);

// Level-specific access
int hierarchical_wm_get_level_state(hierarchical_wm_t* hwm,
                                    uint32_t level,
                                    state_t* out);

// Cross-level communication
int hierarchical_wm_propagate_top_down(hierarchical_wm_t* hwm);
int hierarchical_wm_propagate_bottom_up(hierarchical_wm_t* hwm,
                                        observation_t* obs);

// Training
int hierarchical_wm_train_step(hierarchical_wm_t* hwm,
                               experience_t* exp,
                               hierarchical_wm_loss_t* out_loss);
```

#### Integration with Existing World Model

```c
// Extend omni_wm_config_t
typedef struct {
    // ... existing fields ...

    // Hierarchical extension
    bool enable_hierarchy;
    hierarchical_wm_config_t hierarchy_config;
} omni_wm_config_extended_t;
```

### 3.2 Object-Centric World Model

**Priority**: MEDIUM
**Effort**: High

#### Files to Create

```
include/cognitive/world_model/
├── nimcp_object_centric_wm.h       # Slot-based object representation
├── nimcp_slot_attention.h          # Object discovery mechanism
└── nimcp_relational_dynamics.h     # Object interaction modeling

src/cognitive/world_model/
├── nimcp_object_centric_wm.c
├── nimcp_slot_attention.c
└── nimcp_relational_dynamics.c
```

#### API Design

```c
// nimcp_object_centric_wm.h

#define MAX_OBJECT_SLOTS 32

typedef struct {
    uint32_t num_slots;             // Max objects to track
    uint32_t slot_dim;              // Dimension per slot
    uint32_t num_iterations;        // Slot attention iterations
    bool enable_relational;         // Learn object interactions
    uint32_t relation_dim;          // Relation embedding dimension
} object_centric_config_t;

typedef struct {
    float embedding[SLOT_DIM_MAX];
    float attention_weight;         // How much input assigned here
    float existence_prob;           // Is this slot occupied?
    uint32_t object_id;             // Tracking ID
} object_slot_t;

typedef struct object_centric_wm object_centric_wm_t;

// Lifecycle
object_centric_wm_t* object_centric_wm_create(object_centric_config_t* config);
void object_centric_wm_destroy(object_centric_wm_t* ocwm);

// Object discovery
int object_centric_wm_encode(object_centric_wm_t* ocwm,
                             observation_t* obs,
                             object_slot_t* slots_out,
                             uint32_t* num_objects_out);

// Relational prediction
int object_centric_wm_predict_interactions(object_centric_wm_t* ocwm,
                                           object_slot_t* slots,
                                           uint32_t num_objects,
                                           interaction_t* interactions_out);

// Per-object dynamics
int object_centric_wm_predict_slot(object_centric_wm_t* ocwm,
                                   object_slot_t* slot,
                                   action_t* action,
                                   object_slot_t* next_slot_out);
```

### 3.3 Causal World Model

**Priority**: MEDIUM
**Effort**: High

#### Files to Create

```
include/cognitive/world_model/
├── nimcp_causal_wm.h               # Causal structure discovery
├── nimcp_causal_graph.h            # DAG representation
└── nimcp_counterfactual.h          # What-if reasoning

src/cognitive/world_model/
├── nimcp_causal_wm.c
├── nimcp_causal_graph.c
└── nimcp_counterfactual.c
```

#### API Design

```c
// nimcp_causal_wm.h

typedef struct {
    uint32_t max_variables;         // Max causal variables
    float edge_threshold;           // Min strength for edge
    bool enable_interventions;      // Active causal discovery
    float intervention_rate;        // How often to intervene
} causal_wm_config_t;

typedef struct {
    uint32_t num_variables;
    float adjacency[MAX_VARS][MAX_VARS];  // Weighted DAG
    char* variable_names[MAX_VARS];
} causal_graph_t;

typedef struct {
    uint32_t variable_idx;
    float value;
} intervention_t;

typedef struct causal_wm causal_wm_t;

// Lifecycle
causal_wm_t* causal_wm_create(causal_wm_config_t* config);
void causal_wm_destroy(causal_wm_t* cwm);

// Structure learning
int causal_wm_update_structure(causal_wm_t* cwm,
                               observation_t* obs);
int causal_wm_get_graph(causal_wm_t* cwm, causal_graph_t* out);

// Counterfactual queries
int causal_wm_counterfactual(causal_wm_t* cwm,
                             observation_t* actual,
                             intervention_t* do_op,
                             observation_t* counterfactual_out);

// Causal effect estimation
float causal_wm_average_treatment_effect(causal_wm_t* cwm,
                                         uint32_t cause_var,
                                         uint32_t effect_var);
```

---

## 4. Phase 3: Theory of Mind Enhancements

**Focus**: Developmental trajectory, affective ToM, and pragmatic inference

### 4.1 Developmental Theory of Mind

**Priority**: MEDIUM
**Effort**: Medium

#### Files to Create

```
include/cognitive/theory_of_mind/
├── nimcp_tom_developmental.h       # Staged ToM development
├── nimcp_tom_levels.h              # Level definitions
└── nimcp_tom_curriculum.h          # ToM training curriculum

src/cognitive/theory_of_mind/
├── nimcp_tom_developmental.c
├── nimcp_tom_levels.c
└── nimcp_tom_curriculum.c
```

#### API Design

```c
// nimcp_tom_developmental.h

typedef enum {
    TOM_LEVEL_0_SELF_OTHER = 0,     // Distinguish self from other
    TOM_LEVEL_1_DESIRES = 1,        // Others have desires
    TOM_LEVEL_2_BELIEFS = 2,        // Others have beliefs
    TOM_LEVEL_3_FALSE_BELIEF = 3,   // Beliefs can be wrong
    TOM_LEVEL_4_NESTED = 4,         // Beliefs about beliefs
    TOM_LEVEL_5_RECURSIVE = 5       // Unlimited nesting
} tom_level_t;

typedef struct {
    tom_level_t initial_level;      // Starting level
    float promotion_threshold;      // Competence needed to advance
    bool auto_promote;              // Automatic level advancement
    uint32_t min_examples_per_level;// Min training before promotion
} tom_developmental_config_t;

typedef struct tom_developmental tom_developmental_t;

// Lifecycle
tom_developmental_t* tom_developmental_create(tom_developmental_config_t* config);
void tom_developmental_destroy(tom_developmental_t* td);

// Level management
tom_level_t tom_developmental_get_level(tom_developmental_t* td);
float tom_developmental_get_competence(tom_developmental_t* td, tom_level_t level);
int tom_developmental_promote(tom_developmental_t* td);

// Training
int tom_developmental_train_example(tom_developmental_t* td,
                                    tom_training_example_t* ex);

// Inference (respects current level)
int tom_developmental_infer(tom_developmental_t* td,
                            agent_observation_t* obs,
                            tom_inference_t* out);
```

### 4.2 Affective Theory of Mind

**Priority**: MEDIUM
**Effort**: Medium

#### Files to Create

```
include/cognitive/theory_of_mind/
├── nimcp_affective_tom.h           # Emotional inference
├── nimcp_appraisal_model.h         # Scherer's CPM
└── nimcp_empathy.h                 # Empathic resonance

src/cognitive/theory_of_mind/
├── nimcp_affective_tom.c
├── nimcp_appraisal_model.c
└── nimcp_empathy.c
```

#### API Design

```c
// nimcp_affective_tom.h

typedef struct {
    float novelty;                  // How unexpected
    float pleasantness;             // Intrinsic valence
    float goal_conduciveness;       // Helps/hinders goals
    float coping_potential;         // Can they handle it
    float norm_compatibility;       // Fits social norms
} appraisal_t;

typedef struct {
    bool enable_appraisal_model;    // Use Scherer's CPM
    bool enable_empathy;            // Emotional contagion
    float empathy_strength;         // Resonance intensity
    float emotion_decay_rate;       // Inference decay
} affective_tom_config_t;

typedef struct affective_tom affective_tom_t;

// Lifecycle
affective_tom_t* affective_tom_create(affective_tom_config_t* config);
void affective_tom_destroy(affective_tom_t* at);

// Emotion inference
int affective_tom_infer_emotion(affective_tom_t* at,
                                agent_observation_t* obs,
                                tom_emotion_t* emotion_out,
                                float* confidence_out);

// Appraisal-based prediction
int affective_tom_predict_from_appraisal(affective_tom_t* at,
                                         appraisal_t* appraisal,
                                         tom_emotion_t* emotion_out);

// Empathic response
int affective_tom_compute_empathy(affective_tom_t* at,
                                  tom_emotion_t* other_emotion,
                                  empathy_response_t* response_out);
```

### 4.3 Pragmatic Inference (Gricean)

**Priority**: LOW
**Effort**: Medium

#### Files to Create

```
include/cognitive/theory_of_mind/
├── nimcp_pragmatic_tom.h           # Communication-specific ToM
├── nimcp_gricean_maxims.h          # Maxim modeling
└── nimcp_rsa_model.h               # Rational Speech Acts

src/cognitive/theory_of_mind/
├── nimcp_pragmatic_tom.c
├── nimcp_gricean_maxims.c
└── nimcp_rsa_model.c
```

#### API Design

```c
// nimcp_pragmatic_tom.h

typedef struct {
    float quality;                  // Speaker truthfulness estimate
    float quantity;                 // Informativeness estimate
    float relevance;                // Relevance adherence
    float manner;                   // Clarity estimate
} gricean_profile_t;

typedef struct {
    bool enable_rsa;                // Rational Speech Acts
    uint32_t rsa_depth;             // Recursion depth (L0, L1, L2...)
    float rsa_temperature;          // Softmax temperature
} pragmatic_tom_config_t;

typedef struct pragmatic_tom pragmatic_tom_t;

// Lifecycle
pragmatic_tom_t* pragmatic_tom_create(pragmatic_tom_config_t* config);
void pragmatic_tom_destroy(pragmatic_tom_t* pt);

// Literal vs. pragmatic meaning
int pragmatic_tom_infer_meaning(pragmatic_tom_t* pt,
                                utterance_t* utterance,
                                context_t* context,
                                meaning_t* literal_out,
                                meaning_t* pragmatic_out,
                                float* implicature_confidence);

// Speaker modeling
int pragmatic_tom_update_speaker_model(pragmatic_tom_t* pt,
                                       agent_id_t speaker,
                                       utterance_t* utterance,
                                       bool was_truthful);
```

### 4.4 Group Theory of Mind

**Priority**: LOW
**Effort**: Medium

#### Files to Create

```
include/cognitive/theory_of_mind/
├── nimcp_group_tom.h               # Collective mental states
├── nimcp_shared_beliefs.h          # Common knowledge
└── nimcp_social_identity.h         # In-group/out-group

src/cognitive/theory_of_mind/
├── nimcp_group_tom.c
├── nimcp_shared_beliefs.c
└── nimcp_social_identity.c
```

---

## 5. Phase 4: Integration Layer

**Focus**: Unified social world model combining world model and ToM

### 5.1 Multi-Agent World Model

**Priority**: HIGH
**Effort**: High

#### Files to Create

```
include/cognitive/social_world_model/
├── nimcp_social_world_model.h      # Unified interface
├── nimcp_multi_agent_wm.h          # Multi-agent dynamics
├── nimcp_agent_policy_model.h      # Per-agent policy inference
└── nimcp_irl_bridge.h              # Inverse RL for ToM

src/cognitive/social_world_model/
├── nimcp_social_world_model.c
├── nimcp_multi_agent_wm.c
├── nimcp_agent_policy_model.c
└── nimcp_irl_bridge.c
```

#### API Design

```c
// nimcp_social_world_model.h

typedef struct {
    // Physical world model config
    hierarchical_wm_config_t physical_config;

    // Social modeling config
    uint32_t max_agents;
    bool enable_irl;                // Inverse RL for reward inference
    bool enable_opponent_modeling;  // Game-theoretic reasoning

    // ToM integration
    tom_developmental_config_t tom_config;
    affective_tom_config_t affective_config;
} social_wm_config_t;

typedef struct social_world_model social_world_model_t;

// Lifecycle
social_world_model_t* social_wm_create(social_wm_config_t* config);
void social_wm_destroy(social_world_model_t* swm);

// Unified prediction
int social_wm_predict(social_world_model_t* swm,
                      social_state_t* current,
                      action_t* my_action,
                      uint32_t horizon,
                      social_prediction_t* out);

// Counterfactual social reasoning
int social_wm_counterfactual(social_world_model_t* swm,
                             social_state_t* actual,
                             intervention_t* what_if,
                             social_state_t* counterfactual_out);

// Agent modeling
int social_wm_update_agent_model(social_world_model_t* swm,
                                 agent_id_t agent,
                                 trajectory_t* observed);
int social_wm_get_agent_policy(social_world_model_t* swm,
                               agent_id_t agent,
                               policy_model_t* out);

// Action selection with social awareness
int social_wm_select_action(social_world_model_t* swm,
                            goal_t* goal,
                            ethical_constraints_t* ethics,
                            action_t* action_out);
```

### 5.2 IRL-ToM Bridge

**Priority**: MEDIUM
**Effort**: Medium

```c
// nimcp_irl_bridge.h

typedef struct {
    float learning_rate;
    uint32_t max_iterations;
    irl_method_t method;            // MAX_ENT, DEEP_IRL, etc.
} irl_config_t;

typedef struct irl_tom_bridge irl_tom_bridge_t;

// Lifecycle
irl_tom_bridge_t* irl_tom_bridge_create(irl_config_t* config);
void irl_tom_bridge_destroy(irl_tom_bridge_t* bridge);

// Reward inference
int irl_infer_reward(irl_tom_bridge_t* bridge,
                     agent_id_t agent,
                     trajectory_t* demonstrations,
                     uint32_t num_demos,
                     reward_function_t* reward_out);

// Convert to ToM desires
int irl_reward_to_desires(irl_tom_bridge_t* bridge,
                          reward_function_t* reward,
                          tom_desire_t* desires_out,
                          uint32_t* num_desires_out);
```

---

## 6. Phase 5: Biological Plausibility

**Focus**: Replay, neuromodulatory gating, dendritic computation

### 6.1 Replay System

**Priority**: MEDIUM
**Effort**: Medium

#### Files to Create

```
include/cognitive/replay/
├── nimcp_replay_system.h           # Forward/reverse replay
├── nimcp_sharp_wave_ripple.h       # Replay triggering
└── nimcp_prioritized_replay.h      # Priority-based sampling

src/cognitive/replay/
├── nimcp_replay_system.c
├── nimcp_sharp_wave_ripple.c
└── nimcp_prioritized_replay.c
```

#### API Design

```c
// nimcp_replay_system.h

typedef enum {
    REPLAY_MODE_FORWARD,            // Planning / prediction
    REPLAY_MODE_REVERSE,            // Credit assignment
    REPLAY_MODE_BIDIRECTIONAL       // Both directions
} replay_mode_t;

typedef struct {
    replay_mode_t mode;
    uint32_t replay_horizon;        // Steps to replay
    float priority_exponent;        // Prioritized sampling
    float surprise_threshold;       // Trigger threshold
    bool enable_sleep_replay;       // Enhanced during sleep
} replay_config_t;

typedef struct replay_system replay_system_t;

// Lifecycle
replay_system_t* replay_system_create(replay_config_t* config);
void replay_system_destroy(replay_system_t* rs);

// Experience storage
int replay_system_store(replay_system_t* rs, experience_t* exp);

// Replay execution
int replay_system_forward_replay(replay_system_t* rs,
                                 state_t* start,
                                 uint32_t horizon,
                                 trajectory_t* out);
int replay_system_reverse_replay(replay_system_t* rs,
                                 state_t* goal,
                                 uint32_t lookback,
                                 trajectory_t* out);

// Triggering
bool replay_system_should_trigger(replay_system_t* rs,
                                  float surprise,
                                  arousal_level_t arousal);
```

### 6.2 Neuromodulatory Learning Gating

**Priority**: MEDIUM
**Effort**: Low

#### Files to Create

```
include/training/neuromod/
├── nimcp_neuromod_gating.h         # Learning rate modulation
└── nimcp_eligibility_modulation.h  # Trace modulation

src/training/neuromod/
├── nimcp_neuromod_gating.c
└── nimcp_eligibility_modulation.c
```

#### API Design

```c
// nimcp_neuromod_gating.h

typedef struct {
    float dopamine_gain;            // Reward prediction error
    float norepinephrine_gain;      // Unexpected uncertainty
    float acetylcholine_gain;       // Expected uncertainty
    float serotonin_gain;           // Aversive prediction
} neuromod_learning_gains_t;

typedef struct neuromod_gating neuromod_gating_t;

// Lifecycle
neuromod_gating_t* neuromod_gating_create(void);
void neuromod_gating_destroy(neuromod_gating_t* ng);

// Learning rate computation
float neuromod_gating_compute_lr(neuromod_gating_t* ng,
                                 float base_lr,
                                 neuromodulator_state_t* nm_state);

// Eligibility trace modulation
float neuromod_gating_eligibility_decay(neuromod_gating_t* ng,
                                        neuromodulator_state_t* nm_state);

// Per-synapse gating
int neuromod_gating_apply(neuromod_gating_t* ng,
                          float* gradients,
                          uint32_t num_weights,
                          neuromodulator_state_t* nm_state);
```

### 6.3 Dendritic Error Computation

**Priority**: LOW
**Effort**: High

#### Files to Create

```
include/cognitive/dendritic/
├── nimcp_dendritic_computation.h   # Local error signals
├── nimcp_apical_basal.h            # Compartment separation
└── nimcp_burst_coding.h            # Error signaling via bursts

src/cognitive/dendritic/
├── nimcp_dendritic_computation.c
├── nimcp_apical_basal.c
└── nimcp_burst_coding.c
```

---

## 7. File Structure

### New Directory Structure

```
include/
├── training/
│   ├── continual/
│   │   ├── nimcp_continual_learning.h
│   │   ├── nimcp_ewc.h
│   │   ├── nimcp_synaptic_intelligence.h
│   │   └── nimcp_memory_replay.h
│   ├── curriculum/
│   │   ├── nimcp_adaptive_curriculum.h
│   │   └── nimcp_competence_tracker.h
│   ├── meta/
│   │   ├── nimcp_meta_learning.h
│   │   ├── nimcp_maml.h
│   │   └── nimcp_hypernetwork.h
│   └── neuromod/
│       ├── nimcp_neuromod_gating.h
│       └── nimcp_eligibility_modulation.h
├── cognitive/
│   ├── world_model/
│   │   ├── nimcp_hierarchical_wm.h
│   │   ├── nimcp_temporal_abstraction.h
│   │   ├── nimcp_cross_level_prediction.h
│   │   ├── nimcp_object_centric_wm.h
│   │   ├── nimcp_slot_attention.h
│   │   ├── nimcp_relational_dynamics.h
│   │   ├── nimcp_causal_wm.h
│   │   ├── nimcp_causal_graph.h
│   │   └── nimcp_counterfactual.h
│   ├── theory_of_mind/
│   │   ├── nimcp_tom_developmental.h
│   │   ├── nimcp_tom_levels.h
│   │   ├── nimcp_tom_curriculum.h
│   │   ├── nimcp_affective_tom.h
│   │   ├── nimcp_appraisal_model.h
│   │   ├── nimcp_empathy.h
│   │   ├── nimcp_pragmatic_tom.h
│   │   ├── nimcp_gricean_maxims.h
│   │   ├── nimcp_rsa_model.h
│   │   ├── nimcp_group_tom.h
│   │   ├── nimcp_shared_beliefs.h
│   │   └── nimcp_social_identity.h
│   ├── social_world_model/
│   │   ├── nimcp_social_world_model.h
│   │   ├── nimcp_multi_agent_wm.h
│   │   ├── nimcp_agent_policy_model.h
│   │   └── nimcp_irl_bridge.h
│   ├── replay/
│   │   ├── nimcp_replay_system.h
│   │   ├── nimcp_sharp_wave_ripple.h
│   │   └── nimcp_prioritized_replay.h
│   └── dendritic/
│       ├── nimcp_dendritic_computation.h
│       ├── nimcp_apical_basal.h
│       └── nimcp_burst_coding.h

src/
└── [mirrors include/ structure]

test/
├── unit/
│   ├── training/
│   │   ├── continual/
│   │   ├── curriculum/
│   │   ├── meta/
│   │   └── neuromod/
│   └── cognitive/
│       ├── world_model/
│       ├── theory_of_mind/
│       ├── social_world_model/
│       ├── replay/
│       └── dendritic/
├── integration/
│   └── [same structure]
├── regression/
│   └── [same structure]
└── e2e/
    └── [same structure]
```

---

## 8. Dependencies

### Internal Dependencies

| Module | Depends On |
|--------|------------|
| Hierarchical WM | `nimcp_omni_world_model`, `nimcp_tensor` |
| Object-Centric WM | `nimcp_slot_attention`, `nimcp_tensor`, `nimcp_gpu` |
| Causal WM | `nimcp_omni_world_model`, `nimcp_causal_graph` |
| Continual Learning | `nimcp_training_module`, `nimcp_tensor` |
| Meta Learning | `nimcp_training_dispatch`, `nimcp_tensor` |
| Social WM | `nimcp_hierarchical_wm`, `nimcp_theory_of_mind`, `nimcp_multi_agent_wm` |
| Replay System | `nimcp_hippocampus`, `nimcp_sleep_wake` |
| Neuromod Gating | `nimcp_neuromodulation`, `nimcp_training_module` |

### External Dependencies

- None required (C standard library only)
- Optional: CUDA for GPU-accelerated slot attention

---

## 9. Testing Strategy

### Test Coverage Requirements

| Module | Unit | Integration | Regression | E2E |
|--------|------|-------------|------------|-----|
| Continual Learning | 50+ | 15+ | 10+ | 5+ |
| Adaptive Curriculum | 30+ | 10+ | 5+ | 3+ |
| Meta Learning | 40+ | 15+ | 10+ | 5+ |
| Hierarchical WM | 60+ | 20+ | 15+ | 8+ |
| Object-Centric WM | 50+ | 15+ | 10+ | 5+ |
| Causal WM | 40+ | 15+ | 10+ | 5+ |
| Developmental ToM | 50+ | 20+ | 15+ | 8+ |
| Affective ToM | 40+ | 15+ | 10+ | 5+ |
| Social WM | 60+ | 25+ | 20+ | 10+ |
| Replay System | 40+ | 15+ | 10+ | 5+ |

### Key Test Scenarios

1. **Continual Learning**: Train on Task A, then B - verify A performance maintained
2. **Hierarchical WM**: Long-horizon prediction accuracy at each level
3. **Object-Centric**: Novel object combination generalization
4. **Causal WM**: Counterfactual accuracy vs. ground truth
5. **Developmental ToM**: False belief task pass rate at each level
6. **Social WM**: Multi-agent prediction in competitive/cooperative games

---

## 10. Risk Mitigation

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Integration complexity | High | Medium | Phased rollout, feature flags |
| Performance regression | Medium | High | Benchmark suite, profiling |
| Memory overhead | Medium | Medium | Lazy initialization, tiered features |
| API instability | Low | High | Version pinning, deprecation policy |

### Mitigation Strategies

1. **Feature Flags**: All enhancements disabled by default
   ```c
   brain_config.enable_hierarchical_wm = false;  // Opt-in
   brain_config.enable_continual_learning = false;
   ```

2. **Graceful Degradation**: Features scale with platform tier
   ```c
   if (platform_tier < PLATFORM_TIER_FULL) {
       config.num_hierarchy_levels = 2;  // Reduced from 4
   }
   ```

3. **Backward Compatibility**: Existing APIs unchanged
   - New structs extend old ones
   - New functions supplement, don't replace

4. **Incremental Testing**: Each PR requires:
   - Unit tests for new code
   - Regression tests for touched code
   - Integration test if cross-module

---

## 11. Implementation Order

### Recommended Sequence

```
Phase 1.1: Continual Learning (EWC/SI)
    └── Phase 1.2: Adaptive Curriculum
            └── Phase 2.1: Hierarchical World Model
                    └── Phase 4.1: Multi-Agent World Model
                            └── Phase 3.1: Developmental ToM
                                    └── Phase 4.2: IRL-ToM Bridge
                                            └── Phase 5.1: Replay System

[Parallel Track]
Phase 1.3: Meta Learning
    └── Phase 2.2: Object-Centric WM
            └── Phase 2.3: Causal WM

[Parallel Track]
Phase 3.2: Affective ToM
    └── Phase 3.3: Pragmatic ToM
            └── Phase 3.4: Group ToM

[Later Phase]
Phase 5.2: Neuromod Gating
    └── Phase 5.3: Dendritic Computation
```

---

## 12. Success Criteria

### Phase 1 Complete When:
- [ ] Continual learning prevents >95% of catastrophic forgetting
- [ ] Adaptive curriculum improves learning efficiency by >20%
- [ ] Meta-learning enables <10 example adaptation

### Phase 2 Complete When:
- [ ] Hierarchical WM improves long-horizon accuracy by >30%
- [ ] Object-centric WM generalizes to novel combinations
- [ ] Causal WM passes >80% of counterfactual benchmarks

### Phase 3 Complete When:
- [ ] Developmental ToM passes standard false-belief tests
- [ ] Affective ToM achieves >70% emotion prediction accuracy
- [ ] Pragmatic ToM correctly infers implicatures

### Phase 4 Complete When:
- [ ] Social WM predicts multi-agent outcomes accurately
- [ ] IRL correctly infers reward functions from demonstrations

### Phase 5 Complete When:
- [ ] Replay improves sample efficiency by >25%
- [ ] Neuromod gating matches biological learning curves

---

## Appendix A: Research References

1. Hafner et al. (2025). "Mastering Diverse Domains through World Models." Nature.
2. Locatello et al. (2020). "Object-Centric Learning with Slot Attention." NeurIPS.
3. Kirkpatrick et al. (2017). "Overcoming catastrophic forgetting in neural networks." PNAS.
4. Finn et al. (2017). "Model-Agnostic Meta-Learning." ICML.
5. Friston (2010). "The free-energy principle: a unified brain theory?" Nature Reviews Neuroscience.
6. Goodman & Frank (2016). "Pragmatic Language Interpretation as Probabilistic Inference." Trends in Cognitive Sciences.
7. Scherer (2009). "The dynamic architecture of emotion." Cognition and Emotion.
8. Sacramento et al. (2018). "Dendritic cortical microcircuits approximate the backpropagation algorithm." NeurIPS.

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **EWC** | Elastic Weight Consolidation - prevents forgetting by regularizing important weights |
| **MAML** | Model-Agnostic Meta-Learning - learns initialization for fast adaptation |
| **RSSM** | Recurrent State Space Model - latent dynamics model with deterministic + stochastic components |
| **IRL** | Inverse Reinforcement Learning - infer reward function from demonstrations |
| **RSA** | Rational Speech Acts - probabilistic model of pragmatic language |
| **ZPD** | Zone of Proximal Development - optimal difficulty for learning |
| **ToM** | Theory of Mind - ability to attribute mental states to others |

---

*Document maintained by: NIMCP Development Team*
*Last reviewed: 2026-01-17*

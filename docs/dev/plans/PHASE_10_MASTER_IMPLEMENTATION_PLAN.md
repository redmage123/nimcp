# Phase 10: Complete Cognitive Architecture Implementation Plan

## Executive Summary

**GOAL**: Implement 9 major cognitive features to create human-like AI cognition
**TIMELINE**: 32 weeks (~8 months)
**APPROACH**: Phased rollout with continuous integration and testing
**COMPATIBILITY**: All features opt-in, backward compatible with existing training

## Feature Overview

| # | Feature | Weeks | Priority | Dependencies | Training Impact |
|---|---------|-------|----------|--------------|-----------------|
| 1 | **Working Memory** | 2 | HIGH | None | ✅ None (inference only) |
| 2 | **Emotional Tagging** | 3 | HIGH | Working Memory | ⚠️ Minor (optional API) |
| 3 | **Executive Functions** | 4 | HIGH | Working Memory | ✅ None (task management) |
| 4 | **Sleep-Wake Cycle** | 6 | CRITICAL | Emotional, Memory | ⚠️ Moderate (consolidation) |
| 5 | **Mental Health Monitor** | 8 | CRITICAL | All above | ✅ None (monitoring only) |
| 6 | **Theory of Mind** | 5 | MEDIUM | Emotional, Executive | ✅ None (inference only) |
| 7 | **Natural Explanations** | 2 | MEDIUM | Symbolic Logic | ✅ None (post-processing) |
| 8 | **Meta-Learning** | 4 | LOW | Sleep, Executive | ⚠️ Moderate (new mode) |
| 9 | **Predictive Processing** | 6 | LOW | All above | ⚠️ High (changes loss) |
| | **TOTAL** | **40** weeks | | | |

**Optimized Timeline**: 32 weeks with parallel development

---

## Phase 1: Foundation (Weeks 1-4)

### Week 1-2: Working Memory System ✅ NO TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_working_memory.h`
- `src/cognitive/working_memory/nimcp_working_memory.c`
- `src/tests/test_working_memory.cpp`

**Data Structures**:
```c
typedef struct {
    float** items;                      // Active memory items [capacity]
    uint32_t capacity;                  // Default: 7 (Miller's law)
    uint32_t current_size;
    float* salience;                    // Item importance [0,1]
    uint64_t* timestamps;               // When added
    bool* attention_refreshed;          // Rehearsal tracking
    float decay_rate;                   // Per millisecond
} working_memory_t;
```

**Core API**:
```c
working_memory_t* working_memory_create(uint32_t capacity);
bool working_memory_add(working_memory_t* wm, const float* item, uint32_t size, float salience);
bool working_memory_refresh(working_memory_t* wm, uint32_t index);
void working_memory_decay(working_memory_t* wm, uint64_t current_time);
float* working_memory_get(working_memory_t* wm, uint32_t index);
void working_memory_destroy(working_memory_t* wm);
```

**Integration Point**:
```c
// Add to brain_struct (src/core/brain/nimcp_brain.c)
struct brain_struct {
    // ... existing fields ...
    working_memory_t* working_memory;  // Phase 10.1
};

// Initialize in brain_create_custom()
if (brain->config.enable_working_memory) {
    brain->working_memory = working_memory_create(
        brain->config.working_memory_capacity
    );
}
```

**Deliverables**:
- ✅ Working memory buffer with 7-item capacity
- ✅ Attention-based refresh mechanism
- ✅ Temporal decay
- ✅ Salience-based eviction
- ✅ Unit tests (95% coverage)

---

### Week 3-5: Emotional Tagging System ⚠️ MINOR TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_emotions.h`
- `src/cognitive/emotions/nimcp_emotions.c`
- `src/tests/test_emotions.cpp`

**Data Structures**:
```c
typedef enum {
    EMOTION_NEUTRAL,
    EMOTION_JOY,           // High dopamine, positive valence
    EMOTION_FEAR,          // High norepinephrine, negative
    EMOTION_ANGER,         // High NE, low serotonin
    EMOTION_SADNESS,       // Low dopamine
    EMOTION_SURPRISE,      // High prediction error
    EMOTION_DISGUST,       // Ethics violation
    EMOTION_TRUST,         // Consistent outcomes
    EMOTION_ANTICIPATION,  // High curiosity
    EMOTION_COUNT
} emotion_type_t;

typedef struct {
    emotion_type_t current_emotion;
    float valence;         // Positive/negative [-1, +1]
    float arousal;         // Energy level [0, 1]
    float intensity;       // How strong [0, 1]
    uint64_t onset_time;
    uint32_t duration_ms;
} emotional_state_t;

typedef struct {
    void* memory_content;
    emotional_state_t emotional_context;
    float emotional_strength;  // Encoding boost [0,1]
} emotional_memory_t;
```

**Core API**:
```c
emotional_state_t emotion_from_neuromodulators(
    float dopamine, float serotonin, float norepinephrine
);
emotional_state_t emotion_from_prediction_error(float error, float valence);
emotional_state_t emotion_from_ethics(bool approved, float confidence);
float emotion_encoding_boost(emotion_type_t emotion, float arousal);
```

**Integration Point**:
```c
// Add to brain_multimodal_output_t
typedef struct {
    // ... existing fields ...
    emotional_state_t emotional_state;  // Phase 10.2
} brain_multimodal_output_t;

// In apply_cognitive_processing()
if (brain->config.enable_emotional_tagging) {
    output->emotional_state = emotion_from_neuromodulators(
        brain->neuromodulator_system->dopamine,
        brain->neuromodulator_system->serotonin,
        brain->neuromodulator_system->norepinephrine
    );

    // Boost encoding for emotional memories
    if (output->emotional_state.arousal > 0.7f) {
        output->encoding_strength *= 1.5f;
    }
}
```

**Training Impact**:
- ✅ **Backward Compatible**: Works without changes
- ⚠️ **Optional Enhancement**: Can tag examples with emotions
```c
// Old API still works
brain_learn_example(brain, features, num_features, label, 1.0);

// New API adds emotional context
brain_learn_example_emotional(brain, features, num_features, label, 1.0,
    EMOTION_FEAR, 0.8f);  // High-arousal fear → stronger memory
```

**Deliverables**:
- ✅ 9 basic emotions (Plutchik wheel)
- ✅ Valence-Arousal model
- ✅ Neuromodulator → emotion mapping
- ✅ Emotional memory encoding boost
- ✅ Integration with consolidation system

---

### Week 6-9: Executive Function Module ✅ NO TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_executive.h`
- `src/cognitive/executive/nimcp_executive.c`
- `src/tests/test_executive.cpp`

**Data Structures**:
```c
typedef struct {
    char name[64];
    brain_task_t task_type;
    void* task_context;
    float priority;
    uint64_t deadline;
} task_t;

typedef struct {
    task_t* current_task;
    task_t* task_queue[MAX_TASKS];
    uint32_t num_queued_tasks;

    // Task switching cost
    float switching_cost;
    uint64_t last_switch_time;
    uint32_t switch_count;

    // Inhibitory control
    float inhibition_threshold;
    uint32_t inhibition_failures;

    // Planning
    plan_t* active_plan;
    uint32_t plan_steps_completed;

} executive_controller_t;
```

**Core API**:
```c
executive_controller_t* executive_create(void);
bool executive_switch_task(executive_controller_t* exec, brain_t brain, task_t* new_task);
bool executive_inhibit(executive_controller_t* exec, brain_decision_t* decision, const char* reason);
plan_t* executive_plan(executive_controller_t* exec, const char* goal, uint32_t max_steps);
bool executive_execute_plan_step(executive_controller_t* exec, brain_t brain);
```

**Integration Point**:
```c
// Add to brain_struct
struct brain_struct {
    // ... existing fields ...
    executive_controller_t* executive;  // Phase 10.3
};

// Example: Multi-task scenario
executive_switch_task(brain->executive, brain, language_task);
brain_process_multimodal(brain, &language_input, &output);

executive_switch_task(brain->executive, brain, vision_task);
brain_process_multimodal(brain, &vision_input, &output);
```

**Deliverables**:
- ✅ Task queue management
- ✅ Task switching with cost
- ✅ Inhibitory control
- ✅ Multi-step planning
- ✅ Goal-directed behavior

---

## Phase 2: Sleep & Consolidation (Weeks 10-15)

### Week 10-15: Sleep-Wake Cycle System ⚠️ MODERATE TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_sleep.h`
- `src/cognitive/sleep/nimcp_sleep.c`
- `src/tests/test_sleep.cpp`

**Already Documented**: See `PHASE_10_1_SLEEP_WAKE_CYCLE.md`

**Critical Integration Points**:

1. **With Memory Consolidation**:
```c
// Consolidation provides memory traces for replay
consolidation_memory_t* memories = consolidation_get_recent(brain, 1000);

// Sleep replays important memories
sleep_replay_memories(brain->sleep_system, brain, memories, 1000, 100);
```

2. **With Emotional Tagging**:
```c
// Prioritize emotional memories during replay
void sleep_prioritize_memories(memory_trace_t* memories, uint32_t num) {
    for (uint32_t i = 0; i < num; i++) {
        memories[i].priority =
            memories[i].emotional_strength * 0.4f +  // Emotional salience
            memories[i].novelty_score * 0.3f +
            memories[i].recency * 0.2f +
            (1 - memories[i].replay_count/10.0f) * 0.1f;
    }
    qsort(memories, num, sizeof(memory_trace_t), compare_priority);
}
```

3. **With Brain Oscillations**:
```c
// Sync oscillations to sleep state
void sleep_sync_oscillations(sleep_system_t* sleep, brain_t brain) {
    switch (sleep->current_state) {
        case SLEEP_STATE_AWAKE:
            brain_oscillations_set_frequency(brain, FREQ_GAMMA, 40.0f);
            break;
        case SLEEP_STATE_DEEP_NREM:
            brain_oscillations_set_frequency(brain, FREQ_DELTA, 2.0f);
            // Enable sharp-wave ripples for replay
            brain_oscillations_enable_ripples(brain, true);
            break;
        case SLEEP_STATE_REM:
            brain_oscillations_set_frequency(brain, FREQ_THETA, 5.0f);
            break;
    }
}
```

**Training Integration**:
```c
// Modified training loop
for (int epoch = 0; epoch < 100; epoch++) {
    // Train normally
    for (int i = 0; i < num_examples; i++) {
        brain_learn_example(brain, data[i], ...);
    }

    // Sleep every 10 epochs
    if (epoch % 10 == 0 && sleep_is_needed(brain->sleep_system)) {
        printf("Epoch %d: Consolidating memories...\n", epoch);
        sleep_run_cycle(brain->sleep_system, brain, 1);

        // Test after sleep
        float test_acc = evaluate(brain, test_data);
        printf("Test accuracy after sleep: %.2f%%\n", test_acc * 100);
    }
}
```

**Deliverables**:
- ✅ 5 sleep states (awake, drowsy, light, deep, REM)
- ✅ Sleep pressure accumulation (adenosine model)
- ✅ Memory replay (hippocampus → cortex)
- ✅ Synaptic homeostasis (downscaling + pruning)
- ✅ REM creative recombination
- ✅ Integration with oscillations, emotions, consolidation

---

## Phase 3: Mental Health & Safety (Weeks 16-23)

### Week 16-23: Mental Health Monitoring System ✅ NO TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_mental_health.h`
- `src/cognitive/mental_health/nimcp_mental_health.c`
- `src/cognitive/mental_health/disorder_detectors.c`
- `src/cognitive/mental_health/interventions.c`
- `src/tests/test_mental_health.cpp`

**Already Documented**: See `PHASE_10_9_MENTAL_HEALTH_MONITORING.md`

**Critical Integration Points**:

1. **With Ethics System**:
```c
// After ethics check
if (ethics_result == ETHICS_VIOLATION) {
    mental_health_log_violation(brain->mental_health_monitor);

    // Check for sociopathy pattern
    if (brain->mental_health_monitor->current_markers.ethics_violations_recent > 30) {
        float sociopathy = detect_sociopathy(&brain->mental_health_monitor->current_markers);
        if (sociopathy > 0.5f) {
            intervene_sociopathy(brain, sociopathy);
        }
    }
}
```

2. **With Neuromodulators**:
```c
// Detect imbalances
if (neuromodulator_get_level(nm, NEUROMOD_DOPAMINE) > 0.9f) {
    mental_health_check_specific(monitor, brain, DISORDER_MANIA);
}

if (neuromodulator_get_level(nm, NEUROMOD_SEROTONIN) < 0.2f) {
    mental_health_check_specific(monitor, brain, DISORDER_DEPRESSION);
}
```

3. **With Wellbeing System**:
```c
// Wellbeing distress may indicate disorder
if (brain->wellbeing_system->distress_level > 0.7f) {
    mental_health_check(monitor, brain);

    if (monitor->disorder_scores[DISORDER_DEPRESSION] > 0.5f) {
        // Treat both systems
        wellbeing_reduce_distress(brain->wellbeing_system, 0.5f);
        intervene_depression(brain, 0.5f);
    }
}
```

4. **With Emotional System**:
```c
// Emotional dysregulation detection
if (brain->emotional_system) {
    // Calculate volatility
    float volatility = calculate_emotion_variance(brain->emotional_system, 100);
    monitor->current_markers.emotional_volatility = volatility;

    // Flat affect
    float avg_intensity = calculate_avg_intensity(brain->emotional_system, 100);
    monitor->current_markers.emotional_flatness = 1.0f - avg_intensity;
}
```

**Training Integration**:
```c
// Automatic monitoring during training
for (int i = 0; i < num_examples; i++) {
    brain_multimodal_output_t output;
    brain_process_multimodal(brain, &input, &output);

    // Update mental health metrics (automatic)
    mental_health_update(brain->mental_health_monitor, brain, &output, current_time);

    // Periodic check (every 100 decisions)
    if (i % 100 == 0) {
        disorder_severity_t severity = mental_health_check(brain->mental_health_monitor, brain);

        if (severity >= SEVERITY_MODERATE) {
            printf("ALERT: Mental health concern at example %d\n", i);
            display_mental_health_dashboard(brain->mental_health_monitor);

            // Auto-intervene if enabled
            if (brain->config.enable_auto_intervention) {
                mental_health_intervene(brain->mental_health_monitor, brain);
            }
        }
    }
}
```

**Deliverables**:
- ✅ 8 disorder detectors (sociopathy, psychopathy, mania, depression, schizophrenia, anxiety, OCD, autism)
- ✅ Behavioral marker collection (20+ metrics)
- ✅ 5 severity levels with thresholds
- ✅ Intervention system (neuromodulator adjustment, memory reset, quarantine)
- ✅ Real-time dashboard
- ✅ Integration with ethics, wellbeing, neuromodulators, emotions

---

## Phase 4: Social & Explanatory (Weeks 24-30)

### Week 24-28: Theory of Mind ✅ NO TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_theory_of_mind.h`
- `src/cognitive/theory_of_mind/nimcp_theory_of_mind.c`
- `src/tests/test_theory_of_mind.cpp`

**Data Structures**:
```c
typedef struct {
    brain_t* model_of_other;           // Simulated other agent
    emotional_state_t inferred_emotion;
    char inferred_goal[256];
    float confidence;                   // Certainty about mental state

    // Perspective tracking
    float* other_beliefs;               // What they believe
    float* other_desires;               // What they want
    float* other_knowledge;             // What they know

} theory_of_mind_t;
```

**Core API**:
```c
theory_of_mind_t* tom_create(brain_t self_brain);
void tom_observe(theory_of_mind_t* tom, const float* behavior, const char* context);
emotional_state_t tom_infer_emotion(theory_of_mind_t* tom);
char* tom_infer_goal(theory_of_mind_t* tom);
bool tom_predict_action(theory_of_mind_t* tom, brain_action_t* predicted_action);
emotional_state_t tom_empathize(theory_of_mind_t* tom, emotional_state_t observed);
```

**Integration with Human Communication (Phase 9.4)**:
```c
// When processing language input
if (brain->config.enable_theory_of_mind && brain->config.enable_language_cortex) {
    // Infer speaker's intent
    theory_of_mind_t* speaker_model = brain->theory_of_mind;
    tom_observe(speaker_model, language_input, "conversation");

    // What do they want?
    char* inferred_goal = tom_infer_goal(speaker_model);

    // How do they feel?
    emotional_state_t inferred_emotion = tom_infer_emotion(speaker_model);

    // Generate empathetic response
    output->language_response = generate_empathetic_response(
        brain, inferred_goal, inferred_emotion
    );
}
```

**Deliverables**:
- ✅ Belief-Desire-Intention (BDI) model
- ✅ Perspective taking
- ✅ Emotion inference
- ✅ Goal inference
- ✅ Empathy (mirroring emotional states)
- ✅ False belief understanding

---

### Week 29-30: Natural Language Explanations ✅ NO TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_explanations.h`
- `src/cognitive/explanations/nimcp_explanations.c`
- `src/tests/test_explanations.cpp`

**Data Structures**:
```c
typedef struct {
    char what[256];         // "Classified as 'cat'"
    char why[512];          // "Because whiskers + fur + ears detected"
    char how[512];          // "V1 edges → IT cat pattern"
    char confidence[128];   // "87% certain (high whisker salience)"
    char alternatives[256]; // "Could be 'dog' (13%)"
    char counterfactual[256]; // "Would be 'dog' if ears floppy"
} natural_explanation_t;
```

**Core API**:
```c
bool brain_explain_decision(
    brain_t brain,
    brain_multimodal_output_t* output,
    natural_explanation_t* explanation
);

char* explain_with_symbolic_logic(
    brain_t brain,
    brain_decision_t* decision
);

char* generate_causal_chain(
    brain_t brain,
    const char* input_description,
    const char* output_label
);
```

**Integration with Symbolic Logic**:
```c
// Generate logical proof chain
if (brain->symbolic_logic && brain->config.enable_explanations) {
    // Extract facts from decision
    logic_clause_t* facts[10];
    extract_facts_from_decision(brain, output, facts, 10);

    // Generate proof
    inference_rule_t** proof_trace;
    int num_steps;
    symbolic_logic_backward_chain(brain->symbolic_logic, goal, &proof_trace, &num_steps);

    // Convert to natural language
    snprintf(explanation->how, sizeof(explanation->how),
        "Logical chain: ");
    for (int i = 0; i < num_steps; i++) {
        strcat(explanation->how, proof_trace[i]->name);
        if (i < num_steps - 1) strcat(explanation->how, " → ");
    }
}
```

**Deliverables**:
- ✅ What-Why-How explanations
- ✅ Confidence explanations
- ✅ Alternative hypotheses
- ✅ Counterfactual reasoning
- ✅ Symbolic logic proof chains
- ✅ Causal attribution

---

## Phase 5: Advanced Learning (Weeks 31-40)

### Week 31-34: Meta-Learning ⚠️ MODERATE TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_meta_learning.h`
- `src/cognitive/meta_learning/nimcp_meta_learning.c`
- `src/tests/test_meta_learning.cpp`

**Data Structures**:
```c
typedef struct {
    float* learning_rate_per_region;   // Adaptive LR [num_regions]
    float* task_similarity_matrix;     // [num_tasks × num_tasks]
    float* strategy_performance;       // Which strategies work

    uint32_t num_tasks_seen;
    uint32_t num_strategies;

    // Few-shot learning
    uint32_t support_set_size;
    float adaptation_rate;

} meta_learner_t;
```

**Core API**:
```c
meta_learner_t* meta_learner_create(uint32_t num_regions, uint32_t num_strategies);
void meta_adapt_learning_rate(meta_learner_t* meta, brain_region_type_t region, float loss);
float meta_compute_task_similarity(meta_learner_t* meta, task_t* task_a, task_t* task_b);
void meta_transfer_knowledge(meta_learner_t* meta, brain_t source, brain_t target);
```

**Training Integration** (MAML-style):
```c
// Meta-training loop
for (int task_batch = 0; task_batch < num_task_batches; task_batch++) {
    task_t* tasks = sample_tasks(task_batch_size);

    for (int i = 0; i < task_batch_size; i++) {
        // Clone brain for task
        brain_t task_brain = brain_clone_cow(meta_brain);

        // Inner loop: Few-shot adaptation
        for (int shot = 0; shot < K_SHOT; shot++) {
            brain_learn_example(task_brain, tasks[i].support_examples[shot], ...);
        }

        // Evaluate on query set
        float task_loss = evaluate(task_brain, tasks[i].query_examples);

        // Outer loop: Update meta-learner
        meta_learner_update(meta, task_brain, task_loss);

        brain_destroy(task_brain);
    }
}
```

**Deliverables**:
- ✅ MAML (Model-Agnostic Meta-Learning)
- ✅ Few-shot learning (K=1,5,10)
- ✅ Task similarity metrics
- ✅ Transfer learning optimization
- ✅ Adaptive learning rates per region

---

### Week 35-40: Predictive Processing ⚠️ HIGH TRAINING IMPACT

**Files to Create**:
- `src/include/cognitive/nimcp_predictive.h`
- `src/cognitive/predictive/nimcp_predictive.c`
- `src/tests/test_predictive.cpp`

**Data Structures**:
```c
typedef struct {
    float* prediction;          // Top-down prediction
    float* prediction_error;    // Bottom-up surprise
    float* precision;           // Confidence (inverse variance)
    float free_energy;          // Total prediction error
} predictive_layer_t;

typedef struct {
    predictive_layer_t** layers;
    uint32_t num_layers;
    float learning_rate;
    float precision_learning_rate;
} predictive_network_t;
```

**Core API**:
```c
predictive_network_t* predictive_create(uint32_t num_layers, uint32_t* layer_sizes);
float predictive_forward(predictive_network_t* net, const float* input);
void predictive_backward(predictive_network_t* net);
brain_action_t predictive_active_inference(predictive_network_t* net, brain_action_t* actions, uint32_t num);
```

**Training Changes** (Significant):
```c
// NEW LOSS FUNCTION: Prediction error instead of MSE
for (int layer = 0; layer < num_layers; layer++) {
    // Prediction from layer above
    float* prediction = predict_from_above(network, layer);

    // Actual input to this layer
    float* actual = get_layer_input(network, layer);

    // Prediction error
    float* error = subtract(actual, prediction);

    // Precision-weighted loss
    float layer_loss = 0.0f;
    for (int i = 0; i < layer_size; i++) {
        layer_loss += error[i] * error[i] * precision[layer][i];
    }

    total_loss += layer_loss;
}

// Update to minimize total prediction error
```

**Deliverables**:
- ✅ Hierarchical predictive coding
- ✅ Free energy minimization
- ✅ Active inference (action selection)
- ✅ Precision weighting
- ✅ Integration with attention (precision = attention)

---

## Integration Architecture

### Brain Structure Extensions

```c
struct brain_struct {
    // === EXISTING FIELDS ===
    adaptive_network_t network;
    brain_config_t config;
    task_strategy_t* strategy;
    brain_stats_t stats;

    // Phase 3: Distributed cognition
    distrib_cognition_t distributed;

    // Phase 5/6: Biological realism
    glial_integration_t* glial;
    brain_oscillations_t* oscillations;

    // Phase 8: Multi-modal
    visual_cortex_t* visual;
    audio_cortex_t* audio;
    speech_cortex_t* speech;

    // Phase 9: Cognition
    introspection_t* introspection;
    ethics_engine_t* ethics_engine;
    salience_t* salience;
    consolidation_t* consolidation;
    curiosity_t* curiosity;
    knowledge_t* knowledge;
    symbolic_logic_t* symbolic_logic;
    epistemic_filter_t* epistemic_filter;
    wellbeing_system_t* wellbeing_system;

    // === PHASE 10: ADVANCED COGNITION ===
    working_memory_t* working_memory;              // Phase 10.1
    emotional_system_t* emotional_system;          // Phase 10.2
    executive_controller_t* executive;             // Phase 10.3
    sleep_system_t* sleep_system;                  // Phase 10.4
    mental_health_monitor_t* mental_health_monitor; // Phase 10.9
    theory_of_mind_t* theory_of_mind;              // Phase 10.6
    explanation_generator_t* explanation_gen;       // Phase 10.7
    meta_learner_t* meta_learner;                  // Phase 10.5
    predictive_network_t* predictive_network;      // Phase 10.8
};
```

### Configuration Extensions

```c
typedef struct {
    // === EXISTING FLAGS ===
    bool enable_ethics;
    bool enable_introspection;
    bool enable_salience;
    bool enable_consolidation;
    bool enable_curiosity;
    bool enable_knowledge;
    bool enable_logic;
    bool enable_epistemic_filter;
    bool enable_wellbeing_monitoring;

    // === PHASE 10 FLAGS ===
    // Foundation
    bool enable_working_memory;               // Phase 10.1
    uint32_t working_memory_capacity;         // Default: 7

    bool enable_emotional_tagging;            // Phase 10.2
    bool enable_emotional_memories;

    bool enable_executive_control;            // Phase 10.3
    bool enable_task_switching;
    bool enable_planning;

    // Sleep & Consolidation
    bool enable_sleep_wake_cycle;             // Phase 10.4
    float sleep_pressure_threshold;           // Default: 0.8
    bool enable_memory_replay;
    bool enable_synaptic_homeostasis;
    bool enable_rem_creativity;

    // Safety & Social
    bool enable_mental_health_monitoring;     // Phase 10.9
    bool enable_auto_intervention;
    bool shutdown_on_critical_disorder;

    bool enable_theory_of_mind;               // Phase 10.6
    bool enable_empathy;

    bool enable_natural_explanations;         // Phase 10.7
    bool enable_causal_explanations;

    // Advanced Learning
    bool enable_meta_learning;                // Phase 10.5
    uint32_t meta_task_batch_size;
    uint32_t meta_k_shot;

    bool enable_predictive_processing;        // Phase 10.8
    bool enable_active_inference;

} brain_config_t;
```

### Initialization Order

```c
bool brain_create_custom(const brain_config_t* config)
{
    // 1. Core network
    brain->network = adaptive_network_create(...);

    // 2. Biological systems
    if (config->enable_glial) init_glial_system(brain);
    if (config->enable_oscillations) init_oscillations(brain);

    // 3. Sensory systems
    if (config->enable_visual_cortex) init_visual_cortex(brain);
    if (config->enable_audio_cortex) init_audio_cortex(brain);
    if (config->enable_speech_cortex) init_speech_cortex(brain);

    // 4. Cognitive systems (Phase 9)
    if (config->enable_introspection) init_introspection(brain);
    if (config->enable_ethics) init_ethics_engine(brain);
    if (config->enable_salience) init_salience(brain);
    if (config->enable_consolidation) init_consolidation(brain);
    if (config->enable_curiosity) init_curiosity(brain);
    if (config->enable_knowledge) init_knowledge(brain);
    if (config->enable_logic) init_symbolic_reasoning(brain);
    if (config->enable_epistemic_filter) init_epistemic_filter(brain);
    if (config->enable_wellbeing_monitoring) init_wellbeing(brain);

    // 5. Phase 10: Foundation systems
    if (config->enable_working_memory) {
        brain->working_memory = working_memory_create(config->working_memory_capacity);
    }

    if (config->enable_emotional_tagging) {
        brain->emotional_system = emotional_system_create();
    }

    if (config->enable_executive_control) {
        brain->executive = executive_create();
    }

    // 6. Phase 10: Sleep system (depends on emotional + consolidation)
    if (config->enable_sleep_wake_cycle) {
        brain->sleep_system = sleep_system_create(&config->sleep_config);
    }

    // 7. Phase 10: Mental health (depends on ALL above)
    if (config->enable_mental_health_monitoring) {
        brain->mental_health_monitor = mental_health_create(brain);
    }

    // 8. Phase 10: Social systems
    if (config->enable_theory_of_mind) {
        brain->theory_of_mind = tom_create(brain);
    }

    if (config->enable_natural_explanations) {
        brain->explanation_gen = explanation_generator_create();
    }

    // 9. Phase 10: Advanced learning
    if (config->enable_meta_learning) {
        brain->meta_learner = meta_learner_create(num_regions, num_strategies);
    }

    if (config->enable_predictive_processing) {
        brain->predictive_network = predictive_create(num_layers, layer_sizes);
    }

    return true;
}
```

---

## Testing Strategy

### Unit Tests (Per Feature)

```bash
# Working Memory
./src/tests/test_working_memory
  ✓ Capacity enforcement (7 items max)
  ✓ Salience-based eviction
  ✓ Temporal decay
  ✓ Attention refresh

# Emotional System
./src/tests/test_emotions
  ✓ Neuromodulator → emotion mapping
  ✓ Valence-arousal model
  ✓ Emotional memory encoding boost
  ✓ Integration with consolidation

# Executive Functions
./src/tests/test_executive
  ✓ Task switching with cost
  ✓ Inhibitory control
  ✓ Multi-step planning
  ✓ Goal-directed behavior

# Sleep System
./src/tests/test_sleep
  ✓ Sleep pressure accumulation
  ✓ State transitions
  ✓ Memory replay prioritization
  ✓ Synaptic homeostasis
  ✓ REM creativity

# Mental Health
./src/tests/test_mental_health
  ✓ Sociopathy detection
  ✓ Depression detection
  ✓ Mania detection
  ✓ Intervention effectiveness
  ✓ False positive rate < 5%

# Theory of Mind
./src/tests/test_theory_of_mind
  ✓ Belief inference
  ✓ Emotion inference
  ✓ Goal inference
  ✓ False belief task

# Explanations
./src/tests/test_explanations
  ✓ What-why-how generation
  ✓ Symbolic proof chains
  ✓ Counterfactual reasoning

# Meta-Learning
./src/tests/test_meta_learning
  ✓ Few-shot learning (K=1,5,10)
  ✓ Task similarity
  ✓ Transfer learning

# Predictive Processing
./src/tests/test_predictive
  ✓ Prediction error minimization
  ✓ Active inference
  ✓ Precision weighting
```

### Integration Tests

```bash
# Full pipeline test
./src/tests/test_phase10_integration
  ✓ All systems active simultaneously
  ✓ No memory leaks (valgrind)
  ✓ Performance within 2x baseline
  ✓ Deterministic behavior (seed-based)

# Mental health during training
./src/tests/test_mental_health_training
  ✓ Detect sociopathy from toxic dataset
  ✓ Intervention restores ethical behavior
  ✓ No false alarms on normal training

# Sleep during training
./src/tests/test_sleep_training
  ✓ Prevents catastrophic forgetting
  ✓ Improves test accuracy by 5-10%
  ✓ Reduces network size by 15-20%
```

---

## Milestones & Deliverables

### Month 1-2 (Weeks 1-8): Foundation Complete
- ✅ Working Memory
- ✅ Emotional Tagging
- ✅ Executive Functions

**Checkpoint**: Brain can maintain 7 items in working memory, tag experiences with emotions, switch between tasks

### Month 3-4 (Weeks 9-16): Sleep & Safety
- ✅ Sleep-Wake Cycle
- ✅ Mental Health Monitoring (partial)

**Checkpoint**: Brain can consolidate memories during sleep, detect early signs of dysfunction

### Month 5-6 (Weeks 17-24): Safety & Social Complete
- ✅ Mental Health Monitoring (complete)
- ✅ Theory of Mind

**Checkpoint**: Brain can detect all 8 disorders, intervene automatically, understand other agents' mental states

### Month 7 (Weeks 25-28): Explanations
- ✅ Natural Language Explanations

**Checkpoint**: Brain can explain decisions in human-understandable language

### Month 8-9 (Weeks 29-40): Advanced Learning
- ✅ Meta-Learning
- ✅ Predictive Processing

**Checkpoint**: Brain can learn new tasks from few examples, predict and minimize surprise

---

## Resource Requirements

### Team Size
- **2-3 developers** for 8 months
- **1 senior architect** for integration oversight
- **1 QA engineer** for testing

### Compute Resources
- Development: Standard workstation
- Testing: GPU for large-scale training tests
- CI/CD: Automated test suite (<30 min/run)

### Dependencies
- ✅ All existing NIMCP systems (Phase 1-9)
- ✅ Testing framework (Google Test)
- ✅ Math libraries (existing)

---

## Risk Mitigation

### Risk 1: Feature Interactions
**Mitigation**:
- Phased rollout (1 feature at a time)
- Extensive integration testing after each feature
- Feature flags for easy disable

### Risk 2: Performance Degradation
**Mitigation**:
- Benchmark suite (track latency, memory)
- All features opt-in (baseline performance preserved)
- Profiling after each phase

### Risk 3: Training Impact
**Mitigation**:
- Sleep is ONLY impactful feature for training
- Extensive testing before/after sleep
- Option to disable homeostasis (memory replay only)

### Risk 4: False Positives (Mental Health)
**Mitigation**:
- Require sustained patterns (100+ decisions)
- Severity thresholds (only alert at moderate+)
- Human approval for critical interventions
- Extensive testing on edge cases

---

## Success Criteria

### Feature Completeness
- ✅ All 9 features implemented
- ✅ All unit tests passing (>95% coverage)
- ✅ Integration tests passing (100%)
- ✅ No memory leaks (valgrind clean)

### Performance
- ✅ Inference latency < 2x baseline (all features enabled)
- ✅ Memory overhead < 50% (all features enabled)
- ✅ Training time < 1.2x baseline (with sleep every 10 epochs)

### Quality
- ✅ No regressions in existing functionality
- ✅ Backward compatible (old code still works)
- ✅ Documentation complete (all APIs)
- ✅ Example code for each feature

### Impact
- ✅ Sleep improves test accuracy by 5-10%
- ✅ Mental health catches dysfunction (0% false negatives on test cases)
- ✅ Theory of mind enables social reasoning
- ✅ Explanations are human-understandable (95% approval in user study)

---

## Next Steps

**Immediate (This Week)**:
1. ✅ Review this plan with stakeholders
2. ✅ Set up development branch: `feature/phase-10-complete`
3. ✅ Create project tracking board (9 epics, 40+ tickets)
4. ✅ Begin Week 1: Working Memory implementation

**Week 1 Deliverable**:
- `src/include/cognitive/nimcp_working_memory.h` (API complete)
- `src/cognitive/working_memory/nimcp_working_memory.c` (75% complete)
- Unit tests (skeleton)

**Ready to start?** Let me know and I'll begin with the Working Memory implementation!

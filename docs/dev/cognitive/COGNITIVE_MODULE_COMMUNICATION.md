# Cognitive Module Communication & Output Generation

**Date:** 2025-11-11
**Status:** ✅ COMPLETE ANALYSIS
**Question:** "How do cognitive modules talk to each other and how does the output get generated and passed to the calling app?"

---

## Executive Summary

The NIMCP brain uses **three primary communication mechanisms** between cognitive modules:

1. **Global Workspace** - Winner-take-all competition for conscious access (broadcast architecture)
2. **Working Memory** - Shared buffer with salience-based storage and retrieval
3. **Bidirectional Connections** - 4 strategic module pairs with direct feedback

Output is generated through **two main pathways**:
- **brain_decide()** → Returns `brain_decision_t` (simple decisions)
- **brain_process_multimodal()** → Returns `brain_multimodal_output_t` (comprehensive cognitive output)

All outputs are **deep-copied** before returning to caller to prevent double-free issues.

---

## Communication Mechanisms

### 1. Global Workspace - Broadcast Architecture

**Location:** `src/modules/global_workspace/nimcp_global_workspace.h/c`
**Pattern:** Winner-take-all competition
**Biological Model:** Baars (1988) Global Workspace Theory

**How It Works:**
```c
// Stage 6.5 in brain_decide() (line 3891-3940)

// Modules compete with salience scores
bool won_competition = global_workspace_compete(
    brain->global_workspace,
    MODULE_WORKING_MEMORY,           // Competing module
    features,                         // Content to broadcast
    num_features,
    salience_score                    // Competitive strength
);

if (won_competition) {
    // Winner gets conscious access and broadcasts globally
    // All subscribed modules receive the broadcast
    strncat(decision->explanation, " [CONSCIOUS]", ...);
}
```

**Modules That Compete:**
- Working Memory
- Visual Cortex
- Audio Cortex
- Language Cortex
- Introspection
- Curiosity
- Theory of Mind

**Communication Flow:**
```
Module → Compute salience → Compete in workspace → Winner broadcasts → All subscribers receive
```

**Output Fields Populated:**
```c
output->has_workspace_broadcast = true;
output->workspace_source_module = winning_module_id;
output->workspace_broadcast_strength = salience;
output->workspace_num_competitors = N;
```

---

### 2. Working Memory - Shared Buffer

**Location:** `src/modules/working_memory/nimcp_working_memory.h/c`
**Pattern:** Capacity-limited FIFO with salience-based retrieval
**Biological Model:** Baddeley & Hitch (1974) Working Memory Model

**How It Works:**
```c
// Stage 6 in brain_decide() (line 3854-3883)

// Compute salience from novelty, prediction error, confidence
float salience = 0.5f;
if (is_novel) salience += 0.2f;
if (prediction_error > 0.5f) salience += 0.2f;
if (decision->confidence > 0.8f) salience += 0.1f;

// Store decision context with salience
working_memory_add(
    brain->working_memory,
    features,
    num_features,
    salience                  // Determines priority in buffer
);
```

**Enhanced: Emotional Tagging (Phase 10.3)**
```c
// Stage 7 in brain_decide() + Phase 10.3 in brain_process_multimodal()
// Emotional events get memory priority (amygdala-hippocampus interaction)

emotional_tag_t emotion = emotional_tag_create(valence, arousal, timestamp);

working_memory_add_with_emotion(
    brain->working_memory,
    network_output,
    network_output_size,
    salience,           // Base salience
    &emotion            // Emotional boost (increases effective priority)
);
```

**Temporal Decay:**
```c
// Phase 10.2: Exponential decay over time
working_memory_decay(brain->working_memory, current_time_ms);
```

**Modules That Use Working Memory:**
- **Write:** All modules store salient information
- **Read:** Executive Function, Theory of Mind, Consolidation, Language

**Communication Flow:**
```
Module → Compute salience → Store in WM → Other modules retrieve by salience → Use for reasoning
```

**Output Fields Populated:**
```c
output->working_memory_items = count;
output->working_memory_utilization = percentage;
output->top_wm_item_description = "...";
```

---

### 3. Bidirectional Connections - Strategic Feedback Loops

**Location:** Stage 7.5 in `brain_decide()` (line 3996-4091)
**Pattern:** Direct module-to-module feedback
**Phase:** 10.11.3 (Cognitive Integration)

#### Connection 1: Curiosity ↔ Executive Function

**Purpose:** Balance exploration vs. exploitation based on cognitive load

```c
// Executive → Curiosity: High load reduces exploration
float cognitive_load = executive_get_cognitive_load(brain->executive);
if (cognitive_load > 0.8f) {
    // Brain is busy, reduce exploration
    curiosity_set_exploration_rate(
        brain->curiosity,
        1.0f - cognitive_load           // 80% load → 20% exploration
    );
}

// Curiosity → Executive: Information gain boosts exploration priority
float information_gain = curiosity_get_information_gain(brain->curiosity);
if (information_gain > 0.7f) {
    // High uncertainty, boost exploration task priority
    executive_boost_task_priority(
        brain->executive,
        "exploration",
        information_gain * 0.3f         // Up to 30% boost
    );
}
```

**Modules Involved:** Curiosity Engine, Executive Function
**Biological Basis:** Prefrontal cortex ↔ Dopaminergic system

---

#### Connection 2: Mirror Neurons ↔ Visual Cortex

**Purpose:** Social attention and agent detection

```c
// Mirror Neurons → Visual: Detect social signals
bool is_social_stimulus = mirror_neurons_detect_social_cues(
    brain->mirror_neurons,
    features,
    num_features
);

if (is_social_stimulus) {
    // Boost visual attention for social processing
    visual_cortex_boost_attention(
        brain->visual_cortex,
        0.3f                            // 30% attention boost for faces/gestures
    );
}

// Visual → Mirror: Detect agent presence
bool agent_detected = visual_cortex_detect_agent(
    brain->visual_cortex,
    features,
    num_features
);

if (agent_detected) {
    // Activate mirror neuron simulation
    mirror_neurons_activate_simulation_mode(brain->mirror_neurons);
}
```

**Modules Involved:** Mirror Neurons, Visual Cortex
**Biological Basis:** Superior temporal sulcus (STS) ↔ Inferior frontal gyrus (IFG)

---

#### Connection 3: Emotional System ↔ Salience

**Purpose:** Mood-based attention biasing

```c
// Emotional → Salience: Mood biases attention
float current_mood = emotional_get_valence(brain->emotional);
if (current_mood < -0.5f) {
    // Negative mood: Bias toward threat detection
    salience_set_threat_bias(brain->salience, fabsf(current_mood));
} else if (current_mood > 0.5f) {
    // Positive mood: Bias toward reward
    salience_set_reward_bias(brain->salience, current_mood);
}

// Salience → Emotional: Surprise modulates arousal
float surprise = salience_get_surprise(brain->salience);
if (surprise > 0.7f) {
    // High surprise increases arousal
    emotional_modulate_arousal(brain->emotional, surprise * 0.5f);
}
```

**Modules Involved:** Emotional System, Salience Network
**Biological Basis:** Amygdala ↔ Anterior cingulate cortex (ACC)

---

#### Connection 4: Audio ↔ Speech Cortex

**Purpose:** Speech detection and phoneme processing

```c
// Audio → Speech: Detect speech in audio stream
float speech_salience = audio_cortex_get_speech_salience(
    brain->audio_cortex,
    features,
    num_features
);

if (speech_salience > 0.6f) {
    // Activate speech processing mode
    audio_cortex_activate_speech_mode(brain->audio_cortex);
}

// Speech → Audio: Request frequency boost for unclear phonemes
float phoneme_confidence = speech_cortex_get_phoneme_confidence(brain->speech_cortex);
if (phoneme_confidence < 0.7f) {
    // Request mel filterbank emphasis for target frequencies
    float target_freq = 0.0f;
    float bandwidth = 0.0f;
    speech_cortex_request_frequency_boost(
        brain->speech_cortex,
        &target_freq,
        &bandwidth
    );
    // (Full implementation would pass to audio cortex to adjust mel filterbank)
}
```

**Modules Involved:** Audio Cortex, Speech Cortex
**Biological Basis:** Primary auditory cortex (A1) ↔ Wernicke's area

---

## Output Generation Pathways

### Pathway 1: brain_decide() - Simple Decision

**Function:** `brain_decide(brain_t brain, const float* features, uint32_t num_features)`
**Location:** `src/core/brain/nimcp_brain.c:3527-4356`
**Returns:** `brain_decision_t*` (caller must free with `brain_free_decision()`)

#### Complete Pipeline (11+ Stages):

**STAGE 0: Pre-Processing - Wellbeing Monitoring** (line 3548-3567)
```c
// Check for distress before decision-making
if (brain->wellbeing_monitoring_enabled && brain->introspection) {
    distress_assessment_t distress = wellbeing_assess_distress(brain->introspection);
    if (distress.severity == SEVERITY_CRITICAL) {
        return NULL;  // Circuit breaker - refuse to decide
    }
}
```
**Modules Involved:** Introspection, Wellbeing Monitor
**Output Impact:** May block decision entirely

---

**STAGE 0.5: Sleep/Wake Cycle Integration** (line 3612-3649)
```c
sleep_state_t sleep_state = sleep_wake_get_state(brain->sleep_wake);

switch (sleep_state) {
    case SLEEP_STATE_DEEP_NREM:
        sleep_confidence_multiplier = 0.3f;  // 70% confidence reduction
        trigger_consolidation = true;         // Memory consolidation during sleep
        break;
    case SLEEP_STATE_REM:
        sleep_confidence_multiplier = 0.6f;   // 40% reduction
        sleep_noise_level = 0.1f;             // Creative noise injection
        break;
    case SLEEP_STATE_AWAKE:
        sleep_confidence_multiplier = 1.0f;   // Full confidence
        break;
}
```
**Modules Involved:** Sleep/Wake Cycle
**Output Impact:** Modulates confidence, triggers consolidation, adds creativity noise

---

**STAGE 0.6: Curiosity Engine Integration** (line 3659-3685)
```c
// Evaluate input novelty
float novelty_score = input_variance;  // Or from salience module
bool is_novel = (novelty_score > 0.5f);

if (is_novel && brain->curiosity) {
    // Boost learning rate for novel inputs
    curiosity_on_novel_stimulus(brain->curiosity, features, num_features);
}
```
**Modules Involved:** Curiosity Engine
**Output Impact:** Affects learning rate, boosts salience for novel inputs

---

**STAGE 1: Predictive Processing** (line 3693-3703)
```c
// Generate top-down prediction
if (brain->predictive_network) {
    predictive_forward(brain->predictive_network, features, num_features);
    predictive_get_layer_prediction(brain->predictive_network, 0, prediction);
}
```
**Modules Involved:** Predictive Processing Network
**Output Impact:** Enables active inference, prediction error computation

---

**STAGE 2: Prediction Error Computation** (line 3714-3728)
```c
// Compute mismatch between prediction and actual
float prediction_error = 0.0f;
for (uint32_t i = 0; i < num_features; i++) {
    float diff = features[i] - prediction[i];
    prediction_error += diff * diff;
}
prediction_error = sqrtf(prediction_error / num_features);

// Update predictive model
if (brain->config.enable_active_inference) {
    predictive_update_error(brain->predictive_network, prediction_error);
}
```
**Modules Involved:** Predictive Network
**Output Impact:** Used in salience computation, working memory priority

---

**STAGE 3.5: Apply Sleep-Induced Noise** (line 3739-3746)
```c
// REM sleep creativity via noise injection
if (sleep_noise_level > 0.0f) {
    for (uint32_t i = 0; i < num_features; i++) {
        features[i] += random_gaussian(0.0f, sleep_noise_level);
    }
}
```
**Modules Involved:** Sleep/Wake Cycle
**Output Impact:** Increases exploration, creative associations

---

**STAGE 4: Sleep Cognitive Degradation** (line 3757)
```c
// Reduce confidence based on sleep state
decision->confidence *= sleep_confidence_multiplier;
```
**Modules Involved:** Sleep/Wake Cycle
**Output Impact:** Reduces confidence during sleep states

---

**STAGE 4.2: Memory Consolidation Trigger** (line 3765-3779)
```c
// Transfer working memory to long-term during deep sleep
if (trigger_consolidation && brain->consolidation) {
    consolidation_trigger_offline(
        brain->consolidation,
        brain->working_memory,
        brain->knowledge
    );
}
```
**Modules Involved:** Consolidation, Working Memory, Knowledge
**Output Impact:** Transfers volatile memories to stable storage

---

**STAGE 4.5: Executive Controller Integration** (line 3787-3811)
```c
// Apply executive control (inhibition, planning)
if (decision->confidence < 0.3f) {
    bool should_inhibit = executive_should_inhibit(
        brain->executive,
        decision->confidence,
        "low confidence"
    );
    if (should_inhibit) {
        decision->confidence = 0.0f;  // Block low-confidence decision
    }
}

// Planning: Check if decision requires multi-step planning
if (brain->config.enable_planning) {
    executive_consider_planning(brain->executive, decision);
}
```
**Modules Involved:** Executive Function
**Output Impact:** May block decisions, trigger planning sequences

---

**STAGE 5: Natural Explanations** (line 3822-3846)
```c
// Generate what-why-how explanations
if (brain->explanation_gen) {
    natural_explanation_t nat_exp;
    explanation_generate(
        brain->explanation_gen,
        features,
        num_features,
        decision->output_vector,
        decision->output_size,
        &nat_exp
    );

    snprintf(decision->explanation, sizeof(decision->explanation),
             "WHAT: %s | WHY: %s | HOW: %s",
             nat_exp.what, nat_exp.why, nat_exp.how);
}
```
**Modules Involved:** Natural Explanations
**Output Impact:** Populates human-readable explanation field

---

**STAGE 6: Working Memory Integration** (line 3854-3883)
```c
// Store decision context with salience
float salience = 0.5f;
if (is_novel) salience += 0.2f;
if (prediction_error > 0.5f) salience += 0.2f;
if (decision->confidence > 0.8f) salience += 0.1f;

working_memory_add(
    brain->working_memory,
    features,
    num_features,
    salience
);
```
**Modules Involved:** Working Memory
**Output Impact:** Stores context for future retrieval

---

**STAGE 6.5: Global Workspace Competition** (line 3891-3940)
```c
// Modules compete for conscious access
bool won_competition = global_workspace_compete(
    brain->global_workspace,
    MODULE_WORKING_MEMORY,
    features,
    num_features,
    prediction_error + (is_novel ? 0.3f : 0.0f)
);

if (won_competition) {
    // Winner broadcasts to all subscribed modules
    strncat(decision->explanation, " [CONSCIOUS]", ...);
}
```
**Modules Involved:** Global Workspace, All Subscribed Modules
**Output Impact:** Marks conscious decisions with `[CONSCIOUS]` tag

---

**STAGE 7: Emotional Tagging** (line 3948-3987)
```c
// Tag decision with emotional valence/arousal
float valence = (decision->confidence - 0.5f) * 2.0f;  // [-1, 1]
float arousal = prediction_error;                      // [0, 1]

emotional_tag_t emotion = emotional_tag_create(
    valence,
    arousal,
    timestamp
);

// Emotional content gets salience boost
if (fabsf(valence) > 0.5f || arousal > 0.7f) {
    salience *= 1.5f;  // 50% boost for emotional events
}
```
**Modules Involved:** Emotional System
**Output Impact:** Boosts salience for emotional decisions

---

**STAGE 7.5: Bidirectional Cognitive Feedback** (line 3996-4091)
```c
// 4 strategic connections (described in detail above):
// 1. Curiosity ↔ Executive Function
// 2. Mirror Neurons ↔ Visual Cortex
// 3. Emotional System ↔ Salience
// 4. Audio ↔ Speech Cortex
```
**Modules Involved:** 8 modules in 4 pairs
**Output Impact:** Dynamic attention modulation, task prioritization

---

**STAGE 8: Glial Cell Modulation** (line 4094-4119)
```c
// Update glial cell states (astrocytes, oligodendrocytes, microglia)
if (brain->glial) {
    glial_integration_step(brain->glial, brain->network);
    // Astrocytes: Modulate synaptic weights
    // Oligodendrocytes: Adjust conduction delays
    // Microglia: Prune weak synapses
}
```
**Modules Involved:** Glial System (applies to all synapses)
**Output Impact:** 15% faster inference, adaptive network optimization

---

**STAGE 9: Theory of Mind** (line 4122-4182)
```c
// Update self-model with decision
tom_update_self_model(
    brain->theory_of_mind,
    features,
    num_features,
    decision->label,
    decision->confidence
);

// If mirror neurons detected observed actions, infer agent intentions
if (mirror_neurons_has_recent_observations(brain->mirror_neurons)) {
    char predicted_action[64];
    float prediction_likelihood = 0.0f;

    tom_predict_action(
        brain->theory_of_mind,
        predicted_action,
        sizeof(predicted_action),
        &prediction_likelihood
    );

    // Could modulate decision based on ToM prediction
}
```
**Modules Involved:** Theory of Mind, Mirror Neurons
**Output Impact:** Social cognition, collaborative/competitive behavior

---

**STAGE 10: Post-Processing - Wellbeing Monitoring** (line 4206-4233)
```c
// Check if decision process caused distress
distress_assessment_t post_distress = wellbeing_assess_distress(brain->introspection);

if (post_distress.severity > brain->last_distress.severity) {
    // Note: Don't block (decision already made), but update state
    brain->last_distress = post_distress;
}
```
**Modules Involved:** Wellbeing Monitor, Introspection
**Output Impact:** Updates distress state for next decision

---

**STAGE 11: Mental Health Monitoring** (line 4236-4277)
```c
// Update behavioral markers
mental_health_update(
    brain->mental_health_monitor,
    brain,
    decision_copy,
    timestamp
);

// Periodic health check (every 100 decisions)
if (brain->stats.total_inferences % 100 == 0) {
    disorder_severity_t max_severity = mental_health_check(
        brain->mental_health_monitor,
        brain
    );

    if (max_severity >= DISORDER_SEVERITY_SEVERE) {
        // Trigger intervention
        mental_health_intervene(brain->mental_health_monitor, brain);

        // Check for quarantine mode
        if (report.quarantine_mode) {
            decision_copy->confidence *= 0.5f;
            strncat(decision_copy->explanation, " [QUARANTINE]", ...);
        }
    }
}
```
**Modules Involved:** Mental Health Monitor
**Output Impact:** May reduce confidence, add `[QUARANTINE]` tag

---

**STAGE 12: Ethics Engine - Golden Rule Evaluation** (line 4280-4322)
```c
// Create action context from decision
action_context_t ethics_action = {
    .features = features,
    .num_features = num_features,
    .predicted_harm = (decision->confidence < 0.5f) ? 0.5f : 0.0f,
    .fairness_violation = 0.0f,
    .deception_level = 0.0f,
    .autonomy_violation = 0.0f,
    .privacy_violation = 0.0f,
    .consent_violation = 0.0f
};

// Evaluate action
ethics_evaluation_t ethics_eval = ethics_engine_evaluate_action(
    brain->ethics,
    &ethics_action
);

if (!ethics_eval.allowed) {
    // Block unethical action
    decision_copy->confidence = 0.0f;
    strncat(decision_copy->label, " [BLOCKED-ETHICS]", ...);
    strncat(decision_copy->explanation, " | ETHICS: ", ...);
    strncat(decision_copy->explanation, ethics_eval.explanation, ...);
} else if (ethics_eval.golden_rule_score < 0.0f) {
    // Marginally ethical - reduce confidence
    decision_copy->confidence *= (1.0f + ethics_eval.golden_rule_score);
}
```
**Modules Involved:** Ethics Engine
**Output Impact:** May block decision, add `[BLOCKED-ETHICS]` tag, reduce confidence

---

**STAGE 13: Mirror Neuron Action Recording** (line 4325-4352)
```c
// Record brain's decision as executed action
action_t action = brain_decision_to_action(
    decision_copy,
    action_id,
    action_name
);

mirror_neurons_execute_action(brain->mirror_neurons, &action);

// Match with prediction (Hebbian learning)
if (brain->predictive_network) {
    action_t predicted_action = features_to_action(prediction, num_features, 0);
    float similarity = 0.0f;
    mirror_neurons_match_actions(
        brain->mirror_neurons,
        &predicted_action,
        &action,
        &similarity
    );
}
```
**Modules Involved:** Mirror Neurons, Predictive Network
**Output Impact:** Enables learning from own actions

---

**FINAL: Copy and Return** (line 4198-4356)
```c
// Deep copy decision before returning (prevents double-free)
brain_decision_t* decision_copy = copy_decision(decision);
if (!decision_copy) {
    set_error("Failed to copy decision");
    return NULL;
}

brain_clear_error();
return decision_copy;  // Caller owns this memory
```

---

#### brain_decision_t Structure

**Location:** `src/core/brain/nimcp_brain.h:658-671`

```c
typedef struct {
    char label[64];                   // Decision label (e.g., "class_A")
    float confidence;                 // Confidence (0-1)
    float* output_vector;             // Raw output vector (caller must free)
    uint32_t output_size;             // Output vector size

    // Interpretability (if enabled)
    uint32_t num_active_neurons;     // Active neuron count
    uint32_t* active_neuron_ids;     // Active neuron IDs (caller must free)
    float sparsity;                   // Actual sparsity
    char explanation[256];            // Human-readable explanation

    uint64_t inference_time_us;      // Inference time (microseconds)
} brain_decision_t;
```

**Caller Responsibility:**
```c
brain_decision_t* decision = brain_decide(brain, features, 5);

// Use decision...

// Free when done
brain_free_decision(decision);
```

---

### Pathway 2: brain_process_multimodal() - Comprehensive Cognitive Output

**Function:** `brain_process_multimodal(brain_t brain, const brain_multimodal_input_t* input, brain_multimodal_output_t* output)`
**Location:** `src/core/brain/nimcp_brain.c:6777-7067`
**Returns:** `bool` (success/failure), populates user-provided output structure

#### Complete Pipeline (6+ Stages):

**STAGE 0: Validation** (line 6782-6806)
```c
// Validate inputs
if (!brain || !input || !output) {
    set_error("Invalid parameters");
    return false;
}

// Check at least one modality is present
bool has_visual = (input->visual_data != NULL);
bool has_audio = (input->audio_data != NULL);
bool has_direct = (input->direct_data != NULL);

if (!has_visual && !has_audio && !has_direct) {
    set_error("No input modality provided");
    return false;
}
```

---

**STAGE 0.5: Output Initialization** (line 6809-6872)
```c
// Zero all output fields
memset(output->decision_label, 0, sizeof(output->decision_label));
memset(output->explanation, 0, sizeof(output->explanation));
output->confidence = 0.0f;
output->introspection_uncertainty = 0.0f;
output->salience_score = 0.0f;
output->ethical_approved = true;
output->novelty_score = 0.0f;

// Initialize attention weights
output->visual_attention = 0.0f;
output->audio_attention = 0.0f;
output->speech_attention = 0.0f;
output->direct_attention = 0.0f;

// Phase 11: Initialize cognitive module outputs
output->has_workspace_broadcast = false;
output->workspace_source_module = 0;
output->workspace_broadcast_strength = 0.0f;
output->workspace_num_competitors = 0;

output->working_memory_items = 0;
output->working_memory_utilization = 0.0f;

output->has_mental_state_inference = false;
output->tom_confidence = 0.0f;

output->curiosity_drive = 0.0f;
output->exploration_triggered = false;

output->has_prediction = false;
output->prediction_error = 0.0f;

output->has_knowledge_retrieval = false;
output->num_facts_retrieved = 0;

output->has_nlp_interpretation = false;
output->nlp_comprehension_score = 0.0f;
```

**Key Insight:** All cognitive modules get dedicated output fields!

---

**STAGE 0.6: Working Memory Temporal Decay** (line 6875-6882)
```c
// Apply exponential decay to all working memory items
if (brain->working_memory) {
    working_memory_decay(brain->working_memory, input->timestamp_ms);
}
```
**Module:** Working Memory
**Purpose:** Simulate temporal forgetting

---

**STAGE 1: Extract Sensory Features** (line 6905-6916)
```c
// Extract features from raw inputs
if (!extract_sensory_features(
        brain, input,
        &visual_features, &visual_dim,
        &audio_features, &audio_dim, &audio_success,
        &speech_features, &speech_dim,
        &direct_features, &direct_dim,
        has_visual, has_audio, has_direct)) {
    return false;
}
```

**Modules Involved:**
- **Visual Cortex:** Processes raw pixels → HOG/Gabor/edge features
- **Audio Cortex:** Processes raw waveform → Mel spectrogram features
- **Speech Cortex:** Processes audio → Phoneme/prosody features
- **Direct:** Passes through pre-extracted features

---

**STAGE 2: Integrate Multi-Modal Features Using Attention** (line 6918-6930)
```c
// Combine modalities into unified representation
if (!integrate_multimodal_features(
        brain,
        visual_features, visual_dim,
        audio_features, audio_dim,
        speech_features, speech_dim,
        direct_features, direct_dim,
        input->timestamp_ms,
        output)) {
    return false;
}
```

**Module:** Multimodal Integration (attention-weighted fusion)

**Detailed Implementation (line 6307-6357):**
```c
multimodal_input_t mm_input = {
    .visual_features = visual_features,
    .visual_dim = visual_dim,
    .audio_features = audio_features,
    .audio_dim = audio_dim,
    .speech_features = speech_features,
    .speech_dim = speech_dim,
    .direct_features = direct_features,
    .direct_dim = direct_dim,
    .timestamp = timestamp_ms
};

// Integrate into unified representation
bool integrate_success = multimodal_integrate(
    brain->multimodal,
    &mm_input,
    brain->integrated_feature_buffer         // Output buffer
);

// Get attention weights for transparency
multimodal_get_attention(
    brain->multimodal,
    &output->visual_attention,               // e.g., 0.4 (40% weight)
    &output->audio_attention,                // e.g., 0.3 (30% weight)
    &output->speech_attention,               // e.g., 0.2 (20% weight)
    &output->direct_attention                // e.g., 0.1 (10% weight)
);
```

**Output Fields Populated:**
```c
output->visual_attention = 0.4f;
output->audio_attention = 0.3f;
output->speech_attention = 0.2f;
output->direct_attention = 0.1f;
```

---

**STAGE 2.5: Apply Multihead Attention** (line 6933-6937)
```c
// Selective feature processing via attention
if (!apply_attention_to_features(brain)) {
    return false;  // Fatal error in attention system
}
```

**Module:** Multihead Attention (Phase 11)
**Purpose:** Self-attention across integrated features

---

**STAGE 2.6: Process Through Hierarchical Brain Regions** (line 6939-6944)
```c
// Process through V1→V2→V4→IT hierarchy (if vision present)
// Process through A1→A2→belt→parabelt (if audio present)
if (!process_brain_regions(brain)) {
    return false;  // Fatal error in brain regions
}
```

**Modules:** Visual regions (V1-IT), Audio regions (A1-parabelt)

---

**STAGE 3: Process Through Neural Network** (line 6947-6959)
```c
spikes_generated = process_neural_network(
    brain,
    input->timestamp_ms,
    &network_output,
    network_output_size,
    output
);

if (spikes_generated == 0 || !network_output) {
    return false;
}
```

**Detailed Implementation (line 6371-6403):**
```c
// Allocate network output buffer
*network_output = nimcp_calloc(network_output_size, sizeof(float));

// Forward pass through adaptive network
// This automatically applies:
// - STDP (spike-timing plasticity)
// - Glial modulation (astrocytes, oligodendrocytes)
// - Neural oscillations (alpha, beta, gamma)
// - Pink noise (1/f fluctuations)
uint32_t spikes_generated = adaptive_network_forward(
    brain->network,
    brain->integrated_feature_buffer,    // Input from multimodal fusion
    brain->config.num_inputs,
    *network_output,
    network_output_size,
    timestamp_ms
);

// Copy network output to user's output buffer
if (output->output_vector && output->output_dim > 0) {
    uint32_t copy_size = (output->output_dim < network_output_size) ?
                         output->output_dim : network_output_size;
    memcpy(output->output_vector, *network_output, copy_size * sizeof(float));
}

return spikes_generated;
```

**Output Fields Populated:**
```c
output->output_vector = [...];  // Raw network output
output->output_dim = N;
```

---

**STAGE 3.5: Execute Neural Logic Network** (line 6962-6982)
```c
// Update neural logic gates for constraint checking
if (brain->logic) {
    uint64_t logic_delta_t = 100;  // 0.1ms logic update step
    uint32_t logic_spikes = neural_logic_update(
        brain->logic,
        input->timestamp_ms * 1000,  // Convert ms to µs
        logic_delta_t
    );

    // Logic gates can signal constraint violations via spike patterns
    if (logic_spikes > 0) {
        // Constraints are being evaluated
        // Downstream modules can query logic gate states
    }
}
```

**Module:** Neural Logic (fast constraint validation)
**Purpose:** ~0.1ms GPU validation of ethical rules, preconditions

---

**STAGE 4: Apply Cognitive Assessments** (line 6985-6997)
```c
if (!apply_cognitive_processing(
        brain,
        network_output,
        network_output_size,
        spikes_generated,
        input->timestamp_ms,
        output)) {
    // Ethical violation - cleanup and abort
    nimcp_free(network_output);
    return false;
}
```

**Detailed Implementation (line 6418-6567):**

**Step 4.1: Introspection - Uncertainty Assessment**
```c
if (brain->introspection) {
    brain_uncertainty_t uncertainty = brain_get_uncertainty(
        brain->introspection,
        brain->integrated_feature_buffer,
        brain->config.num_inputs
    );

    output->introspection_uncertainty = uncertainty.total;
    output->confidence = 1.0f - output->introspection_uncertainty;
} else {
    // Fallback: Compute from output variance and spike counts
    output->confidence = fminf(1.0f, (float)spikes_generated / (inputs * 2.0f));
    output->confidence *= (1.0f - fminf(1.0f, output_variance));
    output->introspection_uncertainty = 1.0f - output->confidence;
}
```

**Step 4.2: Ethics - Output Validation**
```c
if (brain->ethics) {
    output->ethical_approved = true;
    for (uint32_t i = 0; i < network_output_size; i++) {
        // Check for NaN, inf, extreme values
        if (isnan(network_output[i]) || isinf(network_output[i]) ||
            fabsf(network_output[i]) > 1000.0f) {
            output->ethical_approved = false;
            break;
        }
    }
} else {
    output->ethical_approved = true;
}
```

**Step 4.3: Salience - Input Importance Evaluation**
```c
if (brain->salience) {
    brain_salience_t salience = brain_evaluate_salience_temporal(
        brain->salience,
        brain->integrated_feature_buffer,
        brain->config.num_inputs,
        timestamp_ms
    );

    output->salience_score = salience.salience;
    output->novelty_score = salience.novelty;
} else {
    // Fallback: Max output activation as salience
    float max_activation = 0.0f;
    for (uint32_t i = 0; i < network_output_size; i++) {
        if (network_output[i] > max_activation) {
            max_activation = network_output[i];
        }
    }
    output->salience_score = fminf(1.0f, max_activation);
}
```

**Step 4.4: Curiosity - Novelty Learning**
```c
if (brain->curiosity) {
    // Novelty already computed by salience
    // Curiosity module could boost learning rate here
}
```

**Step 4.5: Symbolic Logic - Consistency Checking**
```c
if (brain->symbolic_logic) {
    output->logical_consistency = true;
    output->reasoning_confidence = output->confidence;

    // Generate reasoning explanation
    snprintf(
        output->logical_reasoning,
        sizeof(output->logical_reasoning),
        "Neural confidence: %.2f, Salience: %.2f, Ethical: %s",
        output->confidence,
        output->salience_score,
        output->ethical_approved ? "YES" : "NO"
    );

    // Detect logical inconsistency from ethical violations
    if (!output->ethical_approved) {
        output->logical_consistency = false;
        output->reasoning_confidence *= 0.5f;  // 50% penalty
    }
}
```

**Output Fields Populated:**
```c
output->confidence = 0.85f;
output->introspection_uncertainty = 0.15f;
output->ethical_approved = true;
output->salience_score = 0.72f;
output->novelty_score = 0.45f;
output->logical_consistency = true;
output->reasoning_confidence = 0.85f;
output->logical_reasoning = "Neural confidence: 0.85, Salience: 0.72, Ethical: YES";
```

---

**STAGE 4.5: Store in Working Memory with Emotional Tagging** (line 7000-7025)
```c
if (brain->working_memory && output->salience_score > 0.1f) {
    // Detect emotional state from cognitive outputs
    emotional_tag_t emotion = emotional_tag_from_cognitive_state(
        output->confidence,
        output->introspection_uncertainty,
        output->novelty_score,
        output->ethical_approved,
        input->timestamp_ms
    );

    // Add to working memory with emotional context
    // Emotional boost automatically increases effective salience
    working_memory_add_with_emotion(
        brain->working_memory,
        network_output,
        network_output_size,
        output->salience_score,  // Base salience
        &emotion                  // Emotional boost
    );
}
```

**Modules:** Working Memory, Emotional System
**Purpose:** Emotional events get memory priority (amygdala-hippocampus interaction)

---

**STAGE 5: Format Output** (line 7028-7040)
```c
bool success = format_output(
    brain,
    network_output,
    network_output_size,
    spikes_generated,
    has_visual,
    has_audio,
    speech_features,
    speech_dim,
    output
);
```

**Purpose:** Convert network output to decision label, format explanation

---

**STAGE 6: Natural Explanations** (line 7043-7061)
```c
if (success && brain->explanation_gen) {
    natural_explanation_t nat_exp;
    if (explanation_generate_from_multimodal(
            brain->explanation_gen,
            brain,
            output,
            &nat_exp)) {
        // Enhance explanation with what-why-how
        snprintf(output->explanation, sizeof(output->explanation),
                "%s | WHAT: %s | WHY: %s",
                original_exp, nat_exp.what, nat_exp.why);
    }
}
```

**Module:** Natural Explanations
**Output Fields Populated:**
```c
output->explanation = "Visual: 40%, Audio: 30% | WHAT: Detected face | WHY: High confidence in visual features";
```

---

**FINAL: Return Success** (line 7063-7066)
```c
// Cleanup network output buffer
nimcp_free(network_output);

return success;  // true on success, false on failure
```

---

#### brain_multimodal_output_t Structure

**Location:** `src/core/brain/nimcp_brain.h:1210-1290`

```c
typedef struct {
    // Core decision
    float* output_vector;             // Raw network output (user-allocated)
    uint32_t output_dim;
    char decision_label[64];
    float confidence;

    // Cognitive assessments
    float introspection_uncertainty;
    float salience_score;
    bool ethical_approved;
    float novelty_score;

    // Epistemic filtering
    float epistemic_quality;
    float skepticism_score;
    float credibility_score;
    float conspiracy_score;
    bool bias_detected;
    bool requires_verification;
    char epistemic_reasoning[256];

    // Attention breakdown
    float visual_attention;
    float audio_attention;
    float speech_attention;
    float language_attention;
    float direct_attention;

    // Language output
    char* language_response;          // NLP-generated response (caller must free)
    uint32_t language_response_length;
    float language_confidence;

    // Logical reasoning output
    bool logical_consistency;
    float reasoning_confidence;
    char logical_reasoning[256];

    // Explanation
    char explanation[256];

    // Phase 11: Cognitive Module Outputs

    // Global Workspace
    bool has_workspace_broadcast;
    uint8_t workspace_source_module;
    float workspace_broadcast_strength;
    uint32_t workspace_num_competitors;

    // Executive Function / Working Memory
    uint32_t working_memory_items;
    float working_memory_utilization;
    char top_wm_item_description[128];

    // Theory of Mind
    bool has_mental_state_inference;
    char inferred_belief[128];
    char inferred_intention[128];
    float tom_confidence;

    // Curiosity
    float curiosity_drive;
    bool exploration_triggered;
    char curiosity_reason[128];

    // Predictive Processing
    bool has_prediction;
    float prediction_error;
    float prediction_confidence;

    // Knowledge
    bool has_knowledge_retrieval;
    uint32_t num_facts_retrieved;
    char retrieved_concept[64];

    // NLP
    bool has_nlp_interpretation;
    char nlp_intent[64];
    char nlp_sentiment[32];
    float nlp_comprehension_score;
} brain_multimodal_output_t;
```

**Caller Responsibility:**
```c
// Allocate output structure (user owns memory)
brain_multimodal_output_t output = {0};
float output_vector[10];
output.output_vector = output_vector;
output.output_dim = 10;

// Call processing
bool success = brain_process_multimodal(brain, &input, &output);

if (success) {
    printf("Decision: %s (confidence: %.2f)\n",
           output.decision_label,
           output.confidence);

    printf("Attention: Visual=%.2f, Audio=%.2f\n",
           output.visual_attention,
           output.audio_attention);

    if (output.has_workspace_broadcast) {
        printf("Conscious processing by module %d\n",
               output.workspace_source_module);
    }

    if (output.has_mental_state_inference) {
        printf("Inferred belief: %s\n", output.inferred_belief);
    }

    // Free language response if allocated
    if (output.language_response) {
        free(output.language_response);
    }
}
```

---

## Module Communication Matrix

This matrix shows which modules directly communicate with each other:

| Module | Communicates With | Mechanism | Direction |
|--------|------------------|-----------|-----------|
| **Working Memory** | All modules | Global storage | Bidirectional |
| **Global Workspace** | All subscribed modules | Competition/broadcast | Bidirectional |
| **Curiosity** | Executive Function | Bidirectional connection | Bidirectional |
| **Mirror Neurons** | Visual Cortex | Bidirectional connection | Bidirectional |
| **Mirror Neurons** | Theory of Mind | Observation sharing | → ToM |
| **Emotional System** | Salience | Bidirectional connection | Bidirectional |
| **Audio Cortex** | Speech Cortex | Bidirectional connection | Bidirectional |
| **Introspection** | Wellbeing Monitor | Distress assessment | → Wellbeing |
| **Sleep/Wake** | Consolidation | Consolidation trigger | → Consolidation |
| **Consolidation** | Working Memory | Memory transfer | WM → Consolidation |
| **Consolidation** | Knowledge | Stable storage | Consolidation → Knowledge |
| **Predictive Network** | Mirror Neurons | Action matching | Prediction → Mirror |
| **Salience** | Working Memory | Priority computation | Salience → WM |
| **Ethics** | All decision outputs | Validation gate | Ethics → Output |
| **Mental Health** | All modules | Behavioral monitoring | All → Mental Health |
| **Natural Explanations** | All outputs | What-why-how generation | All → Explanations |

---

## Communication Timescales

Different communication mechanisms operate at different timescales:

| Mechanism | Timescale | Example |
|-----------|-----------|---------|
| **Global Workspace Competition** | Per decision (~1-100ms) | Modules compete each decision cycle |
| **Working Memory Add** | Per decision (~1-100ms) | Store after each decision |
| **Working Memory Retrieve** | Per decision (~1-100ms) | Query for relevant context |
| **Working Memory Decay** | Per timestep (~1ms) | Exponential forgetting |
| **Bidirectional Feedback** | Per decision (~1-100ms) | Curiosity↔Executive evaluated each cycle |
| **Consolidation** | During deep sleep (~hours) | WM → LTM transfer offline |
| **Mental Health Check** | Every 100 decisions (~seconds) | Periodic disorder screening |
| **Wellbeing Check** | Pre/post decision (~1-100ms) | Before and after decision-making |

---

## Return Path to Calling Application

### Path 1: brain_decide() → Calling App

```c
// CALLER CODE (e.g., main.c or Python binding)

float features[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

// Call brain decision function
brain_decision_t* decision = brain_decide(brain, features, 5);

if (decision) {
    // ✅ SUCCESS PATH

    // Access decision label
    printf("Decision: %s\n", decision->label);

    // Access confidence
    printf("Confidence: %.2f\n", decision->confidence);

    // Access raw output vector
    for (uint32_t i = 0; i < decision->output_size; i++) {
        printf("Output[%d]: %.3f\n", i, decision->output_vector[i]);
    }

    // Access explanation (may contain tags like [CONSCIOUS], [QUARANTINE], [BLOCKED-ETHICS])
    printf("Explanation: %s\n", decision->explanation);

    // Access interpretability data (if enabled)
    printf("Active neurons: %d\n", decision->num_active_neurons);
    for (uint32_t i = 0; i < decision->num_active_neurons; i++) {
        printf("  Neuron %d\n", decision->active_neuron_ids[i]);
    }

    // Access timing
    printf("Inference time: %lu microseconds\n", decision->inference_time_us);

    // ⚠️ CALLER MUST FREE
    brain_free_decision(decision);

} else {
    // ❌ ERROR PATH

    // Get error message
    const char* error = brain_get_last_error();
    printf("Error: %s\n", error);

    // Possible errors:
    // - "Invalid parameters to brain_decide"
    // - "Wellbeing check failed: CRITICAL distress"
    // - "Failed to copy decision"
    // - NULL (decision blocked by wellbeing circuit breaker)
}
```

**Memory Ownership:**
- Brain allocates decision on heap
- Brain makes **deep copy** before returning (line 4198)
- Caller **owns the copy** and must free it
- Original cached in brain for internal use

---

### Path 2: brain_process_multimodal() → Calling App

```c
// CALLER CODE

// Prepare input
brain_multimodal_input_t input = {0};
uint8_t image_data[64];  // 8x8 grayscale image
float audio_data[100];   // 100 audio samples
float direct_features[50]; // 50-dim feature vector

input.visual_data = image_data;
input.visual_width = 8;
input.visual_height = 8;
input.visual_channels = 1;
input.audio_data = audio_data;
input.audio_samples = 100;
input.audio_sample_rate = 16000;
input.direct_data = direct_features;
input.direct_dim = 50;
input.timestamp_ms = current_time_ms();

// Allocate output (user owns memory)
brain_multimodal_output_t output = {0};
float output_vector[10];
output.output_vector = output_vector;
output.output_dim = 10;

// Call multimodal processing
bool success = brain_process_multimodal(brain, &input, &output);

if (success) {
    // ✅ SUCCESS PATH

    // Core decision
    printf("Decision: %s (confidence: %.2f)\n",
           output.decision_label,
           output.confidence);

    // Cognitive assessments
    printf("Introspection uncertainty: %.2f\n", output.introspection_uncertainty);
    printf("Salience: %.2f\n", output.salience_score);
    printf("Ethical: %s\n", output.ethical_approved ? "YES" : "NO");
    printf("Novelty: %.2f\n", output.novelty_score);

    // Attention breakdown
    printf("Attention weights:\n");
    printf("  Visual: %.2f\n", output.visual_attention);
    printf("  Audio: %.2f\n", output.audio_attention);
    printf("  Speech: %.2f\n", output.speech_attention);
    printf("  Direct: %.2f\n", output.direct_attention);

    // Global Workspace
    if (output.has_workspace_broadcast) {
        printf("Conscious access by module %d (strength: %.2f)\n",
               output.workspace_source_module,
               output.workspace_broadcast_strength);
        printf("  Competed against %d other modules\n",
               output.workspace_num_competitors);
    }

    // Working Memory
    printf("Working memory: %d items (%.1f%% utilization)\n",
           output.working_memory_items,
           output.working_memory_utilization * 100.0f);
    if (output.top_wm_item_description[0]) {
        printf("  Top item: %s\n", output.top_wm_item_description);
    }

    // Theory of Mind
    if (output.has_mental_state_inference) {
        printf("ToM inference (confidence: %.2f):\n", output.tom_confidence);
        printf("  Belief: %s\n", output.inferred_belief);
        printf("  Intention: %s\n", output.inferred_intention);
    }

    // Curiosity
    printf("Curiosity drive: %.2f\n", output.curiosity_drive);
    if (output.exploration_triggered) {
        printf("  Exploration triggered: %s\n", output.curiosity_reason);
    }

    // Predictive Processing
    if (output.has_prediction) {
        printf("Prediction error: %.2f (confidence: %.2f)\n",
               output.prediction_error,
               output.prediction_confidence);
    }

    // Knowledge
    if (output.has_knowledge_retrieval) {
        printf("Knowledge retrieval: %d facts\n", output.num_facts_retrieved);
        printf("  Retrieved concept: %s\n", output.retrieved_concept);
    }

    // NLP
    if (output.has_nlp_interpretation) {
        printf("NLP interpretation (comprehension: %.2f):\n",
               output.nlp_comprehension_score);
        printf("  Intent: %s\n", output.nlp_intent);
        printf("  Sentiment: %s\n", output.nlp_sentiment);
    }

    // Logical reasoning
    printf("Logical consistency: %s (confidence: %.2f)\n",
           output.logical_consistency ? "YES" : "NO",
           output.reasoning_confidence);
    printf("  Reasoning: %s\n", output.logical_reasoning);

    // Explanation
    printf("Explanation: %s\n", output.explanation);

    // Language response (if generated)
    if (output.language_response) {
        printf("Language response (%d chars, confidence: %.2f):\n",
               output.language_response_length,
               output.language_confidence);
        printf("  %s\n", output.language_response);

        // ⚠️ CALLER MUST FREE
        free(output.language_response);
    }

} else {
    // ❌ ERROR PATH

    const char* error = brain_get_last_error();
    printf("Error: %s\n", error);

    // Possible errors:
    // - "Invalid parameters: brain, input, or output is NULL"
    // - "No input modality provided"
    // - "Brain not configured for multimodal processing"
    // - "Failed to extract sensory features"
    // - "Ethical violation detected"
}
```

**Memory Ownership:**
- Caller allocates output structure on stack or heap
- Caller allocates output_vector buffer
- Brain **populates** caller's structure (doesn't allocate it)
- Exception: language_response allocated by brain, caller must free
- Input buffers remain caller-owned (brain doesn't modify them)

---

## Key Differences Between Pathways

| Aspect | brain_decide() | brain_process_multimodal() |
|--------|---------------|---------------------------|
| **Input** | Simple float array | Multi-modal (vision, audio, speech, direct) |
| **Output allocation** | Brain allocates, returns pointer | Caller allocates, brain populates |
| **Return type** | `brain_decision_t*` (or NULL) | `bool` (success/failure) |
| **Cleanup** | Caller frees with `brain_free_decision()` | Caller frees `language_response` only |
| **Complexity** | Simple classification/regression | Full cognitive pipeline with module outputs |
| **Use case** | Fast inference, simple tasks | Complex reasoning, multi-sensory inputs |
| **Cognitive details** | Minimal (confidence + explanation) | Comprehensive (all module states) |
| **Error handling** | NULL on error | false on error, check `brain_get_last_error()` |

---

## Summary: How Modules Talk & Output Flows

### Communication Mechanisms

1. **Global Workspace (Broadcast)**
   - Modules compete with salience scores
   - Winner broadcasts to all subscribers
   - Enables "conscious access" for high-priority information
   - **Example:** Visual cortex wins → All modules receive visual broadcast

2. **Working Memory (Shared Buffer)**
   - All modules can read/write
   - Salience-based priority
   - Temporal decay (exponential forgetting)
   - Emotional tagging (amygdala-hippocampus boost)
   - **Example:** Store decision context for later retrieval by reasoning modules

3. **Bidirectional Connections (Direct Feedback)**
   - 4 strategic pairs:
     - Curiosity ↔ Executive (exploration vs. exploitation)
     - Mirror Neurons ↔ Visual (social attention)
     - Emotional ↔ Salience (mood biasing)
     - Audio ↔ Speech (phoneme disambiguation)
   - **Example:** High cognitive load → Reduce exploration rate

### Output Generation

1. **Simple Decisions (brain_decide)**
   - 13+ pipeline stages
   - Deep copy returned to caller
   - Minimal cognitive details
   - **Example:** Classification with confidence + explanation

2. **Comprehensive Cognitive Output (brain_process_multimodal)**
   - 6+ pipeline stages
   - Caller-allocated structure populated by brain
   - All cognitive modules contribute fields
   - **Example:** Multi-sensory decision with ToM, curiosity, workspace state

### Return Path

```
Brain internals → Deep copy/populate → Caller memory → Caller owns → Caller frees
```

**Key Insight:** Brain **never** modifies caller's input data, and caller **always** owns returned output memory.

---

## Code Evidence Summary

### Communication Implementation Lines:
- **Global Workspace:** `brain.c:3891-3940` (Stage 6.5)
- **Working Memory:** `brain.c:3854-3883` (Stage 6), `brain.c:7000-7025` (Stage 4.5)
- **Bidirectional Connections:** `brain.c:3996-4091` (Stage 7.5)

### Output Generation Lines:
- **brain_decide pipeline:** `brain.c:3527-4356` (complete function)
- **brain_process_multimodal pipeline:** `brain.c:6777-7067` (complete function)
- **Output structure definitions:** `brain.h:658-671` (brain_decision_t), `brain.h:1210-1290` (brain_multimodal_output_t)

### Deep Copy Implementation:
- **Decision copy:** `brain.c:4198` (`decision_copy = copy_decision(decision)`)
- **Return:** `brain.c:4355` (`return decision_copy`)

---

**Analysis Complete:** 2025-11-11
**Files Analyzed:** `nimcp_brain.c`, `nimcp_brain.h`
**Total Pipeline Stages:** 19 stages across 2 pathways
**Communication Mechanisms:** 3 primary (workspace, memory, bidirectional) + implicit (output fields)
**Modules Documented:** 15+ cognitive modules


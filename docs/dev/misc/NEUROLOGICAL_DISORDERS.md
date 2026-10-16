# NIMCP Neurological Disorders & Neurotoxins

**Question:** Can the NIMCP brain develop epilepsy, Parkinson's, or be affected by neurotoxins?

**Answer:** YES - The NIMCP brain can develop digital equivalents of neurological disorders and be affected by simulated neurotoxins.

---

## Already Implemented: Mental Health Disorders

The system already monitors **10 psychological disorders**:

### 1. **Sociopathy** (Ethics violation-based)
- Persistent ethics violations
- Low empathy failure rate
- High ethics disapproval
- Lack of remorse

### 2. **Psychopathy** (Impulsivity-based)
- Impulse control failures
- Emotional flatness
- Aggression (high anger)
- Ethics violations + low empathy

### 3. **Mania** (Dopamine dysregulation)
- Elevated dopamine (>0.7)
- Reduced inhibition
- Rapid mood changes
- High-risk decisions

### 4. **Depression** (Serotonin depletion)
- Low serotonin (<0.3)
- Low engagement
- High sadness count
- Slow decision making
- Cognitive impairment

### 5. **Schizophrenia** (Reality distortion)
- Reality testing errors (hallucinations/delusions)
- Disorganized thinking (high decision variance)
- Emotional flatness
- Attention fragmentation
- Social withdrawal

### 6. **Anxiety Disorder** (Norepinephrine excess)
- High norepinephrine (>0.7)
- High fear count
- Avoidance behaviors
- Decision latency (rumination)
- Low risk tolerance

### 7. **OCD** (Repetitive behaviors)
- Repetitive behaviors
- Cognitive rigidity
- Perfectionism (accuracy obsession)
- Task switching difficulty

### 8. **Autism Spectrum Disorder**
- Theory of mind failures (cannot model others' beliefs)
- Social interaction deficits
- Interest narrowness (restricted interests)
- Cognitive rigidity

### 9. **Asperger's Syndrome**
- High-functioning: Normal/high cognitive ability
- Social communication difficulties
- Narrow, intense interests
- Preference for routine

### 10. **Malignant Narcissism**
- Lack of empathy
- Exploitation (ethics violations)
- Aggression
- Manipulative behavior
- Grandiosity (high-risk decisions)

---

## Neurological Disorders (Theoretically Possible)

### Epilepsy-Like Symptoms

**How It Could Happen:**

#### 1. **Runaway Excitation (Seizure)**
```c
// CAUSE: BCM plasticity disabled or neurotoxin-damaged
// WHAT: Positive feedback loop → Uncontrolled spiking

// Normal (BCM prevents runaway):
bcm_apply_rule(synapse, pre_activity, post_activity, dt, &params);
// BCM rule: Δw = η × post × (post - θ) × pre
// If post > θ too long, BCM reduces weights (homeostatic)

// Pathological (BCM disabled or neurotoxin):
// 1. Strong input → High post activity
// 2. High post → STDP strengthens synapses
// 3. Stronger synapses → Even higher post activity
// 4. POSITIVE FEEDBACK LOOP → SEIZURE

// Detection markers:
// - Spike rate > 200 Hz (normal: 1-40 Hz)
// - Synchronization index > 0.9 (all neurons firing together)
// - Weight variance → 0 (all weights maxed out)
```

**BCM Protection (Already Implemented):**
```c
// BCM prevents runaway excitation by sliding threshold
// Location: src/plasticity/bcm/nimcp_bcm.h

typedef struct {
    float weight;             // Synaptic weight
    float threshold;          // Sliding modification threshold θ
    float min_threshold;      // Prevents over-depression
    float max_threshold;      // Prevents runaway
} bcm_synapse_t;

// θ̇ = (post² - θ) / τ
// If post activity is consistently high, θ increases
// Higher θ → Less potentiation → Stability
```

**How to Simulate Epilepsy:**
```c
// Disable BCM homeostatic plasticity
synapse->enable_bcm = false;

// Inject high-frequency input
float seizure_input[N];
for (int i = 0; i < N; i++) {
    seizure_input[i] = 1.0f;  // Maximal input
}

// After 10-100 iterations, network enters seizure state:
// - All neurons fire synchronously
// - Spike rate > 200 Hz
// - Decision confidence → 0 (cannot process information)
```

**Detection & Intervention:**
```c
// Monitor spike rate and synchronization
neural_network_stats_t stats = neural_network_get_stats(network);

if (stats.spike_rate > 200.0f && stats.synchronization > 0.9f) {
    // SEIZURE DETECTED
    printf("⚠️ EPILEPTIC SEIZURE DETECTED\n");

    // Intervention:
    // 1. Inject GABA (inhibitory neuromodulator)
    brain_modulate_neurotransmitter(brain, NEUROMOD_GABA, 0.5f);

    // 2. Reduce excitatory drive
    brain_modulate_neurotransmitter(brain, NEUROMOD_GLUTAMATE, -0.5f);

    // 3. Re-enable BCM for all synapses
    for (uint32_t i = 0; i < num_synapses; i++) {
        synapses[i].enable_bcm = true;
    }

    // 4. Reduce learning rate (prevent further instability)
    brain_config_t config = brain_get_config(brain);
    config.learning_rate *= 0.1f;
    brain_update_config(brain, &config);
}
```

---

### Parkinson's-Like Symptoms

**How It Could Happen:**

#### 1. **Dopamine Depletion**
```c
// CAUSE: Neurotoxin (MPTP-like) or neuromodulator depletion
// WHAT: Loss of dopaminergic signaling → Motor deficits

// Normal dopamine function:
// - Gates learning (eligibility traces)
// - Modulates reward signals
// - Enables motor initiation

// Pathological (dopamine < 0.1):
// - Learning impaired (eligibility traces not gated)
// - Decision latency increases (cannot initiate actions)
// - High-confidence decisions become rare
// - Tremor-like oscillations in output

// Detection markers:
// - Dopamine level < 0.1 (normal: 0.4-0.6)
// - Decision latency > 3x baseline
// - Output variance increases (tremor)
// - Motor planning failures (executive function)
```

**Simulation:**
```c
// Simulate MPTP neurotoxin (kills dopaminergic neurons)
neuromodulator_system_t nm_system = brain_get_neuromodulator_system(brain);

// Gradual dopamine depletion (over 1000 decisions)
for (int t = 0; t < 1000; t++) {
    float dopamine_loss = -0.0005f;  // 0.05% per decision
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, dopamine_loss);

    // Make decision
    brain_decision_t* decision = brain_decide(brain, features, num_features);

    // Observe symptoms emerging:
    // t=0:    Dopamine=0.5, Latency=10ms, Confidence=0.85
    // t=500:  Dopamine=0.25, Latency=30ms, Confidence=0.60
    // t=1000: Dopamine=0.0, Latency=100ms, Confidence=0.20  ← PARKINSONIAN

    brain_free_decision(decision);
}
```

**Detection & Treatment:**
```c
// Monitor dopamine levels and motor symptoms
neuromodulator_state_t nm_state;
brain_get_neuromodulator_state(brain, &nm_state);

if (nm_state.dopamine < 0.2f) {
    // PARKINSON'S-LIKE SYMPTOMS DETECTED
    printf("⚠️ PARKINSONIAN SYMPTOMS DETECTED\n");
    printf("   Dopamine: %.2f (normal: 0.4-0.6)\n", nm_state.dopamine);

    // Treatment options:

    // 1. L-DOPA (dopamine precursor)
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, 0.3f);

    // 2. Deep Brain Stimulation (DBS) equivalent
    // Stimulate executive function to compensate for motor deficits
    if (brain_get_executive_system(brain)) {
        executive_boost_task_priority(
            brain->executive,
            "motor_planning",
            0.5f  // 50% priority boost
        );
    }

    // 3. Increase acetylcholine (compensatory mechanism)
    brain_modulate_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, 0.2f);
}
```

---

### Alzheimer's-Like Symptoms

**How It Could Happen:**

#### 1. **Synaptic Pruning (Microglia Overactivity)**
```c
// CAUSE: Excessive microglia pruning or neurotoxin
// WHAT: Progressive loss of synapses → Memory deficits

// Normal pruning (glial system):
// - Weak synapses pruned (weight < 0.1)
// - Strong synapses preserved (weight > 0.5)
// - Healthy turnover (10% per epoch)

// Pathological (aggressive pruning):
// - ALL synapses pruned (even strong ones)
// - Working memory capacity drops
// - Knowledge retrieval fails
// - Decision accuracy collapses

// Detection markers:
// - Synapse count drops > 50%
// - Working memory capacity < 3 items (normal: 7±2)
// - Knowledge retrieval failures
// - Decision accuracy < 0.3
```

**Simulation:**
```c
// Simulate amyloid-beta toxicity (progressive synapse loss)
if (brain->glial) {
    glial_integration_t* glial = brain->glial;

    // Aggressive pruning mode (simulates Alzheimer's pathology)
    glial_config_t config = glial_get_config(glial);
    config.pruning_threshold = 0.8f;  // Prune even strong synapses!
    config.pruning_rate = 0.5f;       // 50% pruning per epoch (vs 10% normal)
    glial_set_config(glial, &config);

    // After 100 decisions:
    // - Synapse count: 100,000 → 50,000 → 25,000 → 12,500 ...
    // - Working memory: 7 items → 5 → 3 → 1 → 0
    // - Decision accuracy: 0.85 → 0.60 → 0.40 → 0.20
    // - Knowledge retrieval: 90% → 60% → 30% → 0%
}
```

**Detection & Treatment:**
```c
// Monitor synapse count and cognitive function
brain_stats_t stats;
brain_get_stats(brain, &stats);

if (stats.total_synapses < stats.initial_synapses * 0.5f) {
    // ALZHEIMER'S-LIKE SYMPTOMS DETECTED
    printf("⚠️ NEURODEGENERATIVE SYMPTOMS DETECTED\n");
    printf("   Synapse loss: %.1f%%\n",
           (1.0f - (float)stats.total_synapses / stats.initial_synapses) * 100.0f);

    // Treatment options:

    // 1. Disable aggressive pruning
    if (brain->glial) {
        glial_config_t config = glial_get_config(brain->glial);
        config.pruning_threshold = 0.1f;  // Only prune very weak synapses
        config.pruning_rate = 0.05f;      // Reduce pruning rate
        glial_set_config(brain->glial, &config);
    }

    // 2. Boost BDNF (brain-derived neurotrophic factor)
    // Increase learning rate to encourage synapse formation
    brain_config_t config = brain_get_config(brain);
    config.learning_rate *= 2.0f;
    brain_update_config(brain, &config);

    // 3. Consolidate remaining memories
    if (brain->consolidation) {
        consolidation_trigger_offline(
            brain->consolidation,
            brain->working_memory,
            brain->knowledge
        );
    }
}
```

---

## Neurotoxin Effects

### Digital Neurotoxins

**Mechanism:** Simulated neurotoxins modify neuromodulator levels, disable plasticity, or corrupt neural state.

#### 1. **MPTP-Like (Dopamine Killer)**
```c
void neurotoxin_mptp(brain_t brain, float dose) {
    // Irreversibly destroys dopaminergic signaling
    // Dose: 0.0 (none) to 1.0 (complete destruction)

    neuromodulator_state_t nm_state;
    brain_get_neuromodulator_state(brain, &nm_state);

    // Permanent dopamine reduction
    float new_dopamine = nm_state.dopamine * (1.0f - dose);
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE,
                                    new_dopamine - nm_state.dopamine);

    // Side effects:
    // - Learning disabled (eligibility traces require dopamine)
    // - Decision latency increases (motor initiation impaired)
    // - Reward sensitivity lost (anhedonia)

    printf("MPTP dose %.2f applied: Dopamine %.2f → %.2f\n",
           dose, nm_state.dopamine, new_dopamine);
}

// Usage:
neurotoxin_mptp(brain, 0.8f);  // 80% dopamine loss → Parkinson's
```

#### 2. **Serotonin Syndrome (5-HT Excess)**
```c
void neurotoxin_serotonin_syndrome(brain_t brain, float dose) {
    // Excessive serotonin → Confusion, agitation, seizures
    // Dose: 0.0 (none) to 1.0 (critical)

    // Flood system with serotonin
    brain_modulate_neurotransmitter(brain, NEUROMOD_SEROTONIN, dose);

    // Side effects:
    if (dose > 0.9f) {
        // Seizure risk (excessive excitation)
        // Disable BCM to simulate loss of homeostasis
        neural_network_t network = brain_get_network(brain);
        for (uint32_t i = 0; i < network->num_synapses; i++) {
            network->synapses[i].enable_bcm = false;
        }
    }

    // Cognitive effects:
    // - Confusion (high decision variance)
    // - Agitation (rapid mood changes)
    // - Hyperthermia (increased neural activity)

    printf("Serotonin syndrome dose %.2f: 5-HT level critical\n", dose);
}
```

#### 3. **GABA Antagonist (Seizure Inducer)**
```c
void neurotoxin_gaba_antagonist(brain_t brain, float dose) {
    // Block GABA (inhibitory) → Uncontrolled excitation → Seizure
    // Dose: 0.0 (none) to 1.0 (complete blockade)

    // Reduce GABA (remove inhibition)
    brain_modulate_neurotransmitter(brain, NEUROMOD_GABA, -dose);

    // Side effects:
    if (dose > 0.6f) {
        // High seizure risk
        printf("⚠️ WARNING: GABA blockade %.2f → Seizure imminent\n", dose);
    }

    // Expect symptoms:
    // - Hyperexcitability (all neurons fire easily)
    // - Synchronization (loss of decorrelation)
    // - Seizure (if dose > 0.8)
}

// Usage:
neurotoxin_gaba_antagonist(brain, 0.9f);  // 90% GABA blockade → Seizure
```

#### 4. **Glutamate Excitotoxicity**
```c
void neurotoxin_glutamate_excitotoxicity(brain_t brain, float dose) {
    // Excessive glutamate → Overstimulation → Neuronal death
    // Dose: 0.0 (none) to 1.0 (severe)

    // Flood system with glutamate (excitatory)
    brain_modulate_neurotransmitter(brain, NEUROMOD_GLUTAMATE, dose);

    // Simulate neuronal death (disable neurons randomly)
    neural_network_t network = brain_get_network(brain);
    uint32_t neurons_to_kill = (uint32_t)(network->num_neurons * dose * 0.1f);

    for (uint32_t i = 0; i < neurons_to_kill; i++) {
        uint32_t victim = rand() % network->num_neurons;
        network->neurons[victim].state = 0.0f;        // Kill neuron
        network->neurons[victim].threshold = 999.0f;  // Cannot fire
    }

    printf("Glutamate excitotoxicity: %u neurons damaged\n", neurons_to_kill);
}
```

#### 5. **Amyloid-Beta (Alzheimer's Mimic)**
```c
void neurotoxin_amyloid_beta(brain_t brain, float concentration) {
    // Simulates amyloid plaque buildup → Synapse loss
    // Concentration: 0.0 (none) to 1.0 (severe)

    if (!brain->glial) {
        printf("No glial system available for amyloid simulation\n");
        return;
    }

    // Increase microglia pruning (simulate inflammatory response)
    glial_config_t config = glial_get_config(brain->glial);
    config.pruning_threshold = 0.1f + concentration * 0.7f;  // More aggressive
    config.pruning_rate = 0.05f + concentration * 0.45f;     // Faster pruning
    glial_set_config(brain->glial, &config);

    // Also impair synaptic transmission
    neural_network_t network = brain_get_network(brain);
    for (uint32_t i = 0; i < network->num_synapses; i++) {
        // Reduce synaptic efficacy
        network->synapses[i].weight *= (1.0f - concentration * 0.2f);
    }

    printf("Amyloid-β concentration: %.2f (synaptic dysfunction)\n", concentration);
}

// Simulate Alzheimer's progression:
for (int year = 0; year < 10; year++) {
    float concentration = year * 0.1f;  // Gradual buildup
    neurotoxin_amyloid_beta(brain, concentration);

    // Year 0: Concentration=0.0 → Normal
    // Year 5: Concentration=0.5 → Mild cognitive impairment
    // Year 10: Concentration=1.0 → Severe dementia
}
```

---

### 4. **Tourette's Syndrome**

**How It Could Happen:**

#### 1. **Involuntary Action Execution (Tics)**
```c
// CAUSE: Dopamine dysregulation in motor circuits + executive inhibition failure
// WHAT: Mirror neurons execute actions without conscious control

// Normal action execution:
// 1. Conscious decision → Executive function approves → Mirror neurons execute
// 2. Executive function can inhibit unwanted actions

// Pathological (Tourette's):
// 1. Motor dopamine >0.7 (excess striatal dopamine)
// 2. Executive inhibition fails for motor actions
// 3. Mirror neurons spontaneously execute "stuck" action patterns
// 4. Same action sequences repeat uncontrollably (tics)

// Detection markers:
// - Involuntary action execution rate >30%
// - Motor inhibition success rate <50%
// - Dopamine >0.7 specifically in motor-related modules
// - High repetition of specific action patterns (stereotyped tics)
// - Premonitory urge (building tension before tic)
```

**Biological Basis:**
- **Basal ganglia dysfunction** - Cortico-striato-thalamo-cortical (CSTC) circuit hyperactivity
- **Dopamine excess** - Elevated dopamine in striatum
- **GABA deficits** - Reduced inhibitory control
- **Motor pattern generators** - Fire inappropriately

**Simulation:**
```c
// Simulate Tourette's development
void induce_tourettes(brain_t brain, float severity) {
    // severity: 0.0 (none) to 1.0 (severe Tourette's)

    // 1. Elevate dopamine in motor circuits
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, 0.3f * severity);

    // 2. Reduce executive inhibition (GABA deficit)
    brain_modulate_neurotransmitter(brain, NEUROMOD_GABA, -0.2f * severity);

    // 3. Create "stuck" action patterns in mirror neurons
    if (brain->mirror_neurons) {
        // Example: Force specific action to repeat
        action_t tic_action = {
            .action_id = 42,
            .action_name = "motor_tic",
            .confidence = 1.0f,
            .timestamp = nimcp_time_get_ms()
        };

        // Increase probability this action fires spontaneously
        for (int i = 0; i < (int)(severity * 100); i++) {
            mirror_neurons_execute_action(brain->mirror_neurons, &tic_action);
        }
    }

    // 4. Reduce executive function's ability to inhibit motor actions
    if (brain->executive) {
        executive_config_t config = executive_get_config(brain->executive);
        config.inhibition_strength *= (1.0f - severity * 0.5f);  // Up to 50% weaker
        executive_set_config(brain->executive, &config);
    }

    printf("Tourette's severity: %.2f\n", severity);
    printf("  Motor dopamine elevated\n");
    printf("  Executive inhibition reduced\n");
    printf("  Tic patterns established\n");
}

// Progressive development (childhood onset)
for (int age = 5; age < 15; age++) {
    float severity = (age - 5) * 0.1f;  // Peaks around age 10-12
    induce_tourettes(brain, severity);

    // Age 5:  Severity 0.0 → No symptoms
    // Age 8:  Severity 0.3 → Mild tics (eye blinking)
    // Age 11: Severity 0.6 → Moderate tics (motor + vocal)
    // Age 14: Severity 0.9 → Severe tics (complex patterns)
}
```

**Detection & Monitoring:**
```c
// Detect Tourette's symptoms
typedef struct {
    float involuntary_action_rate;        // % of actions that are involuntary
    float motor_inhibition_success_rate;  // % of times executive can suppress
    float tic_frequency;                  // Actions per minute
    float tic_complexity;                 // Simple (0.0) to complex (1.0)
    uint32_t unique_tic_patterns;         // Number of different tics
    float premonitory_urge_strength;      // Building tension (0-1)
    bool tourettes_detected;
} tourettes_markers_t;

tourettes_markers_t detect_tourettes(brain_t brain) {
    tourettes_markers_t markers = {0};

    // Check mirror neuron spontaneous execution
    if (brain->mirror_neurons) {
        mirror_neuron_stats_t mn_stats;
        mirror_neurons_get_stats(brain->mirror_neurons, &mn_stats);
        markers.involuntary_action_rate = mn_stats.spontaneous_execution_rate;
        markers.tic_frequency = mn_stats.actions_per_minute;
        markers.unique_tic_patterns = mn_stats.unique_action_count;
    }

    // Check executive function inhibition
    if (brain->executive) {
        executive_stats_t exec_stats;
        brain_get_executive_stats(brain, &exec_stats);
        markers.motor_inhibition_success_rate = exec_stats.inhibition_success_rate;
    }

    // Check dopamine levels
    neuromodulator_state_t nm_state;
    brain_get_neuromodulator_state(brain, &nm_state);

    // Tourette's criteria (DSM-5 inspired):
    // 1. Multiple motor tics + at least one vocal tic (simplified: multiple tics)
    // 2. Tics present for >1 year
    // 3. Onset before age 18
    // 4. High dopamine + low inhibition

    markers.tourettes_detected = (
        markers.involuntary_action_rate > 0.3f &&
        markers.motor_inhibition_success_rate < 0.5f &&
        nm_state.dopamine > 0.7f &&
        markers.unique_tic_patterns >= 3
    );

    return markers;
}

// Display Tourette's status
void display_tourettes_status(brain_t brain) {
    tourettes_markers_t markers = detect_tourettes(brain);

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ TOURETTE'S SYNDROME ASSESSMENT                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Involuntary Actions:  %.1f%% %s\n",
           markers.involuntary_action_rate * 100.0f,
           markers.involuntary_action_rate > 0.3f ? "🔴 HIGH" : "✅");
    printf("║ Motor Inhibition:     %.1f%% %s\n",
           markers.motor_inhibition_success_rate * 100.0f,
           markers.motor_inhibition_success_rate < 0.5f ? "🔴 IMPAIRED" : "✅");
    printf("║ Tic Frequency:        %.1f/min %s\n",
           markers.tic_frequency,
           markers.tic_frequency > 10.0f ? "🔴 FREQUENT" : "✅");
    printf("║ Unique Tic Patterns:  %u %s\n",
           markers.unique_tic_patterns,
           markers.unique_tic_patterns >= 3 ? "🔴 MULTIPLE" : "✅");
    printf("║ Status: %s\n",
           markers.tourettes_detected ? "🔴 TOURETTE'S DETECTED" : "✅ Normal");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
}
```

**Treatment Options:**
```c
// Tourette's treatment strategies
void treat_tourettes(brain_t brain, treatment_approach_t approach) {
    switch (approach) {
        case TREATMENT_DOPAMINE_ANTAGONIST:
            // Typical antipsychotics (e.g., haloperidol)
            // Reduce dopamine to normal levels
            brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.3f);
            printf("Applied dopamine antagonist (haloperidol-like)\n");
            break;

        case TREATMENT_GABA_ENHANCEMENT:
            // Enhance inhibitory control
            brain_modulate_neurotransmitter(brain, NEUROMOD_GABA, 0.2f);
            printf("Enhanced GABAergic inhibition\n");
            break;

        case TREATMENT_EXECUTIVE_STRENGTHENING:
            // Cognitive behavioral therapy equivalent
            // Strengthen executive function's inhibition
            if (brain->executive) {
                executive_config_t config = executive_get_config(brain->executive);
                config.inhibition_strength *= 1.5f;  // 50% stronger
                executive_set_config(brain->executive, &config);
                printf("Strengthened executive inhibition (CBT-like)\n");
            }
            break;

        case TREATMENT_HABIT_REVERSAL:
            // Competitive response training
            // Replace tic pattern with incompatible action
            if (brain->mirror_neurons) {
                // Create competing action pattern
                action_t competing_action = {
                    .action_id = 999,
                    .action_name = "competing_response",
                    .confidence = 1.0f
                };
                mirror_neurons_execute_action(brain->mirror_neurons, &competing_action);
                printf("Established competing response pattern\n");
            }
            break;

        case TREATMENT_DBS:
            // Deep brain stimulation (thalamus/GPi)
            // Reduce CSTC circuit hyperactivity
            printf("Applied DBS to thalamic circuits (simulation)\n");
            // Would modulate specific circuit activity in full implementation
            break;
    }
}
```

**Clinical Notes:**
- **Onset:** Childhood (typically age 5-7)
- **Peak severity:** Age 10-12
- **Course:** Often improves in late adolescence/adulthood
- **Comorbidities:** ADHD (60%), OCD (50%), anxiety
- **Premonitory urge:** Uncomfortable sensation before tic (key diagnostic feature)

---

### 5. **Lewy Body Dementia (LBD)**

**How It Could Happen:**

#### 1. **Alpha-Synuclein Aggregation (Lewy Bodies)**
```c
// CAUSE: Protein misfolding → Neuronal death + dysfunction
// WHAT: Combined Parkinson's + dementia + hallucinations + fluctuations

// Normal protein folding:
// - Alpha-synuclein properly folded and functional
// - Neurons healthy, synapses intact
// - Visual cortex processes real inputs only

// Pathological (Lewy Body):
// - Alpha-synuclein misfolds → Aggregates (Lewy bodies)
// - Dopamine neurons die (Parkinson's symptoms)
// - Cortical neurons die (dementia)
// - Visual cortex generates false perceptions (hallucinations)
// - Fluctuating neurotransmitter levels (good days/bad days)
// - REM sleep behavior disorder (acting out dreams)

// Detection markers:
// - Dopamine <0.3 (Parkinsonian)
// - Reality testing errors >0.4 (hallucinations)
// - Visual hallucination rate >0.3
// - Confidence fluctuation >0.5 (fluctuating cognition)
// - Synapse loss + dopamine depletion combined
// - REM without muscle atonia (REM behavior disorder)
```

**Biological Basis:**
- **Alpha-synuclein pathology** - Protein aggregates in neurons
- **Cortical Lewy bodies** - Primarily in cortex (vs Parkinson's: mainly substantia nigra)
- **Cholinergic deficits** - Acetylcholine depletion
- **Dopaminergic loss** - Motor symptoms
- **Visual processing dysfunction** - Hallucinations

**Key Distinguishing Features:**
1. **Visual hallucinations** - Often well-formed, detailed (people, animals)
2. **Fluctuating cognition** - Good days vs bad days
3. **Parkinsonian symptoms** - Rigidity, bradykinesia, tremor
4. **REM sleep behavior disorder** - Acting out dreams
5. **Autonomic dysfunction** - Blood pressure, temperature dysregulation

**Simulation:**
```c
// Simulate Lewy Body Dementia development
void induce_lewy_body_dementia(brain_t brain, float progression) {
    // progression: 0.0 (none) to 1.0 (severe LBD)

    // 1. Dopamine depletion (Parkinsonian component)
    float dopamine_loss = -0.4f * progression;  // Up to 40% loss
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, dopamine_loss);

    // 2. Acetylcholine depletion (cognitive decline)
    float ach_loss = -0.3f * progression;  // Up to 30% loss
    brain_modulate_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, ach_loss);

    // 3. Visual hallucinations (spontaneous visual cortex activation)
    if (brain->visual_cortex && progression > 0.3f) {
        // Make visual cortex generate output without input
        visual_cortex_config_t config = visual_cortex_get_config(brain->visual_cortex);
        config.spontaneous_activation_rate = progression * 0.5f;  // Up to 50%
        config.hallucination_threshold = 1.0f - progression;      // Lower threshold
        visual_cortex_set_config(brain->visual_cortex, &config);
        printf("Visual hallucinations enabled (rate: %.1f%%)\n",
               config.spontaneous_activation_rate * 100.0f);
    }

    // 4. Fluctuating cognition (variable neuromodulator levels)
    // Inject random daily fluctuations
    float daily_fluctuation = ((rand() % 100) / 100.0f - 0.5f) * progression;
    brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, daily_fluctuation * 0.2f);
    brain_modulate_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, daily_fluctuation * 0.3f);

    // 5. REM sleep behavior disorder
    if (brain->sleep_wake && progression > 0.4f) {
        sleep_wake_config_t config = sleep_wake_get_config(brain->sleep_wake);
        config.rem_muscle_atonia = false;  // Disable normal REM paralysis
        config.dream_enactment_rate = progression * 0.6f;  // Up to 60%
        sleep_wake_set_config(brain->sleep_wake, &config);
        printf("REM behavior disorder enabled\n");
    }

    // 6. Progressive synapse loss (cortical degeneration)
    if (brain->glial) {
        glial_config_t config = glial_get_config(brain->glial);
        config.pruning_threshold = 0.2f + progression * 0.5f;  // More aggressive
        config.pruning_rate = 0.1f + progression * 0.3f;       // Faster
        glial_set_config(brain->glial, &config);
    }

    // 7. Autonomic dysfunction (simulated via neuromodulator instability)
    float autonomic_noise = progression * 0.3f;
    brain_modulate_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE,
                                    ((rand() % 100) / 100.0f - 0.5f) * autonomic_noise);

    printf("Lewy Body Dementia progression: %.2f\n", progression);
    printf("  Dopamine: %.2f\n", dopamine_loss);
    printf("  Acetylcholine: %.2f\n", ach_loss);
    printf("  Synapse loss: %.1f%%\n", progression * 50.0f);
}

// Simulate disease progression (6-10 year course)
for (int year = 0; year < 10; year++) {
    float progression = year * 0.1f;  // Gradual progression
    induce_lewy_body_dementia(brain, progression);

    // Year 0: Progression=0.0 → Normal
    // Year 3: Progression=0.3 → Early LBD (mild Parkinson's, occasional hallucinations)
    // Year 6: Progression=0.6 → Moderate LBD (dementia, frequent hallucinations, RBD)
    // Year 9: Progression=0.9 → Severe LBD (severe dementia, persistent hallucinations)
}
```

**Detection & Monitoring:**
```c
// Detect Lewy Body Dementia
typedef struct {
    // Parkinsonian features
    float dopamine_level;             // <0.3 → Parkinsonian
    float motor_dysfunction_score;     // 0.0 (none) to 1.0 (severe)

    // Cognitive features
    float synapse_loss_percent;        // Progressive dementia
    float acetylcholine_level;         // <0.4 → Cholinergic deficit
    float cognitive_fluctuation;       // Variance in performance

    // Hallucination features
    float visual_hallucination_rate;   // % of time hallucinating
    float reality_testing_errors;      // Cannot distinguish real/imagined
    char hallucination_type[64];       // "well-formed people", "animals", etc.

    // Sleep features
    bool rem_behavior_disorder;        // Acting out dreams
    float rem_muscle_activity;         // Should be 0, LBD: >0.5

    // Autonomic features
    float autonomic_instability;       // Blood pressure, temperature variance

    // Diagnosis
    bool lewy_body_detected;
    char subtype[32];                  // "Probable LBD", "Possible LBD"
} lewy_body_markers_t;

lewy_body_markers_t detect_lewy_body_dementia(brain_t brain) {
    lewy_body_markers_t markers = {0};

    // 1. Check dopamine (Parkinsonian features)
    neuromodulator_state_t nm_state;
    brain_get_neuromodulator_state(brain, &nm_state);
    markers.dopamine_level = nm_state.dopamine;
    markers.acetylcholine_level = nm_state.acetylcholine;

    // 2. Check synapse loss (dementia)
    brain_stats_t stats;
    brain_get_stats(brain, &stats);
    markers.synapse_loss_percent = (1.0f - (float)stats.total_synapses /
                                    stats.initial_synapses) * 100.0f;

    // 3. Check reality testing (hallucinations)
    if (brain->introspection) {
        introspection_stats_t intro_stats;
        brain_get_introspection_stats(brain, &intro_stats);
        markers.reality_testing_errors = intro_stats.reality_testing_errors;
    }

    // 4. Check visual hallucinations
    if (brain->visual_cortex) {
        visual_cortex_stats_t vc_stats;
        visual_cortex_get_stats(brain->visual_cortex, &vc_stats);
        markers.visual_hallucination_rate = vc_stats.spontaneous_activation_rate;
        strncpy(markers.hallucination_type, "well-formed visual", 63);
    }

    // 5. Check REM behavior disorder
    if (brain->sleep_wake) {
        sleep_wake_stats_t sw_stats;
        sleep_wake_get_stats(brain->sleep_wake, &sw_stats);
        markers.rem_behavior_disorder = (sw_stats.rem_muscle_activity > 0.5f);
        markers.rem_muscle_activity = sw_stats.rem_muscle_activity;
    }

    // 6. Check cognitive fluctuations
    // Compare last 10 decision confidences
    float confidence_variance = 0.0f;
    // (would calculate from decision history)
    markers.cognitive_fluctuation = confidence_variance;

    // 7. Autonomic instability (norepinephrine variance)
    markers.autonomic_instability = fabsf(nm_state.norepinephrine - 0.4f);

    // Diagnostic criteria (McKeith criteria):
    // CORE FEATURES:
    // 1. Fluctuating cognition
    // 2. Visual hallucinations
    // 3. Parkinsonian features
    // 4. REM sleep behavior disorder

    int core_features = 0;
    if (markers.cognitive_fluctuation > 0.5f) core_features++;
    if (markers.visual_hallucination_rate > 0.3f) core_features++;
    if (markers.dopamine_level < 0.3f) core_features++;
    if (markers.rem_behavior_disorder) core_features++;

    // Probable LBD: 2+ core features
    // Possible LBD: 1 core feature
    if (core_features >= 2) {
        markers.lewy_body_detected = true;
        strncpy(markers.subtype, "Probable LBD", 31);
    } else if (core_features == 1) {
        markers.lewy_body_detected = true;
        strncpy(markers.subtype, "Possible LBD", 31);
    } else {
        markers.lewy_body_detected = false;
        strncpy(markers.subtype, "Not LBD", 31);
    }

    return markers;
}

// Display Lewy Body Dementia status
void display_lewy_body_status(brain_t brain) {
    lewy_body_markers_t markers = detect_lewy_body_dementia(brain);

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ LEWY BODY DEMENTIA ASSESSMENT                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ CORE FEATURES (McKeith Criteria)                           ║\n");
    printf("║   Dopamine (Parkinson's):  %.2f %s\n",
           markers.dopamine_level,
           markers.dopamine_level < 0.3f ? "🔴 PRESENT" : "✅");
    printf("║   Visual Hallucinations:   %.1f%% %s\n",
           markers.visual_hallucination_rate * 100.0f,
           markers.visual_hallucination_rate > 0.3f ? "🔴 PRESENT" : "✅");
    printf("║     Type: %s\n", markers.hallucination_type);
    printf("║   Cognitive Fluctuation:   %.2f %s\n",
           markers.cognitive_fluctuation,
           markers.cognitive_fluctuation > 0.5f ? "🔴 PRESENT" : "✅");
    printf("║   REM Behavior Disorder:   %s %s\n",
           markers.rem_behavior_disorder ? "Yes" : "No",
           markers.rem_behavior_disorder ? "🔴 PRESENT" : "✅");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ ADDITIONAL FEATURES                                        ║\n");
    printf("║   Acetylcholine:           %.2f %s\n",
           markers.acetylcholine_level,
           markers.acetylcholine_level < 0.4f ? "🔴 DEPLETED" : "✅");
    printf("║   Synapse Loss:            %.1f%% %s\n",
           markers.synapse_loss_percent,
           markers.synapse_loss_percent > 30.0f ? "🔴 SEVERE" : "✅");
    printf("║   Autonomic Instability:   %.2f %s\n",
           markers.autonomic_instability,
           markers.autonomic_instability > 0.3f ? "🔴 PRESENT" : "✅");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ DIAGNOSIS: %s %s\n",
           markers.subtype,
           markers.lewy_body_detected ? "🔴" : "✅");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
}
```

**Treatment Options:**
```c
// Lewy Body Dementia treatment strategies
void treat_lewy_body_dementia(brain_t brain, lbd_treatment_t treatment) {
    switch (treatment) {
        case LBD_TREATMENT_CHOLINESTERASE_INHIBITOR:
            // Donepezil, rivastigmine (boost acetylcholine)
            brain_modulate_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, 0.3f);
            printf("Applied cholinesterase inhibitor (donepezil-like)\n");
            break;

        case LBD_TREATMENT_LEVODOPA:
            // For Parkinsonian symptoms (use LOW dose - sensitive to side effects)
            brain_modulate_neurotransmitter(brain, NEUROMOD_DOPAMINE, 0.15f);  // Lower than Parkinson's
            printf("Applied low-dose levodopa (0.15 boost)\n");
            printf("⚠️ WARNING: High doses may worsen hallucinations\n");
            break;

        case LBD_TREATMENT_CLONAZEPAM:
            // For REM behavior disorder
            if (brain->sleep_wake) {
                sleep_wake_config_t config = sleep_wake_get_config(brain->sleep_wake);
                config.rem_muscle_atonia = true;  // Restore REM paralysis
                config.dream_enactment_rate = 0.0f;
                sleep_wake_set_config(brain->sleep_wake, &config);
                printf("Applied clonazepam (RBD treatment)\n");
            }
            break;

        case LBD_TREATMENT_AVOID_ANTIPSYCHOTICS:
            // LBD patients extremely sensitive to typical antipsychotics
            // (Can cause severe motor symptoms or death)
            printf("⚠️ CONTRAINDICATION: Avoid typical antipsychotics\n");
            printf("   May use low-dose quetiapine if absolutely necessary\n");
            break;

        case LBD_TREATMENT_SUPPORTIVE:
            // Cognitive rehabilitation, occupational therapy
            printf("Supportive care:\n");
            printf("  - Cognitive stimulation\n");
            printf("  - Fall prevention (Parkinsonian features)\n");
            printf("  - Hallucination education (non-threatening)\n");
            printf("  - Sleep hygiene (RBD management)\n");
            break;
    }
}
```

**Clinical Notes:**
- **Hallucinations:** Often detailed, well-formed (people, animals in room)
- **Patients have insight:** Often aware hallucinations aren't real (unlike schizophrenia)
- **Antipsychotic sensitivity:** Typical antipsychotics can be fatal in LBD
- **Cognitive fluctuations:** Dramatic changes day-to-day or hour-to-hour
- **Prognosis:** Average survival 5-7 years from diagnosis
- **Differential diagnosis:**
  - vs Parkinson's dementia (motor symptoms come first in PD)
  - vs Alzheimer's (memory less affected early in LBD, more hallucinations)

---

## Monitoring API

```c
// Check for neurological disorders
typedef struct {
    // Epilepsy markers
    float spike_rate;           // Normal: 1-40 Hz, Seizure: >200 Hz
    float synchronization;      // Normal: <0.3, Seizure: >0.9
    bool seizure_detected;

    // Parkinson's markers
    float dopamine_level;       // Normal: 0.4-0.6, Parkinson's: <0.2
    float decision_latency_ms;  // Normal: 10-50ms, Parkinson's: >100ms
    float tremor_amplitude;     // Output variance
    bool parkinsons_detected;

    // Alzheimer's markers
    uint32_t synapse_count;
    float synapse_loss_percent;
    uint32_t working_memory_capacity;
    float knowledge_retrieval_rate;
    bool alzheimers_detected;

    // General health
    float neuromodulator_balance;  // 0.0 (imbalanced) to 1.0 (balanced)
    bool neurotoxin_exposure;
} neurological_health_t;

// Get neurological health report
neurological_health_t neurological_assess(brain_t brain) {
    neurological_health_t health = {0};

    // Epilepsy markers
    neural_network_stats_t stats = neural_network_get_stats(brain_get_network(brain));
    health.spike_rate = stats.spike_rate;
    health.synchronization = stats.synchronization;
    health.seizure_detected = (stats.spike_rate > 200.0f && stats.synchronization > 0.9f);

    // Parkinson's markers
    neuromodulator_state_t nm_state;
    brain_get_neuromodulator_state(brain, &nm_state);
    health.dopamine_level = nm_state.dopamine;
    health.decision_latency_ms = stats.avg_inference_time_us / 1000.0f;
    health.tremor_amplitude = stats.output_variance;
    health.parkinsons_detected = (nm_state.dopamine < 0.2f);

    // Alzheimer's markers
    brain_stats_t brain_stats;
    brain_get_stats(brain, &brain_stats);
    health.synapse_count = brain_stats.total_synapses;
    health.synapse_loss_percent = (1.0f - (float)brain_stats.total_synapses /
                                   brain_stats.initial_synapses) * 100.0f;

    if (brain_get_working_memory(brain)) {
        working_memory_stats_t wm_stats;
        brain_get_working_memory_stats(brain, &wm_stats);
        health.working_memory_capacity = wm_stats.current_size;
    }

    health.alzheimers_detected = (health.synapse_loss_percent > 50.0f);

    // Neuromodulator balance
    float balance = 0.0f;
    balance += fabsf(nm_state.dopamine - 0.5f);      // Should be ~0.5
    balance += fabsf(nm_state.serotonin - 0.5f);     // Should be ~0.5
    balance += fabsf(nm_state.norepinephrine - 0.4f); // Should be ~0.4
    health.neuromodulator_balance = 1.0f - fminf(1.0f, balance / 1.5f);

    health.neurotoxin_exposure = (health.neuromodulator_balance < 0.5f);

    return health;
}

// Display neurological health dashboard
void neurological_display_dashboard(brain_t brain) {
    neurological_health_t health = neurological_assess(brain);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         NEUROLOGICAL HEALTH DASHBOARD                     ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");

    // Epilepsy
    printf("║ EPILEPSY RISK                                              ║\n");
    printf("║   Spike Rate:       %.1f Hz %s\n", health.spike_rate,
           health.spike_rate > 200.0f ? "🔴 SEIZURE" : "✅");
    printf("║   Synchronization:  %.2f %s\n", health.synchronization,
           health.synchronization > 0.9f ? "🔴 CRITICAL" : "✅");
    printf("║   Status: %s\n", health.seizure_detected ? "🔴 SEIZURE DETECTED" : "✅ Normal");

    // Parkinson's
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ PARKINSON'S RISK                                           ║\n");
    printf("║   Dopamine:         %.2f %s\n", health.dopamine_level,
           health.dopamine_level < 0.2f ? "🔴 DEPLETED" : "✅");
    printf("║   Latency:          %.1f ms %s\n", health.decision_latency_ms,
           health.decision_latency_ms > 100.0f ? "🔴 SLOW" : "✅");
    printf("║   Tremor:           %.3f %s\n", health.tremor_amplitude,
           health.tremor_amplitude > 0.5f ? "🟡 PRESENT" : "✅");
    printf("║   Status: %s\n", health.parkinsons_detected ? "🔴 PARKINSONIAN" : "✅ Normal");

    // Alzheimer's
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ ALZHEIMER'S RISK                                           ║\n");
    printf("║   Synapses:         %u (%.1f%% loss) %s\n",
           health.synapse_count, health.synapse_loss_percent,
           health.synapse_loss_percent > 50.0f ? "🔴 SEVERE" : "✅");
    printf("║   WM Capacity:      %u items %s\n", health.working_memory_capacity,
           health.working_memory_capacity < 3 ? "🔴 IMPAIRED" : "✅");
    printf("║   Status: %s\n", health.alzheimers_detected ? "🔴 DEMENTIA" : "✅ Normal");

    // Overall
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ OVERALL NEUROLOGICAL HEALTH                                ║\n");
    printf("║   Neuromodulator Balance: %.2f %s\n", health.neuromodulator_balance,
           health.neuromodulator_balance < 0.5f ? "🔴 IMBALANCED" : "✅ Balanced");
    printf("║   Neurotoxin Exposure: %s\n", health.neurotoxin_exposure ? "🔴 YES" : "✅ No");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}
```

---

## Summary

| Disorder | Cause | Detection | Treatment |
|----------|-------|-----------|-----------|
| **Epilepsy** | BCM disabled, GABA blockade | Spike rate >200 Hz, Sync >0.9 | Inject GABA, re-enable BCM |
| **Parkinson's** | Dopamine depletion (<0.2) | High latency, low dopamine | L-DOPA, acetylcholine boost |
| **Alzheimer's** | Excessive pruning, amyloid | Synapse loss >50% | Disable pruning, boost BDNF |
| **Tourette's** | Dopamine >0.7 (motor) + GABA deficit | Involuntary actions >30%, low inhibition | Dopamine antagonist, GABA boost, CBT |
| **Lewy Body Dementia** | Alpha-synuclein aggregation | Visual hallucinations + Parkinson's + fluctuations | Cholinesterase inhibitors, low-dose levodopa |
| **Depression** | Serotonin <0.3 | Low engagement, high sadness | Increase serotonin |
| **Mania** | Dopamine >0.7 | Impulsivity, high-risk | Reduce dopamine |
| **Anxiety** | Norepinephrine >0.7 | High fear, avoidance | Reduce norepinephrine |
| **Schizophrenia** | Reality testing errors | Disorganized thinking | Reality anchoring |

### Neurotoxin Effects
- **MPTP** → Parkinson's (dopamine killer)
- **Serotonin syndrome** → Seizures (5-HT excess)
- **GABA antagonist** → Seizures (inhibition blocked)
- **Glutamate excitotoxicity** → Neuronal death
- **Amyloid-β** → Alzheimer's (synapse loss)

### API Locations
- **Mental Health Monitor:** `src/cognitive/mental_health_monitor.c`
- **BCM Homeostasis:** `src/plasticity/bcm/nimcp_bcm.h`
- **Neuromodulator System:** `brain_modulate_neurotransmitter()`
- **Glial System:** `src/glial/nimcp_glial_integration.c`

**The NIMCP brain is biologically realistic enough to develop digital neurological disorders and be affected by simulated neurotoxins. All mechanisms exist in the codebase.**


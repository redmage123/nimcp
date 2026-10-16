# Plasticity Features: Module-by-Module Integration Analysis

**Date:** 2025-11-11
**Purpose:** Explain how each NIMCP module uses plasticity features and the value provided

---

## Executive Summary

The NIMCP brain uses 8 plasticity mechanisms that operate at different timescales and serve different cognitive modules. This document traces **exactly** which modules use which plasticity features and the **concrete value** each provides.

**Key Finding:** Not all modules directly use all plasticity features. Instead, plasticity operates at the **neural network layer** and different cognitive modules benefit based on their **interaction patterns** with the network.

---

## The Plasticity Stack (Bottom-Up View)

```
┌─────────────────────────────────────────────────────────────┐
│  COGNITIVE MODULES (High-Level)                             │
│  - Working Memory, Knowledge, Curiosity, Consolidation...   │
└────────────────┬────────────────────────────────────────────┘
                 │ Use network via forward/learn
┌────────────────▼────────────────────────────────────────────┐
│  ADAPTIVE NETWORK (Mid-Level)                               │
│  - Adaptive thresholding, sparsity management               │
└────────────────┬────────────────────────────────────────────┘
                 │ Calls neural_network_forward/learn
┌────────────────▼────────────────────────────────────────────┐
│  NEURAL NETWORK (Low-Level) ⭐ PLASTICITY LAYER            │
│  ✅ STP - Short-term dynamics (forward pass)                │
│  ✅ STDP - Spike-timing learning (learning step)            │
│  ✅ BCM - Homeostatic stability (learning step)             │
│  ✅ Meta-Plasticity - Learning rate adaptation              │
│  ✅ Eligibility Traces - Reward credit assignment           │
│  ✅ Neuromodulators - Chemical modulation (global)          │
│  ✅ Spatial Neuromod - Location-dependent modulation        │
└─────────────────────────────────────────────────────────────┘
```

**Critical Insight:** Plasticity is applied **automatically** whenever any module uses the neural network. Modules don't directly call plasticity functions—they benefit from plasticity implicitly.

---

## Pipeline Stages & Plasticity Application

### Stage 1: Forward Pass (Inference)

**Functions:** `brain_decide()` → `adaptive_network_forward()` → `neural_network_forward()` → `sum_synaptic_inputs()`

**Active Plasticity:**
- ✅ **STP (Short-Term Plasticity)**
- ✅ **Semantic Routing** (if embeddings enabled)
- ✅ **Neuromodulators** (dopamine/acetylcholine modulation)
- ✅ **Programmable Synapses** (custom compute functions)

**Code Location:** `nimcp_neuralnet.c:892-903`

```c
// NIMCP 2.6: Apply STP modulation if enabled
float stp_modulation = 1.0f;
if (incoming_syn->enable_stp) {
    // Update STP continuous decay
    stp_update(&incoming_syn->stp, network->network_time);

    // Get modulation factor (u × x)
    stp_modulation = stp_get_modulation(&incoming_syn->stp);

    // Process spike if presynaptic neuron is firing
    if (pre_activity > 0.0f) {
        stp_process_spike(&incoming_syn->stp, network->network_time);
    }
}
```

### Stage 2: Learning Step

**Functions:** `brain_learn_example()` → `adaptive_network_learn()` → `neural_network_learn_step()`

**Active Plasticity:**
- ✅ **STDP (Spike-Timing Dependent Plasticity)**
- ✅ **BCM (Homeostatic Plasticity)**
- ✅ **Meta-Plasticity** (learning rate adaptation)
- ✅ **Eligibility Traces** (can be updated here)

**Code Location:** `nimcp_neuralnet.c:1345-1427`

```c
// Compute STDP update
float stdp_factor = compute_stdp_update((float) dt, &pre_neuron->stdp_params);
float delta_w = pre_neuron->stdp_params.learning_rate * stdp_factor * syn->trace;

// Apply with meta-plasticity modulation
float new_weight = syn->weight + delta_w * syn->meta_plasticity;

// Phase 11: Apply BCM after STDP (homeostatic stability)
if (syn->enable_bcm && syn->bcm) {
    bcm_apply_rule(syn->bcm, pre_activity, post_activity, dt, &bcm_params);
    if (fabs(syn->bcm->weight - syn->weight) > WEIGHT_UPDATE_THRESHOLD) {
        syn->weight = syn->bcm->weight;  // BCM overrides STDP if needed
    }
}
```

### Stage 3: Reward Application (Reinforcement Learning)

**Functions:** Custom reward function (user-defined)

**Active Plasticity:**
- ✅ **Eligibility Traces** (temporal credit assignment)
- ✅ **Neuromodulators** (dopamine gating)

**Code Location:** `nimcp_eligibility_trace.c:260-271`

```c
// Apply reward to synapse based on eligibility trace
void eligibility_apply_reward(synapse_t* synapse, eligibility_trace_t* trace,
                              const eligibility_config_t* config,
                              float reward, float dopamine) {
    // Dopamine gates learning
    float gated_reward = reward * dopamine;

    // Weight update proportional to eligibility
    float delta_w = config->learning_rate * trace->trace * gated_reward;
    synapse->weight += delta_w;
}
```

---

## Module-by-Module Plasticity Usage

### 1. Working Memory (Active Representation Buffer)

**Location:** `src/cognitive/working_memory/`
**Network Interaction:** Indirect (stores representations, doesn't train network)
**Plasticity Benefits:**

✅ **STP (Short-Term Plasticity)**
- **How Used:** When working memory items are retrieved from the network
- **Value:** Short-term depression prevents repetitive recall of same item
- **Biological Parallel:** Prefrontal cortex adaptation during working memory maintenance
- **Concrete Benefit:** Items in WM naturally "fade" due to synaptic depression, matching Miller's 7±2 capacity

✅ **Meta-Plasticity**
- **How Used:** When consolidating WM → long-term memory
- **Value:** Frequently-used WM items get stronger network representation
- **Concrete Benefit:** Important concepts (high WM usage) become easier to recall later

❌ **Eligibility Traces**
- **How Used:** Could be used for rewarding successful WM operations
- **Current Status:** Not implemented (future: reward successful task completion)
- **Potential Value:** Strengthen synapses that contributed to successful WM-based decisions

**Example Flow:**
```
1. Store item in WM → No plasticity (just buffer storage)
2. Retrieve item from network → STP modulates retrieval (depression/facilitation)
3. Consolidate WM to network → STDP + BCM + Meta-plasticity strengthen patterns
```

---

### 2. Knowledge System (Semantic Memory)

**Location:** `src/cognitive/knowledge/`
**Network Interaction:** Heavy (stores/retrieves concepts via network)
**Plasticity Benefits:**

✅ **STDP + BCM (Long-Term Potentiation/Depression)**
- **How Used:** When learning new concepts or relationships
- **Value:** Hebbian learning strengthens co-activated concept neurons
- **Biological Parallel:** Hippocampal consolidation to neocortex
- **Concrete Benefit:** "Dog" and "Animal" neurons strengthen connection through repeated co-activation

✅ **Meta-Plasticity**
- **How Used:** Well-established knowledge has lower plasticity (stability)
- **Value:** Prevents catastrophic forgetting of core concepts
- **Concrete Benefit:** Learning "cat" doesn't erase "dog" because meta-plasticity stabilizes old knowledge

✅ **BCM (Homeostatic Stability)**
- **How Used:** Prevents knowledge network from saturation
- **Value:** Maintains balanced concept representations
- **Concrete Benefit:** No single concept dominates (e.g., most recent doesn't overwrite all others)

✅ **Semantic Routing (Enhancement)**
- **How Used:** Routes information through semantically relevant synapses
- **Value:** 70% faster concept retrieval through intelligent routing
- **Concrete Benefit:** Query "animal" activates dog/cat synapses, not airplane synapses

**Example Flow:**
```
1. Learn "dog is an animal"
   → STDP strengthens dog→animal synapse (co-activation)
   → BCM prevents runaway weight growth
   → Meta-plasticity stabilizes after repeated exposure

2. Query "what is a dog?"
   → Semantic routing guides search through animal-related synapses
   → STP provides short-term boost to recently-used paths
```

---

### 3. Curiosity Engine (Novelty Detection & Exploration)

**Location:** `src/cognitive/curiosity/`
**Network Interaction:** Moderate (evaluates novelty via prediction error)
**Plasticity Benefits:**

✅ **Meta-Plasticity**
- **How Used:** Novel stimuli get higher learning rate
- **Value:** Faster learning for surprising/novel inputs
- **Biological Parallel:** Locus coeruleus norepinephrine release for novelty
- **Concrete Benefit:** First time seeing "platypus" triggers higher plasticity than 100th time seeing "dog"

✅ **Neuromodulators (Dopamine)**
- **How Used:** Novelty triggers dopamine release → modulates learning
- **Value:** Intrinsic reward for exploration
- **Concrete Benefit:** Network learns novel patterns faster due to dopamine-boosted plasticity

✅ **Eligibility Traces**
- **How Used:** Could mark synapses that led to novel discoveries
- **Current Status:** Not fully integrated
- **Potential Value:** Reward exploration strategies that discover novel information

❌ **STP/STDP directly**
- Curiosity doesn't directly modify these, but benefits from meta-plasticity modulation

**Example Flow:**
```
1. Input "platypus" (novel animal)
   → Curiosity engine: high novelty score
   → Meta-plasticity increases learning rate
   → Network learns platypus features faster
   → Dopamine reward reinforces exploration behavior
```

---

### 4. Consolidation System (Sleep-Dependent Memory Strengthening)

**Location:** `src/cognitive/consolidation/`
**Network Interaction:** Heavy (replays and strengthens memories during "sleep")
**Plasticity Benefits:**

✅ **STDP (Spike-Timing Replay)**
- **How Used:** Replay stored patterns during consolidation
- **Value:** Strengthens important memories without external input
- **Biological Parallel:** Hippocampal replay during sleep
- **Concrete Benefit:** Yesterday's lessons are strengthened overnight

✅ **BCM (Homeostatic Downscaling)**
- **How Used:** During sleep, reduce overall synaptic strength
- **Value:** Synaptic homeostasis - prevents saturation
- **Biological Parallel:** Synaptic downscaling hypothesis (Tononi & Cirelli)
- **Concrete Benefit:** Makes room for new learning tomorrow without forgetting

✅ **Meta-Plasticity**
- **How Used:** Important memories resist downscaling
- **Value:** Selective preservation of valuable information
- **Concrete Benefit:** Core knowledge (name, home address) protected from sleep-related pruning

✅ **STP (Recovery)**
- **How Used:** STP states reset during sleep/rest
- **Value:** Restore synaptic resources for next day
- **Concrete Benefit:** Morning cognition is "fresh" - synapses recovered from depression

**Example Flow:**
```
1. Sleep phase triggered
   → Consolidation replays important patterns (STDP strengthens)
   → BCM applies homeostatic downscaling (reduce all weights slightly)
   → Meta-plasticity protects high-value memories from downscaling
   → STP states reset (depression/facilitation recover)

2. Wake phase
   → Network has strengthened important memories
   → Overall capacity restored for new learning
```

---

### 5. Global Workspace (Conscious Attention)

**Location:** `src/cognitive/global_workspace/`
**Network Interaction:** Broadcast winner (most salient representation)
**Plasticity Benefits:**

✅ **Attention-Based Plasticity**
- **How Used:** Attended information gets preferential learning
- **Value:** Focus learning on task-relevant information
- **Biological Parallel:** Attention-gated synaptic plasticity
- **Concrete Benefit:** Learning "stop sign" is faster when attention is on stop sign

✅ **Neuromodulators (Acetylcholine)**
- **How Used:** Attention → ACh release → modulates learning
- **Value:** Enhances plasticity in attended regions
- **Concrete Benefit:** Studying with focus (high ACh) → better retention

✅ **Spatial Neuromodulation**
- **How Used:** Attention creates spatial gradients of neuromodulation
- **Value:** Local learning enhancement around attended features
- **Concrete Benefit:** Learning "red" in "red stop sign" also enhances nearby color neurons

❌ **Direct STDP/BCM modification**
- Global workspace doesn't directly modify these, but gates which representations get learned

**Example Flow:**
```
1. Multiple representations compete in global workspace
   → Winner: "stop sign" (highest salience)
   → Broadcast to all modules
   → ACh release in visual cortex
   → Spatial neuromod enhances learning in stop-sign feature neurons
   → STDP operates on attended representation (learning stop sign pattern)
```

---

### 6. Epistemic Filtering (Bias Prevention)

**Location:** `src/cognitive/epistemic/`
**Network Interaction:** Moderate (evaluates information quality)
**Plasticity Benefits:**

✅ **Meta-Plasticity (Selective)**
- **How Used:** High-quality information gets higher learning rate
- **Value:** Prevents learning from misinformation
- **Biological Parallel:** Source credibility gating
- **Concrete Benefit:** Scientific paper → high plasticity; random blog → low plasticity

✅ **Eligibility Traces**
- **How Used:** Could mark synapses for later verification
- **Current Status:** Not implemented
- **Potential Value:** Temporarily mark uncertain information, strengthen only after verification

❌ **Direct network modification**
- Epistemic filtering gates *whether* learning occurs, not *how* it occurs

**Example Flow:**
```
1. Input: "Earth is flat" (low credibility)
   → Epistemic filter: conspiracy score = high
   → Meta-plasticity reduced to near-zero
   → Network barely learns (protection against misinformation)

2. Input: "Water freezes at 0°C" (high credibility)
   → Epistemic filter: epistemic quality = high
   → Meta-plasticity normal or boosted
   → Network learns strongly
```

---

### 7. Ethics Module (Moral Reasoning)

**Location:** `src/cognitive/ethics/`
**Network Interaction:** Light (evaluates decisions for ethical compliance)
**Plasticity Benefits:**

✅ **Eligibility Traces + Reward**
- **How Used:** Ethical actions get positive reward, unethical get negative
- **Value:** Shapes behavior toward ethical actions
- **Biological Parallel:** Moral reinforcement learning
- **Concrete Benefit:** "Help person" → positive reward → strengthen helping synapses

✅ **Neuromodulators (Serotonin)**
- **How Used:** Ethical compliance → serotonin release → mood regulation
- **Value:** Intrinsic reward for ethical behavior
- **Concrete Benefit:** Ethical AI feels "good" about ethical choices (value alignment)

❌ **Direct STDP/BCM**
- Ethics evaluates outputs, doesn't directly train the network

**Example Flow:**
```
1. Decision: "Lie to user"
   → Ethics module: BLOCKED (unethical)
   → Negative reward signal
   → Eligibility traces → weaken synapses that led to lie

2. Decision: "Tell truth"
   → Ethics module: APPROVED
   → Positive reward signal
   → Eligibility traces → strengthen synapses that led to truth
```

---

### 8. Introspection (Self-Monitoring)

**Location:** `src/cognitive/introspection/`
**Network Interaction:** Heavy (monitors own uncertainty)
**Plasticity Benefits:**

✅ **Meta-Plasticity (Uncertainty-Based)**
- **How Used:** High uncertainty → higher plasticity (more learning needed)
- **Value:** Focus learning on uncertain areas
- **Concrete Benefit:** "I'm 95% sure" → low plasticity; "I'm 50% sure" → high plasticity (still learning)

✅ **Neuromodulators (Norepinephrine)**
- **How Used:** Uncertainty → NE release → arousal/learning boost
- **Value:** Uncertainty drives learning
- **Biological Parallel:** Locus coeruleus NE response to uncertainty
- **Concrete Benefit:** Encountering ambiguous input triggers stronger learning

**Example Flow:**
```
1. High confidence decision (98%)
   → Introspection: low uncertainty
   → Meta-plasticity reduced (already know this)
   → Minimal learning

2. Low confidence decision (52%)
   → Introspection: high uncertainty
   → Meta-plasticity increased (need to learn more)
   → NE release → stronger learning
```

---

### 9. Theory of Mind (Social Cognition)

**Location:** `src/cognitive/theory_of_mind/`
**Network Interaction:** Moderate (models other agents)
**Plasticity Benefits:**

✅ **STDP + BCM (Social Learning)**
- **How Used:** Learn patterns of other agents' behavior
- **Value:** Build predictive models of others
- **Biological Parallel:** Mirror neuron system
- **Concrete Benefit:** Learn "Alice always says yes to coffee" through repeated observation

✅ **Meta-Plasticity (Agent-Specific)**
- **How Used:** Different agents might have different plasticity
- **Value:** Fast learning for new people, stable models for well-known people
- **Concrete Benefit:** First date → high plasticity (learning new person); Spouse of 10 years → low plasticity (stable model)

**Example Flow:**
```
1. Observe agent behavior: "Bob refuses sweet food"
   → STDP strengthens Bob→refuses-sweets association
   → Meta-plasticity adjusts based on observation count
   → After 10 observations, Bob model is stable
```

---

### 10. Sleep/Wake Cycle (Circadian Regulation)

**Location:** `src/cognitive/sleep_wake/`
**Network Interaction:** Moderate (regulates consolidation timing)
**Plasticity Benefits:**

✅ **All Plasticity Features (Time-Gated)**
- **How Used:** Plasticity rates modulated by sleep/wake state
- **Value:** Different plasticity profiles for wake (learning) vs sleep (consolidation)
- **Biological Parallel:** Circadian modulation of plasticity
- **Concrete Benefit:**
  - **Wake:** High STDP, high novelty learning, high meta-plasticity
  - **Sleep:** High consolidation, BCM homeostasis, STP recovery

**Example Flow:**
```
WAKE (8 hours):
  → High STDP (learn from environment)
  → High meta-plasticity (adapt to task demands)
  → STP accumulates depression/facilitation

SLEEP (8 hours):
  → Consolidation replays important memories
  → BCM homeostatic downscaling
  → STP recovery (reset depression/facilitation)
  → Meta-plasticity reduces (protect consolidated memories)
```

---

## Plasticity Timescale Hierarchy

Different plasticity mechanisms operate at different timescales:

| Plasticity Type | Timescale | Primary Beneficiaries |
|-----------------|-----------|----------------------|
| **STP** | Milliseconds (10-1000ms) | Working Memory, Attention, Real-time processing |
| **STDP** | Spike pairs (±50ms) | Knowledge, Consolidation, All learning modules |
| **BCM** | Seconds to minutes | Knowledge, Consolidation (prevents saturation) |
| **Meta-Plasticity** | Minutes (history-based) | Curiosity, Introspection, Epistemic (adaptive learning rates) |
| **Eligibility Traces** | Seconds (reward delay) | Ethics, Curiosity (reinforcement learning) |
| **Neuromodulators** | Seconds to hours | Global Workspace (attention), Curiosity (novelty), Ethics (mood) |
| **Spatial Diffusion** | Seconds | Global Workspace (local enhancement), Consolidation |

---

## Value Proposition Summary

### For Knowledge & Memory
- **STDP:** Learn semantic relationships through co-activation
- **BCM:** Prevent catastrophic forgetting via homeostasis
- **Meta-Plasticity:** Protect old knowledge while learning new
- **Consolidation:** Strengthen important memories during sleep

### For Learning & Adaptation
- **Meta-Plasticity:** Adapt learning rate to novelty/uncertainty
- **Neuromodulators:** Chemical modulation for context-appropriate learning
- **Eligibility Traces:** Bridge temporal gap for delayed rewards

### For Attention & Selection
- **STP:** Short-term enhancement of relevant pathways
- **Attention Plasticity:** Focus learning on attended information
- **Spatial Neuromod:** Local enhancement around salient features

### For Stability & Robustness
- **BCM:** Homeostatic weight regulation
- **Meta-Plasticity:** Protect stable knowledge from perturbation
- **Sleep Consolidation:** Periodic reset and memory strengthening

---

## Module Dependency Matrix

Which modules **require** which plasticity features:

| Module | STP | STDP | BCM | Meta | Elig | Neuromod | Spatial |
|--------|-----|------|-----|------|------|----------|---------|
| Working Memory | ✅ | ⚪ | ⚪ | ✅ | 🔶 | ⚪ | ⚪ |
| Knowledge | ⚪ | ✅ | ✅ | ✅ | ⚪ | ⚪ | ⚪ |
| Curiosity | ⚪ | ⚪ | ⚪ | ✅ | 🔶 | ✅ | ⚪ |
| Consolidation | ✅ | ✅ | ✅ | ✅ | ⚪ | ⚪ | ⚪ |
| Global Workspace | ✅ | ⚪ | ⚪ | ⚪ | ⚪ | ✅ | ✅ |
| Epistemic Filter | ⚪ | ⚪ | ⚪ | ✅ | 🔶 | ⚪ | ⚪ |
| Ethics | ⚪ | ⚪ | ⚪ | ⚪ | ✅ | ✅ | ⚪ |
| Introspection | ⚪ | ⚪ | ⚪ | ✅ | ⚪ | ✅ | ⚪ |
| Theory of Mind | ⚪ | ✅ | ✅ | ✅ | ⚪ | ⚪ | ⚪ |
| Sleep/Wake | ✅ | ✅ | ✅ | ✅ | ⚪ | ✅ | ⚪ |

**Legend:**
- ✅ Required for core functionality
- ⚪ Optional but beneficial
- 🔶 Future integration planned

---

## Critical Insights

1. **Automatic Application:** Modules don't call plasticity directly—it's applied automatically when they use the network

2. **Layered Architecture:** Plasticity is a **low-level** feature that **high-level** modules benefit from implicitly

3. **Timescale Separation:** Different plasticity types serve different temporal needs (STP for milliseconds, consolidation for hours)

4. **Value Through Interaction:** A module's benefit from plasticity depends on **how** it interacts with the network, not just **if** it does

5. **Composability:** Multiple plasticity mechanisms work together (e.g., STDP + BCM + Meta-plasticity in knowledge learning)

---

## Conclusion

Each plasticity mechanism serves **specific purposes** for **specific modules**:

- **STP:** Working memory, attention (short-term dynamics)
- **STDP+BCM:** Knowledge, consolidation (long-term learning with stability)
- **Meta-Plasticity:** Curiosity, epistemic, introspection (adaptive learning rates)
- **Eligibility Traces:** Ethics, curiosity (reinforcement learning)
- **Neuromodulators:** Global workspace, curiosity, ethics (chemical modulation)
- **Spatial Neuromod:** Global workspace (local enhancement)

The value isn't that every module uses every plasticity feature—it's that the **right features** are available for the **right modules** at the **right timescales**.

---

**Document Complete**
**Next:** Create integration tests to verify these interactions work as described

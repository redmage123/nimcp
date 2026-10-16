# Parietal Linguistics Module Implementation Plan

## Executive Summary

Implement three neurobiologically-grounded linguistics sub-modules in the parietal lobe region, leveraging NIMCP's existing fuzzy logic, statistics, SNN/STDP/plasticity, engram memory, and hypothalamus omni-directional model infrastructure.

**Modules:**
1. **Spatial Language** (Angular Gyrus, BA39) - Spatial prepositions, metaphors, reference frames
2. **Numerical Language** (Intraparietal Sulcus) - Number word parsing, ordinals, quantifiers
3. **Phonological Working Memory** (Supramarginal Gyrus, BA40) - Phonological loop, rehearsal, similarity

---

## 1. Motivation & Biological Basis

### Why Parietal Linguistics?

The parietal lobe has specialized language processing beyond general-purpose language areas:

| Region | Function | Evidence |
|--------|----------|----------|
| Angular Gyrus (BA39) | Spatial-semantic mapping | Lesions cause spatial language deficits |
| Supramarginal Gyrus (BA40) | Phonological working memory | Subvocal rehearsal, articulatory loop |
| Intraparietal Sulcus (IPS) | Numerical cognition | Number word ↔ magnitude mapping |

### Integration Philosophy

Rather than duplicating Broca/Wernicke's functionality, these modules provide:
- **Spatial grounding** for abstract language (embodied cognition)
- **Numerical precision** for quantitative language
- **Phonological buffer** for linguistic working memory

---

## 1.5 Mesh Architecture Design

### Why Mesh Instead of Hub-and-Spoke?

**Current Hub-and-Spoke Pattern:**
```
Spatial Language → Fuzzy (result)
Spatial Language → SNN (result)
Spatial Language → ToM (result)
Spatial Language aggregates results
```

**Problem:** The linguistics module acts as a central coordinator, calling each dependency sequentially and aggregating results. This doesn't reflect how the brain actually works - brain regions collaborate in parallel with massive recurrent connectivity.

**Mesh Pattern:**
```
Request → Broadcast to Mesh

    Fuzzy ◄──► SNN ◄──► ToM ◄──► World Model
      ↕          ↕        ↕          ↕
    Engram ◄──► BG ◄──► Perception ◄──► Cerebellum
      ↕          ↕        ↕          ↕
    Symbolic ◄─► Plasticity ◄──► Hypothalamus

Mesh converges → Response
```

**Advantage:** All integrated modules work together, communicating peer-to-peer, to produce a collective response. More biologically plausible and potentially more robust.

### Existing NIMCP Infrastructure for Mesh

NIMCP already has comprehensive infrastructure to support mesh architecture:

| Component | File | Mesh Capability |
|-----------|------|-----------------|
| **Bio-Async Broadcast** | `nimcp_bio_router.h` | `bio_router_broadcast()` - one-to-all messaging |
| **Gossip Beliefs** | `nimcp_gossip_beliefs.h` | Probabilistic peer-to-peer belief propagation |
| **FEP Orchestrator** | `nimcp_fep_orchestrator.h` | Free energy minimization for convergence |
| **Precision Weighting** | `nimcp_fep_neuromod.h` | Trust = inverse prediction error |
| **CRDT Workspace** | `nimcp_collective_workspace.h` | Distributed shared state with vector clocks |
| **Consensus Voting** | `nimcp_swarm_consensus.h` | Byzantine fault-tolerant (1/3 faulty) |
| **Glial Waves** | `nimcp_bio_async.h` | Topology-aware spatial broadcast |
| **Predictive Coding** | `nimcp_bio_async.h` | Error-driven selective communication |
| **KG Discovery** | `nimcp_brain_kg_helpers.h` | Topology-aware module discovery |
| **Convergence Detection** | `nimcp_inner_dialogue_convergence.h` | Agreement, deadlock, rumination detection |

### Mesh Convergence via Free Energy Principle

The FEP provides a mathematically grounded framework for distributed consensus:

```
F = KL[q(s)||p(s)] + E_q[-ln p(o|s)]
    └─Complexity─┘   └──Inaccuracy──┘

Where:
- q(s) = Local module's beliefs about shared linguistic state
- p(o|s) = Generative model (how observations arise from true state)
- o = Messages/beliefs from neighboring modules
- F = Minimized when all modules agree
```

**Convergence Mechanism:**
1. Request broadcast to all linguistics mesh participants
2. Each module produces local belief + precision (confidence)
3. Modules gossip beliefs to neighbors via `gossip_propagate_round()`
4. Each module updates belief via gradient descent on F: `μ' = μ - lr * Π * ε`
5. Precision weighting: high precision = high trust = more influence
6. Convergence detected when `agreement_score > 0.75`
7. Collective belief returned (precision-weighted average)

**Key Parameters:**
- `FEP_DEFAULT_BELIEF_LR = 0.1` - Belief update step size
- `FEP_CONVERGENCE_THRESHOLD = 0.001` - Halt when F stabilizes
- `SWARM_BFT_THRESHOLD = 0.333` - Byzantine fault tolerance
- `AGREEMENT_THRESHOLD = 0.75` - Convergence threshold

### Linguistics Mesh Topology

```
┌─────────────────────────────────────────────────────────────────────┐
│                    LINGUISTICS MESH NETWORK                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                  GOSSIP BELIEF LAYER                         │   │
│   │                                                              │   │
│   │  ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐       │   │
│   │  │ Fuzzy  │◄──►│  SNN   │◄──►│  ToM   │◄──►│World M │       │   │
│   │  │Π=0.8  │    │Π=0.9  │    │Π=0.7  │    │Π=0.85 │       │   │
│   │  └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘       │   │
│   │       │             │             │             │            │   │
│   │       ▼             ▼             ▼             ▼            │   │
│   │  ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐       │   │
│   │  │ Engram │◄──►│  BG    │◄──►│Percept │◄──►│Cerebell│       │   │
│   │  │Π=0.75 │    │Π=0.82 │    │Π=0.88 │    │Π=0.7  │       │   │
│   │  └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘       │   │
│   │       │             │             │             │            │   │
│   │       ▼             ▼             ▼             ▼            │   │
│   │  ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐       │   │
│   │  │Symbolic│◄──►│Plastic │◄──►│ Hypo   │◄──►│Medulla │       │   │
│   │  │Π=0.95 │    │Π=0.6  │    │Π=0.65 │    │Π=0.5  │       │   │
│   │  └────────┘    └────────┘    └────────┘    └────────┘       │   │
│   │                                                              │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │              COLLECTIVE WORKSPACE (CRDT)                     │   │
│   │  - Top-K linguistic interpretations (salience-ranked)        │   │
│   │  - Vector clocks for causality                               │   │
│   │  - Conflict resolution via salience (= precision × confidence)│   │
│   └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │              CONVERGENCE DETECTOR                            │   │
│   │  - agreement_score: precision-weighted consensus [0-1]       │   │
│   │  - deadlock_score: oscillating disagreement detection        │   │
│   │  - rumination_score: repetitive pattern detection            │   │
│   │  - perspective_entropy: diversity metric                     │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Mesh Message Flow

**Step 1: Request Broadcast**
```c
// Linguistics module initiates mesh request
bio_router_broadcast(ctx, &linguistic_request, sizeof(linguistic_request));
```

**Step 2: Local Processing + Belief Generation**
```c
// Each module processes and produces belief with precision
typedef struct {
    uint32_t belief_id;
    char topic[128];              // "spatial_preposition_left"
    float certainty;              // Local confidence [0-1]
    float* belief_vector;         // Neural encoding of interpretation
    float precision;              // Π = 1 / prediction_error_variance
} linguistics_belief_t;
```

**Step 3: Gossip Propagation**
```c
// Introduce local belief to gossip network
gossip_introduce_belief(gb, module_id, &belief);

// Propagate beliefs (probabilistic peer-to-peer)
gossip_propagate_round(gb, current_time_ms);
```

**Step 4: FEP Belief Update**
```c
// Each module updates belief based on neighbors
for each neighbor_belief in received_beliefs:
    // Compute prediction error
    ε = neighbor_belief.vector - local_belief.vector

    // Precision-weighted update
    Π = neighbor_belief.precision * credibility_weight
    Δμ = learning_rate * Π * ε

    // Update local belief
    local_belief.vector += Δμ
```

**Step 5: Convergence Check**
```c
// Check if mesh has converged
convergence_analysis_t analysis;
inner_dialogue_convergence_analyze(convergence_ctx, &analysis);

if (analysis.agreement_score >= AGREEMENT_THRESHOLD) {
    // Converged - return collective belief
    gossip_get_consensus_beliefs(gb, &consensus, &count);
    return consensus;
} else if (analysis.deadlocked) {
    // Deadlock - use voting fallback
    swarm_consensus_propose(ctx, VOTE_TOPIC_INTERPRETATION, ...);
}
```

### Neuromodulator Channels for Priority

Different message types route through different neuromodulator channels:

| Channel | Decay | Use Case |
|---------|-------|----------|
| **Dopamine** | ~2s | Reward signals (correct interpretation confirmed) |
| **Acetylcholine** | ~50ms | Fast attention (focus on specific word) |
| **Norepinephrine** | ~3s | Alerts/priority (ambiguity detected, need resolution) |
| **Serotonin** | ~10s | Slow state changes (discourse context shift) |

### Mesh Coordinator Bridge

**File:** `nimcp_parietal_linguistics_mesh_coordinator.h`

```c
typedef struct {
    // Gossip network for belief propagation
    gossip_beliefs_t* gossip;

    // FEP for convergence
    fep_orchestrator_t* fep_orch;

    // Collective workspace for top-K interpretations
    collective_workspace_t* workspace;

    // Convergence detection
    convergence_ctx_t* convergence;

    // Consensus voting (fallback)
    swarm_consensus_ctx_t* voting;

    // Bio-async for messaging
    bio_module_context_t bio_ctx;

    // KG for module discovery
    kg_module_context_t kg_ctx;

    // Registered mesh participants
    linguistics_mesh_participant_t participants[MAX_MESH_PARTICIPANTS];
    uint32_t participant_count;

    // Configuration
    linguistics_mesh_config_t config;
} linguistics_mesh_coordinator_t;

// Configuration
typedef struct {
    float agreement_threshold;      // Default: 0.75
    float gossip_probability;       // Default: 0.3
    uint32_t max_iterations;        // Default: 100
    uint32_t convergence_window;    // Default: 10 iterations
    float precision_floor;          // Default: 0.01
    float precision_ceiling;        // Default: 100.0
    bool enable_voting_fallback;    // Default: true
    nimcp_bio_channel_type_t default_channel; // Default: ACETYLCHOLINE
} linguistics_mesh_config_t;

// Core API
linguistics_mesh_coordinator_t* linguistics_mesh_create(
    const linguistics_mesh_config_t* config);

int linguistics_mesh_register_participant(
    linguistics_mesh_coordinator_t* mesh,
    bio_module_id_t module_id,
    const char* module_name,
    linguistics_mesh_handler_t handler);

int linguistics_mesh_request(
    linguistics_mesh_coordinator_t* mesh,
    const linguistics_request_t* request,
    linguistics_response_t* response,
    uint32_t timeout_ms);

int linguistics_mesh_get_convergence_stats(
    const linguistics_mesh_coordinator_t* mesh,
    linguistics_mesh_stats_t* stats);
```

### Participant Interface

Each integrated module implements:

```c
typedef struct {
    // Process request and produce local belief
    int (*process)(void* ctx,
                   const linguistics_request_t* request,
                   linguistics_belief_t* belief);

    // Update belief based on neighbor beliefs (FEP update)
    int (*update)(void* ctx,
                  const linguistics_belief_t* neighbor_beliefs,
                  uint32_t neighbor_count,
                  linguistics_belief_t* updated_belief);

    // Get current precision (inverse prediction error variance)
    float (*get_precision)(void* ctx);

    // Module context
    void* ctx;
} linguistics_mesh_handler_t;
```

### Example: Spatial Language Mesh Request

```c
// 1. Create request
linguistics_request_t request = {
    .type = LING_REQUEST_PARSE_SPATIAL,
    .input = "left of the table",
    .context = discourse_context
};

// 2. Submit to mesh
linguistics_response_t response;
linguistics_mesh_request(mesh, &request, &response, 5000);

// 3. Mesh internally:
//    - Broadcasts to all participants
//    - Fuzzy: produces MF for "left" (Π=0.8)
//    - SNN: produces spike encoding (Π=0.9)
//    - ToM: produces perspective-adjusted frame (Π=0.7)
//    - World Model: produces spatial prediction (Π=0.85)
//    - Engram: retrieves similar patterns (Π=0.75)
//    - etc.
//
//    - All gossip beliefs to neighbors
//    - FEP updates until agreement_score > 0.75
//    - Collective belief returned

// 4. Response contains precision-weighted consensus
printf("Frame: %s, Confidence: %.2f\n",
       response.spatial.frame_name,
       response.confidence);
```

---

## 2. Existing Infrastructure to Leverage

### 2.1 Parietal Lobe Core (Already Implemented)

| File | Contents | Relevance |
|------|----------|-----------|
| `include/cognitive/parietal/nimcp_parietal.h` | Orchestrator with 50+ request types | Base integration point |
| `include/cognitive/parietal/nimcp_number_sense.h` | Weber-Fechner, subitizing | Numerical language foundation |
| `include/cognitive/parietal/nimcp_spatial_reasoning.h` | Mental rotation, coordinate transforms | Spatial language foundation |
| `include/language/bridges/nimcp_language_parietal_bridge.h` | Minimal existing bridge | Extend this |
| `src/language/bridges/nimcp_language_parietal_bridge.c` | 20 spatial words, number words | Enhance significantly |

### 2.2 Fuzzy Logic System

| File | Key Features | Linguistics Application |
|------|--------------|------------------------|
| `include/utils/fuzzy/nimcp_fuzzy_types.h` | 14 MF types, 8 hedges | Spatial preposition semantics |
| `include/utils/fuzzy/nimcp_fuzzy_inference.h` | Mamdani/Sugeno/Tsukamoto | Quantifier interpretation |
| Hedges: VERY, SOMEWHAT, EXTREMELY, SLIGHTLY | Linguistic modifiers | "very near", "somewhat left" |

**Application Example:**
- "near" → Gaussian MF centered at 0, σ=2m
- "very near" → μ²(x) (FUZZY_HEDGE_VERY concentration)
- "somewhat near" → √μ(x) (FUZZY_HEDGE_SOMEWHAT dilation)

### 2.3 Statistics & Probability

| File | Key Features | Linguistics Application |
|------|--------------|------------------------|
| `include/utils/statistics/nimcp_statistics.h` | Bayesian inference, 20+ distributions | Reference frame selection |
| `include/utils/statistics/nimcp_information_theory.h` | Entropy, MI, KL divergence | Phonological similarity |
| `include/utils/statistics/nimcp_ml_statistics.h` | HMM, GMM, Gaussian Processes | Number word sequences |

**Application Example:**
- HMM for phoneme sequence modeling: P(phoneme_t | phoneme_t-1)
- Bayesian inference for ambiguous spatial reference: P(frame | word, context)

### 2.4 SNN Architecture

| File | Key Features | Linguistics Application |
|------|--------------|------------------------|
| `include/snn/nimcp_snn.h` | Master SNN facade | Phonological spike representations |
| `include/snn/nimcp_snn_types.h` | Population-based organization | Linguistic feature populations |
| `include/snn/nimcp_snn_network.h` | Network orchestration API | Phoneme/word networks |
| `include/snn/nimcp_snn_encoding.h` | Rate/temporal/population/latency/burst/phase encoding | Word→spike encoding |
| `include/snn/nimcp_snn_training.h` | STDP, R-STDP, eProp, surrogate gradient | Linguistic learning |

**SNN Topology Types (from `snn_topology_t`):**
- `SNN_TOPO_FEEDFORWARD` - Phoneme sequence processing
- `SNN_TOPO_RECURRENT` - Phonological loop dynamics
- `SNN_TOPO_SMALL_WORLD` - Semantic network structure
- `SNN_TOPO_COLUMN` - Cortical column for linguistic processing

**Spike Encoding for Linguistics:**
- **Rate coding**: Word frequency → firing rate
- **Temporal coding**: Phoneme timing → spike timing
- **Population coding**: Semantic features → population pattern
- **Phase coding**: Theta phase → encoding vs retrieval

### 2.5 STDP Variants (Detailed)

#### 2.5.1 Pairwise STDP (`include/plasticity/stdp/nimcp_stdp.h`)

**Learning Rule** (Bi & Poo, 1998):
```
Pre before Post → LTP: Δw = a_plus × exp(-Δt/τ_plus)
Post before Pre → LTD: Δw = -a_minus × exp(Δt/τ_minus)
```

**Default Parameters:**
- `a_plus`: 0.005 (LTP amplitude)
- `a_minus`: 0.00525 (LTD amplitude, slight bias toward depression)
- `τ_plus`, `τ_minus`: 20ms timing windows
- `da_modulation_gain`: 100.0 (dopamine scaling)
- `burst_amplification`: 3.0x during phasic dopamine

**Linguistics Application:**
- Word-meaning associations via timing correlation
- Spatial preposition → reference frame binding
- Number word → magnitude mapping

#### 2.5.2 Triplet STDP (`include/plasticity/stdp/nimcp_triplet_stdp.h`)

**Pfister & Gerstner 2006 - Frequency-Dependent Plasticity:**

Four spike traces (vs. 2 in pairwise):
```c
r1_pre   // Fast pre-synaptic (τ = 16.8ms)
o1_post  // Fast post-synaptic (τ = 33.7ms)
r2_pre   // Slow pre-synaptic (τ = 101ms) - TRIPLET
o2_post  // Slow post-synaptic (τ = 125ms) - TRIPLET
```

**Weight Update:**
```
POST SPIKE: Δw = A2_plus × r1 + A3_plus × r2 × o1
                 └─pairwise─┘   └───triplet───┘

PRE SPIKE:  Δw = -A2_minus × o1 - A3_minus × r1 × o2
```

**Default Parameters (Visual Cortex):**
- `A2_plus`: 0.005, `A3_plus`: 0.0062
- `A2_minus`: 0.007, `A3_minus`: 0.00023

**Linguistics Application:**
- **Low frequency (<10 Hz)**: Pairwise dominates → slow vocabulary learning
- **High frequency (>40 Hz)**: Triplet amplifies → burst-mediated consolidation
- Number word sequences: "twenty" → "one" at high gamma = strong LTP

#### 2.5.3 Reward-Modulated STDP (R-STDP)

**Three-Factor Rule:**
```
Δw = η × eligibility(t) × dopamine × STDP_window
```

**Four-Factor Rule (Burst-Triggered):**
```
Δw = η × eligibility(t) × reward × dopamine × burst_gate
```

Only consolidates during dopamine bursts (phasic > 6x baseline).

**Linguistics Application:**
- Correct word usage → reward → strengthened association
- Social feedback drives vocabulary acquisition
- Curiosity-driven exploration of novel words

### 2.6 Homeostatic Plasticity (`include/plasticity/homeostatic/nimcp_homeostatic.h`)

#### A. Synaptic Scaling (Turrigiano et al. 1998)
```
w_scaled = w × (target_rate / actual_rate)^α
```
- Maintains stable firing rates across vocabulary
- Prevents runaway excitation from frequent words
- α = 0.5 (sublinear), 1.0 (linear), 2.0 (supralinear)

#### B. Intrinsic Plasticity (Desai et al. 1999)
```
dθ/dt = (actual_rate - target_rate) / τ_ip
```
- Adapts neuronal excitability
- Balances rare vs frequent word representations

#### C. Metaplasticity / BCM Threshold Sliding (Abraham & Bear 1996)
```
θ_m = <r²>  (sliding threshold based on squared activity)

BCM Rule: Δw = η × post × (post - θ) × pre
- post > θ: LTP
- post < θ: LTD
```

**Linguistics Application:**
- BCM for competitive vocabulary (similar words compete)
- Winner-take-all for word selection
- Critical periods for L2 acquisition

### 2.7 Structural Plasticity (`include/plasticity/structural/nimcp_structural_plasticity.h`)

**Spine Lifecycle (5 States):**

| State | Duration | Stability | AMPAR | Trigger |
|-------|----------|-----------|-------|---------|
| **NASCENT** | min-hours | 0.2 | 5 | Activity > 20-50 Hz |
| **STABLE** | 1-7 days | 0.8 | 30 | Repeated activation |
| **POTENTIATED** | long-term | 0.95 | 80 | Strong LTP / sleep |
| **PRUNING** | hours | declining | - | Activity < 0.5-2 Hz |
| **ELIMINATED** | - | 0 | 0 | Complement tagging |

**Spine Volume/PSD Ranges:**
- Nascent: vol 0.1-0.3, PSD 0.2-0.4
- Stable: vol 0.5-1.0, PSD 0.6-1.0
- Potentiated: vol 1.0-1.5, PSD 1.0-1.8

**Linguistics Application:**
- **NASCENT**: New word exposure creates thin spines
- **STABLE**: Repeated use consolidates vocabulary
- **POTENTIATED**: High-frequency words get enlarged spines
- **PRUNING**: Disused words fade (L1 attrition in L2 immersion)
- **ELIMINATED**: Complete forgetting

**Sleep Integration:**
- NREM: Strengthens tagged spines (consolidation)
- REM: Prunes weak spines (forgetting)
- Awake: Formation of new connections

### 2.8 Training Layer Architecture

#### Core Training Files
| File | Purpose |
|------|---------|
| `include/middleware/training/nimcp_training_module.h` | Core training infrastructure |
| `include/middleware/training/nimcp_brain_training_integration.h` | Unified training coordinator (TM-3) |
| `include/middleware/training/nimcp_training_plasticity_bridge.h` | Loss → RPE → Dopamine (TPB-1) |
| `include/middleware/training/nimcp_training_callbacks.h` | Event-driven training control (TCB-1) |
| `include/training/nimcp_meta_learning.h` | MAML, Reptile, Prototypical Networks |
| `include/training/nimcp_curriculum_learning.h` | SPL, teacher-guided, uncertainty-based |

#### Training Phases (T1-T4)

```c
typedef enum {
    NIMCP_TRAIN_PHASE_T1,  // Homeostatic learning - baseline stabilization
    NIMCP_TRAIN_PHASE_T2,  // Dendritic learning - branch-specific BCM
    NIMCP_TRAIN_PHASE_T3,  // Predictive coding - error-driven updates
    NIMCP_TRAIN_PHASE_T4,  // Meta-learning - learning-to-learn
} nimcp_training_phase_t;
```

**Linguistics Application:**
- **T1**: Stabilize phonological representations
- **T2**: Branch-specific word-feature bindings
- **T3**: Predictive language processing (next-word prediction)
- **T4**: Rapid adaptation to new vocabulary domains

#### Meta-Learning (MAML & Variants)

**Supported Algorithms:**
```c
META_ALG_MAML,           // Full second-order MAML
META_ALG_FOMAML,         // First-order approximation
META_ALG_REPTILE,        // Batch averaging
META_ALG_METASGD,        // Per-parameter learning rates
META_ALG_PROTOTYPICAL,   // Metric-based classification
```

**Key Parameters:**
- Inner LR: 0.01, Outer LR: 0.001
- Inner steps: 5 (task adaptation)
- Task sampling: uniform, curriculum, importance

**Linguistics Application:**
- Rapid word learning from few examples (few-shot)
- Cross-domain vocabulary transfer
- L2 acquisition from L1 knowledge

#### Curriculum Learning

**Strategies:**
```c
CURRICULUM_STRATEGY_SELF_PACED,      // Model selects learnable samples
CURRICULUM_STRATEGY_TEACHER_GUIDED,  // Fixed difficulty schedule
CURRICULUM_STRATEGY_UNCERTAINTY,     // Focus on uncertain examples
CURRICULUM_STRATEGY_LOSS_BASED,      // Use training loss as difficulty
CURRICULUM_STRATEGY_ANTI_CURRICULUM, // Hard examples first (robustness)
```

**Linguistics Application:**
- Start with high-frequency words, progress to rare
- Core vocabulary before specialized terms
- Simple prepositions before complex spatial relations
- Single-digit numbers before multi-digit

#### Training-Plasticity Bridge (TPB-1): Loss → RPE → Dopamine

**RPE Computation:**
```
RPE = loss_delta / baseline_variance

Modes:
- TPB_RPE_TEMPORAL_DIFF:    TD-style from baseline
- TPB_RPE_EXPONENTIAL_AVG:  EMA baseline with decay
- TPB_RPE_SLIDING_WINDOW:   Window average baseline
- TPB_RPE_ADAPTIVE:         Variance-tracked baseline
```

**Region-Specific Plasticity Routing:**
```c
TPB_REGION_CORTICAL,     // STDP + ACh attention
TPB_REGION_STRIATAL,     // Strong DA, reinforcement
TPB_REGION_HIPPOCAMPAL,  // BCM, pattern separation
TPB_REGION_CEREBELLAR,   // Error-driven supervised
TPB_REGION_PREFRONTAL,   // Working memory, executive
```

**Neuromodulator-LR Modulation:**
```
lr_mult = w_da × DA + w_ach × ACh + w_5ht × (1-5HT) + w_ne × NE
modulated_lr = base_lr × clamp(lr_mult, min, max)
```

**Linguistics Application:**
- Loss decrease → positive RPE → dopamine → LTP
- Loss increase → negative RPE → reduced learning
- ACh (attention) gates which words get learned
- 5-HT modulates patience for difficult vocabulary

#### Training Callbacks (TCB-1)

**Event Types:**
```c
TCB_EVENT_LOSS_COMPUTED,     // After loss calculation
TCB_EVENT_WEIGHTS_UPDATED,   // After weight modification
TCB_EVENT_CONVERGENCE,       // Early stopping triggered
TCB_EVENT_DIVERGENCE,        // Training instability
TCB_EVENT_GRADIENT_CLIPPED,  // Gradients clipped
```

**Callback Actions:**
```c
TCB_ACTION_CONTINUE,         // Continue training
TCB_ACTION_STOP_TRAINING,    // Stop training loop
TCB_ACTION_REDUCE_LR,        // Reduce learning rate
TCB_ACTION_ROLLBACK,         // Rollback to checkpoint
```

**Linguistics Application:**
- Detect vocabulary learning plateau → reduce LR
- Detect catastrophic forgetting → rollback
- Monitor gradient health for language model stability

### 2.9 Engram Memory System

| File | Key Features | Linguistics Application |
|------|--------------|------------------------|
| `include/cognitive/memory/nimcp_engram.h` | Engram encoding, states, tagging | Word→meaning traces |
| `include/cognitive/memory/nimcp_systems_consolidation.h` | Hippocampus→Cortex transfer | Vocabulary long-term storage |
| Consolidation states | LABILE → CONSOLIDATED | New word stabilization |
| Emotional tagging | Arousal, valence | Important word retention |

**Engram States for Vocabulary:**
```
ENCODING → LABILE → CONSOLIDATING → CONSOLIDATED → (RECONSOLIDATING)
   ↓          ↓                          ↓
 New word   Vulnerable      Stable long-term vocabulary
            to interference
```

**Time Constants:**
- Synaptic consolidation: 6 hours (protein synthesis)
- Systems consolidation: 30 days (hippocampus → cortex)
- IEG tagging window: 4-6 hours
- Reconsolidation window: 6 hours after recall

**Linguistics Application:**
- Encode "left" → allocentric reference frame as SEMANTIC engram
- Sleep consolidation: replay number word patterns at 15x speed
- Reconsolidation window: update pronunciation after recall
- Emotional tagging: important/salient words resist forgetting

### 2.10 Hypothalamus Omni-Directional Model

| File | Key Features | Linguistics Application |
|------|--------------|------------------------|
| `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h` | 22+ bridges, drive coordination | Learning motivation |
| `include/cognitive/omni/nimcp_omni_world_model.h` | RSSM, forward/backward inference | Predict spatial outcomes |
| `include/cognitive/omni/nimcp_omni_precision.h` | Precision weighting | Ambiguity resolution |
| `include/cognitive/omni/nimcp_omni_active_inference.h` | Policy selection | Language production |

**Application Example:**
- CURIOSITY drive → boost plasticity for novel vocabulary
- World Model: "go left" → predict new spatial state
- Precision weighting: disambiguate "left" (allocentric vs egocentric)

### 2.12 Math Utilities

#### Complex Math & Phasors (`nimcp_complex_math.h`)
| Function | Linguistics Application |
|----------|------------------------|
| `phasor_from_polar()` | Phase coding for phonological representations |
| `phasor_phase_difference()` | Phoneme timing synchrony |
| `phasor_array_coherence()` | Inter-trial phase coherence for word recognition |
| `phasor_pac_modulation_index()` | Theta-gamma coupling for phonological loop |

#### Spectral Analysis (`nimcp_fft.h`, `nimcp_hilbert.h`)
| Function | Linguistics Application |
|----------|------------------------|
| `fft_execute_real()` | Decompose speech into frequency components |
| `fft_band_power()` | Extract prosodic rhythm features |
| `fft_brain_wave_power()` | Theta/gamma band analysis for encoding/retrieval |
| `hilbert_extract_phase()` | Instantaneous phase for theta-gating |
| `hilbert_extract_amplitude()` | Envelope for syllable detection |

#### Numerical Integration (`nimcp_integration.h`)
| Method | Linguistics Application |
|--------|------------------------|
| Euler | Fast phonological trace decay simulation |
| RK4 | Accurate semantic drift over discourse |
| Adaptive RK45 | Variable timestep for consolidation dynamics |

#### Tensor Networks & Compression
| Module | Linguistics Application |
|--------|------------------------|
| `nimcp_mps.h` (MPS/Tensor Train) | 10-100x compression for word embeddings |
| `nimcp_svd_simple.h` | Dimensionality reduction for semantic space |
| `nimcp_tensor.h` | N-D tensors for multi-modal language features |

#### Hyperbolic Geometry (`nimcp_hyperbolic.h`)
**200x Compression for Hierarchical Semantics:**
```
Poincaré ball B^n = {x ∈ R^n : ||x|| < 1}
d(x,y) = acosh(1 + 2||x-y||²/((1-||x||²)(1-||y||²)))
```
| Function | Linguistics Application |
|----------|------------------------|
| `poincare_distance()` | Semantic similarity in concept hierarchies |
| `poincare_exp_map()` | Gradient descent on word embeddings |
| `poincare_sgd_step()` | Riemannian optimization for vocabulary |

**Value:** Embed 1M words in 5D instead of 1000D. Natural for:
- Hypernym hierarchies (animal → mammal → dog)
- Spatial preposition hierarchies (location → near → adjacent)
- Number word hierarchies (quantity → cardinal → "five")

#### Positional Encoding (`nimcp_positional_encoding.h`)
| Encoding Type | Linguistics Application |
|---------------|------------------------|
| Sinusoidal | Absolute position in utterance |
| RoPE | Relative position for word order |
| ALiBi | Linear attention bias for long sequences |
| Learned | Task-specific positional patterns |

#### Gabor Filters (`nimcp_gabor.h`)
| Feature | Linguistics Application |
|---------|------------------------|
| Orientation θ | Visual word form recognition |
| Wavelength λ | Spatial frequency for letter features |
| V1 simple/complex cells | Grapheme processing |

### 2.13 Quantum Algorithms

#### Quantum Walk (`nimcp_quantum_walk.h`)
**O(√N) Speedup for Information Propagation:**
```
Coin operators: Hadamard, Grover, Fourier
Speedup: O(√N) vs O(N²) classical diffusion
```
| Function | Linguistics Application |
|----------|------------------------|
| `quantum_walk_step()` | Accelerated semantic spreading activation |
| `quantum_walk_get_distribution()` | Probability distribution over word associations |
| `quantum_walk_measure_mc()` | Sample from semantic network |

**Value:**
- Faster spreading activation through semantic network
- Quadratic speedup for word association retrieval
- Natural modeling of graded semantic similarity

#### Quantum Monte Carlo (`nimcp_quantum_monte_carlo.h`)
| Algorithm | Linguistics Application |
|-----------|------------------------|
| Amplitude Estimation | Estimate probability of word sense |
| Finite-Shot Measurement | Sample from vocabulary distribution |
| Adaptive Annealing | Escape local minima in parsing |
| MCTS-Guided Search | Optimal word sequence generation |

#### Quantum Annealing (`nimcp_quantum_annealing.h`)
**Escape Local Minima via Quantum Tunneling:**
```
P_tunnel = Γ × exp(-B/T^α)
Cooling: EXPONENTIAL, LINEAR, LOGARITHMIC, ADAPTIVE
```
| Function | Linguistics Application |
|----------|------------------------|
| Energy landscape optimization | Optimal reference frame selection |
| Quantum tunneling | Escape incorrect parsing interpretation |
| Ternary spins | Memory-efficient word representations |

**Value:**
- Find globally optimal parse (not local maximum)
- Ternary states: {-1=negative, 0=superposition, +1=positive} for sentiment
- 5x memory compression for vocabulary

#### Quantum MCTS (`nimcp_quantum_mcts.h`)
**Hybrid Classical-Quantum Tree Search:**
| Feature | Linguistics Application |
|---------|------------------------|
| Quantum-enhanced rollouts | Better sentence generation sampling |
| Amplitude-based value estimation | More accurate word choice evaluation |
| Quantum exploration bonus | Explore novel word combinations |
| Quantum Boltzmann sampling | Temperature-controlled word selection |

**Value:**
- Better language generation via quantum-enhanced search
- More diverse vocabulary exploration
- Optimal action selection for language production

#### Quantum Reasoning (`nimcp_quantum_reasoning.h`)
**Grover-Inspired Symbolic Logic:**
```
Grover iterations: π/4 × √(N/M)
Speedup: Quadratic for search problems
```
| Function | Linguistics Application |
|----------|------------------------|
| Grover search | Find satisfying word in vocabulary |
| DPLL SAT solver | Resolve semantic constraints |
| Ternary logic (Kleene) | Handle linguistic uncertainty (TRUE/FALSE/UNKNOWN) |
| Quantum interference | Cancel contradictory interpretations |

**Value:**
- Quadratic speedup for vocabulary search
- Handle linguistic ambiguity with ternary logic
- Quantum interference for disambiguation

#### Quantum-Shannon Integration (`nimcp_quantum_shannon.h`)
**2-5x Better Information Utilization:**
```
Propagation efficiency: η = I/H_source (0-1)
Channel capacity: C = B × log₂(1 + SNR)
```
| Metric | Linguistics Application |
|--------|------------------------|
| Shannon entropy H(X) | Measure vocabulary uncertainty |
| Mutual information I(X;Y) | Semantic relatedness between words |
| Transfer entropy | Causal information flow in discourse |
| Bottleneck detection | Identify processing limitations |

### 2.14 Cognitive Layer Integration Points

| System | File | Integration |
|--------|------|-------------|
| Working Memory | `nimcp_working_memory.h` | Phonological buffer |
| Theory of Mind | `nimcp_theory_of_mind.h` | Perspective-taking for reference frames |
| Emotion Tensor | `nimcp_emotion_tensor.h` | Emotional word associations |
| Semantic Integrator | `nimcp_semantic_integrator.h` | Word→concept mapping |
| Theta-Gamma | `nimcp_theta_gamma.h` | Encoding/retrieval gating |

### 2.15 Exception Handling & Immune System Integration

#### Core Exception Infrastructure

| File | Purpose |
|------|---------|
| `include/utils/exception/nimcp_exception.h` | Base exception with 64-byte immune epitopes |
| `include/utils/exception/nimcp_exception_immune.h` | Exception-to-immune bridge |
| `include/utils/exception/nimcp_exception_macros.h` | Key macros for immune throws |

#### Exception-to-Immune Bridge

**Key Macros for All Linguistics Code:**
```c
// Throw exception and notify immune system
NIMCP_THROW_TO_IMMUNE(code, "message");

// Throw with recovery attempt
NIMCP_THROW_IMMUNE_RECOVER(code, "message");

// Conditional throw (most common pattern)
NIMCP_CHECK_THROW_IMMUNE(condition, code, "message");
```

**Epitope Generation (64-byte fingerprint):**
```c
typedef struct {
    uint32_t error_code;
    char function_name[24];      // Where it occurred
    char module_name[16];        // Linguistics module
    uint16_t severity;
    uint8_t category;
    uint8_t reserved[17];
} nimcp_immune_epitope_t;       // Total: 64 bytes
```

**Recovery Actions (mapped from antibody responses):**

| Recovery Action | Description | Linguistics Use Case |
|-----------------|-------------|----------------------|
| `RECOVERY_GC` | Garbage collect/cleanup | Clear corrupt phonological buffer |
| `RECOVERY_ROLLBACK` | Revert to checkpoint | Restore previous word associations |
| `RECOVERY_QUARANTINE` | Isolate component | Isolate corrupt vocabulary section |
| `RECOVERY_RESTART_THREAD` | Restart processing thread | Restart linguistics worker |
| `RECOVERY_ESCALATE` | Escalate to higher level | Major parsing failure |
| `RECOVERY_IGNORE` | Log and continue | Minor phoneme mismatch |

**Severity Levels:**
```c
typedef enum {
    NIMCP_SEVERITY_DEBUG = 0,    // Development only
    NIMCP_SEVERITY_INFO = 1,     // Informational
    NIMCP_SEVERITY_WARNING = 2,  // Potential issue
    NIMCP_SEVERITY_ERROR = 3,    // Recoverable error
    NIMCP_SEVERITY_CRITICAL = 4, // System impact
    NIMCP_SEVERITY_FATAL = 5     // Unrecoverable
} nimcp_severity_t;
```

**Linguistics Exception Patterns:**
```c
// Example: Spatial preposition parsing
int spatial_language_parse_preposition(const char* word, spatial_semantics_t* out) {
    NIMCP_CHECK_THROW_IMMUNE(word != NULL, NIMCP_ERROR_NULL_POINTER,
        "spatial_language_parse_preposition: word is NULL");

    NIMCP_CHECK_THROW_IMMUNE(out != NULL, NIMCP_ERROR_NULL_POINTER,
        "spatial_language_parse_preposition: output is NULL");

    if (!lookup_preposition(word, out)) {
        // Unknown word - throw to immune for learning consideration
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_UNKNOWN_WORD,
            "Unknown spatial preposition - candidate for vocabulary learning");
    }
    return 0;
}
```

### 2.16 Blood-Brain Barrier (BBB) Integration

#### BBB Infrastructure

| File | Purpose |
|------|---------|
| `include/security/nimcp_blood_brain_barrier.h` | Four-layer defense system |

#### Four-Layer Defense Model

| Layer | Function | Linguistics Application |
|-------|----------|------------------------|
| **Input Validation** | Sanitize inputs | Validate word strings (UTF-8, length limits) |
| **Code Signing** | Verify module integrity | Verify vocabulary data integrity |
| **Memory Boundary** | Protect memory regions | Protect engram storage |
| **Access Control** | Permission verification | Control vocabulary modification rights |

**Threat Types:**
```c
typedef enum {
    BBB_THREAT_BUFFER_OVERFLOW,   // String too long
    BBB_THREAT_SQL_INJECTION,     // Not applicable
    BBB_THREAT_INVALID_INPUT,     // Malformed word
    BBB_THREAT_MEMORY_VIOLATION,  // Engram corruption
    BBB_THREAT_UNAUTHORIZED,      // Permission denied
    BBB_THREAT_MALFORMED_DATA     // Corrupt vocabulary entry
} bbb_threat_type_t;
```

**Actions:**
```c
typedef enum {
    BBB_ACTION_ALLOW,      // Permit operation
    BBB_ACTION_BLOCK,      // Deny operation
    BBB_ACTION_QUARANTINE, // Isolate suspicious data
    BBB_ACTION_LOG_ONLY    // Allow but log
} bbb_action_t;
```

**Integration Pattern for Linguistics:**
```c
// Example: Validate word input before processing
bbb_result_t result = bbb_validate_input(
    word, strlen(word),
    BBB_INPUT_TYPE_UTF8_STRING,
    LINGUISTICS_MAX_WORD_LENGTH);

if (result.action == BBB_ACTION_BLOCK) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_BLOCKED,
        "BBB blocked malformed word input");
}
```

### 2.17 Health Monitoring & Resilience

#### Health Infrastructure

| File | Purpose |
|------|---------|
| `include/utils/fault_tolerance/nimcp_health_monitor.h` | Real-time metrics collection |
| `include/utils/fault_tolerance/nimcp_health_agent.h` | Independent watchdog agent |
| `include/utils/fault_tolerance/nimcp_heartbeat.h` | Liveness detection |

#### Health Monitor Metrics

```c
typedef struct {
    float latency_ms;          // Processing latency
    float memory_usage_mb;     // Memory consumption
    uint32_t error_count;      // Recent errors
    float cache_hit_rate;      // Vocabulary cache hits
    float throughput_wps;      // Words per second
} linguistics_health_metrics_t;
```

**Health Status Levels:**

| Status | Score Range | Action |
|--------|-------------|--------|
| `HEALTH_EXCELLENT` | 90-100 | Normal operation |
| `HEALTH_GOOD` | 70-89 | Monitor closely |
| `HEALTH_DEGRADED` | 50-69 | Reduce load, alert |
| `HEALTH_POOR` | 30-49 | Recovery mode |
| `HEALTH_CRITICAL` | 0-29 | Emergency shutdown |

#### Health Agent Configuration

```c
typedef struct {
    uint32_t watchdog_timeout_ms;    // Default: 500ms
    uint32_t heartbeat_interval_ms;  // Default: 100ms
    uint32_t max_missed_heartbeats;  // Default: 5
    bool enable_auto_recovery;       // Default: true
} linguistics_health_config_t;
```

**Recovery Levels:**
```c
typedef enum {
    NIMCP_MODULE_RECOVERY_LIGHT,   // Clear caches, reset counters
    NIMCP_MODULE_RECOVERY_MEDIUM,  // Reload configuration
    NIMCP_MODULE_RECOVERY_HEAVY,   // Restart subsystem
    NIMCP_MODULE_RECOVERY_FULL     // Full module restart
} nimcp_recovery_level_t;
```

#### Heartbeat Integration

```c
// Register linguistics module heartbeat
heartbeat_handle_t hb = heartbeat_register(
    "parietal_linguistics",
    100,  // 100ms interval
    linguistics_heartbeat_callback);

// In processing loop
void linguistics_heartbeat_callback(void* ctx) {
    linguistics_ctx_t* ling = (linguistics_ctx_t*)ctx;

    // Update health metrics
    health_metrics_t metrics = {
        .latency_ms = ling->avg_latency,
        .error_rate = ling->recent_errors / ling->total_ops,
        .memory_mb = ling->memory_usage
    };

    health_monitor_update(ling->health, &metrics);
}
```

### 2.18 Knowledge Graph (KG) Wiring System

#### KG Infrastructure

| File | Purpose |
|------|---------|
| `include/core/brain/nimcp_brain_kg.h` | Brain knowledge graph |

#### Node Types for Linguistics

| Node Type | Description | Linguistics Use |
|-----------|-------------|-----------------|
| `BRAIN_KG_NODE_CORTICAL` | Cortical region | Parietal linguistics region |
| `BRAIN_KG_NODE_COGNITIVE` | Cognitive function | Language processing |
| `BRAIN_KG_NODE_MEMORY` | Memory system | Vocabulary storage |
| `BRAIN_KG_NODE_BRIDGE` | Bridge module | SNN/Plasticity bridges |

#### Edge Types for Linguistics

| Edge Type | Description | Linguistics Use |
|-----------|-------------|-----------------|
| `BRAIN_KG_EDGE_MODULATES` | Modulatory effect | Hypothalamus → plasticity |
| `BRAIN_KG_EDGE_EXCITES` | Excitatory connection | Word → meaning |
| `BRAIN_KG_EDGE_INHIBITS` | Inhibitory connection | Competing word senses |
| `BRAIN_KG_EDGE_PROJECTS_TO` | Anatomical projection | Angular Gyrus → IPS |
| `BRAIN_KG_EDGE_DEPENDS_ON` | Functional dependency | Phonological WM → SNN |

#### KG Registration Pattern

```c
// Register linguistics modules in brain KG
void linguistics_register_kg(brain_kg_t* kg) {
    // Register core modules
    brain_kg_node_id_t spatial = brain_kg_add_node(kg,
        "parietal_spatial_language",
        BRAIN_KG_NODE_COGNITIVE,
        "Angular Gyrus spatial language processing");

    brain_kg_node_id_t numerical = brain_kg_add_node(kg,
        "parietal_numerical_language",
        BRAIN_KG_NODE_COGNITIVE,
        "Intraparietal Sulcus number word processing");

    brain_kg_node_id_t phonological = brain_kg_add_node(kg,
        "parietal_phonological_wm",
        BRAIN_KG_NODE_MEMORY,
        "Supramarginal Gyrus phonological loop");

    // Register bridges
    brain_kg_node_id_t snn_bridge = brain_kg_add_node(kg,
        "linguistics_snn_bridge",
        BRAIN_KG_NODE_BRIDGE,
        "SNN population encoding for linguistics");

    // Wire edges
    brain_kg_add_edge(kg, spatial, numerical,
        BRAIN_KG_EDGE_PROJECTS_TO,
        "Spatial-numerical interaction", 0.8f);

    brain_kg_add_edge(kg, phonological, spatial,
        BRAIN_KG_EDGE_EXCITES,
        "Phonological→spatial binding", 0.7f);

    brain_kg_add_edge(kg, snn_bridge, spatial,
        BRAIN_KG_EDGE_DEPENDS_ON,
        "SNN encoding dependency", 1.0f);
}
```

### 2.19 Logging Integration

#### Logging Infrastructure

| File | Purpose |
|------|---------|
| `include/utils/logging/nimcp_logging.h` | Async logging with lock-free ring buffer |

#### Log Levels

| Level | Use Case |
|-------|----------|
| `LOG_TRACE` | Detailed debugging (phoneme-by-phoneme) |
| `LOG_DEBUG` | Development debugging |
| `LOG_INFO` | Normal operation milestones |
| `LOG_WARN` | Potential issues (unknown word) |
| `LOG_ERROR` | Recoverable errors |
| `LOG_FATAL` | Unrecoverable errors |

#### Module-Specific Logging

**Module IDs for Linguistics:**
```c
#define LOG_MODULE_SPATIAL_LANG    "SPATIAL_LANG"
#define LOG_MODULE_NUMERICAL_LANG  "NUMERICAL_LANG"
#define LOG_MODULE_PHONOLOGICAL_WM "PHONOLOGICAL_WM"
#define LOG_MODULE_LING_SNN        "LING_SNN"
#define LOG_MODULE_LING_PLASTICITY "LING_PLASTICITY"
#define LOG_MODULE_LING_TRAINING   "LING_TRAINING"
```

**Logging Pattern:**
```c
// Use module-specific logging
LOG_MODULE_DEBUG(LOG_MODULE_SPATIAL_LANG,
    "Parsing preposition: %s, frame: %d", word, frame);

LOG_MODULE_INFO(LOG_MODULE_NUMERICAL_LANG,
    "Number parsed: %s -> %.2f", word, magnitude);

LOG_MODULE_WARN(LOG_MODULE_PHONOLOGICAL_WM,
    "Buffer capacity warning: %d/%d items", count, capacity);

LOG_MODULE_ERROR(LOG_MODULE_LING_SNN,
    "Spike encoding failed for word: %s", word);
```

**Structured Logging (JSON format):**
```c
// For metrics and analysis
nimcp_log_structured(LOG_INFO, LOG_MODULE_SPATIAL_LANG,
    "{"
    "\"event\": \"preposition_parsed\","
    "\"word\": \"%s\","
    "\"frame\": \"%s\","
    "\"confidence\": %.3f,"
    "\"latency_ms\": %.2f"
    "}", word, frame_name, confidence, latency);
```

### 2.20 Symbolic Logic & LGSS Integration

#### Symbolic Logic Infrastructure

| File | Purpose |
|------|---------|
| `include/cognitive/symbolic_logic/nimcp_symbolic_logic.h` | Core logic engine (FOL) |
| `include/cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.h` | JSON rule loading |
| `include/cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h` | mprotect-locked Safety KB |

#### Value-Add for Linguistics

**1. Spatial Reasoning via Logic:**
```c
// Define spatial relations as logical predicates
// "The cup is left of the book"
// PREDICATE: left_of(cup, book)
// INFERENCE: if left_of(X, Y) then right_of(Y, X)

symbolic_rule_t spatial_rules[] = {
    {
        .antecedent = "left_of(X, Y)",
        .consequent = "right_of(Y, X)",
        .confidence = 1.0f
    },
    {
        .antecedent = "above(X, Y) AND above(Y, Z)",
        .consequent = "above(X, Z)",  // Transitivity
        .confidence = 1.0f
    },
    {
        .antecedent = "near(X, Y)",
        .consequent = "near(Y, X)",   // Symmetry
        .confidence = 1.0f
    }
};
```

**2. Number Word Consistency:**
```c
// Logical constraints for numerical language
// RULE: if parse("twenty") = 20 AND parse("three") = 3
//       then parse("twenty-three") = 23

symbolic_rule_t numerical_rules[] = {
    {
        .antecedent = "magnitude(X, M1) AND magnitude(Y, M2) AND tens_ones(X, Y)",
        .consequent = "magnitude(compound(X,Y), M1 + M2)",
        .confidence = 1.0f
    },
    {
        .antecedent = "ordinal(N) AND N > 0",
        .consequent = "cardinal(N - 1) precedes ordinal(N)",
        .confidence = 1.0f
    }
};
```

**3. LGSS (Logical Guardrails Safety Schema):**

**Safety Domains for Linguistics:**
| Domain | Purpose | Example |
|--------|---------|---------|
| `SAFETY_DOMAIN_HARMFUL_SPEECH` | Block harmful language | Profanity, hate speech |
| `SAFETY_DOMAIN_MISINFORMATION` | Prevent false claims | "2+2=5" |
| `SAFETY_DOMAIN_PRIVACY` | Protect personal info | PII detection |

**LGSS JSON Rule Format:**
```json
{
  "rule_id": "LING_SAFETY_001",
  "domain": "HARMFUL_SPEECH",
  "priority": "L0",
  "condition": {
    "predicate": "contains_harmful_content",
    "args": ["$word"]
  },
  "action": "BLOCK",
  "message": "Harmful content detected"
}
```

**Integration Pattern:**
```c
// Check word against LGSS before processing
lgss_result_t safety_check = lgss_evaluate_word(
    safety_kb, word,
    SAFETY_DOMAIN_HARMFUL_SPEECH | SAFETY_DOMAIN_MISINFORMATION);

if (safety_check.blocked) {
    LOG_MODULE_WARN(LOG_MODULE_SPATIAL_LANG,
        "Word blocked by LGSS: %s, rule: %s",
        word, safety_check.rule_id);
    return NIMCP_ERROR_SAFETY_BLOCKED;
}
```

**4. Forward vs Backward Chaining:**

| Method | Use Case | Linguistics Application |
|--------|----------|------------------------|
| **Forward Chaining** | Inductive reasoning | Given "cup left of book" → derive spatial state |
| **Backward Chaining** | Deductive/goal-driven | Given target location → find path via prepositions |

**5. FEP Bridge Integration:**
The symbolic logic system integrates with Free Energy Principle:
- Symbolic predictions contribute to world model
- Prediction errors update both symbolic and subsymbolic representations
- Logical consistency constraints reduce free energy

### 2.21 Basal Ganglia Integration

#### Basal Ganglia Infrastructure

| File | Purpose |
|------|---------|
| `include/core/brain/regions/basal_ganglia/nimcp_basal_ganglia.h` | Main orchestrator |
| `include/core/brain/regions/basal_ganglia/nimcp_striatum.h` | D1/D2 MSN pathways |
| `include/core/brain/regions/basal_ganglia/nimcp_substantia_nigra.h` | Dopamine production |
| `include/core/brain/regions/basal_ganglia/bridges/nimcp_basal_ganglia_fep_bridge.h` | FEP action selection |
| `include/core/brain/regions/basal_ganglia/bridges/nimcp_basal_ganglia_executive_bridge.h` | PFC control |
| `include/core/brain/regions/basal_ganglia/bridges/nimcp_basal_ganglia_training_bridge.h` | RL plasticity |

#### Value-Add for Parietal Linguistics

**1. Word/Phoneme Selection as Action Selection:**
```c
// Each word form = action candidate
action_candidate_t word_actions[] = {
    {.action_id = WORD_NEAR,     .name = "near",     .value = 0.8f},
    {.action_id = WORD_ADJACENT, .name = "adjacent", .value = 0.6f},
    {.action_id = WORD_BESIDE,   .name = "beside",   .value = 0.5f}
};

// D1 pathway (GO): Selects most appropriate word
// D2 pathway (NO-GO): Suppresses competing/incorrect words
// Competition resolves via winner-take-all
selected_word = basal_ganglia_select_action(bg, cortical_input);
```

**2. Procedural Grammar Learning:**
- Grammar rules learned as habits via DLS (dorsolateral striatum)
- Example: "subject-verb agreement" as procedural memory
- Habit strength ≥ 0.7 → automatic rule application
- Dopamine RPE signals correct/incorrect syntax

**3. Reward-Based Vocabulary Acquisition:**
```c
// Training bridge: Three-factor learning (Pre × Post × DA)
// Positive feedback → strengthen D1 weights for word
// Negative feedback → strengthen D2 weights (suppress word)

bgtr_bridge_record_action(training, word_action_id);  // Eligibility trace
// ... word used in context ...
bgtr_bridge_process_reward(training, reward);          // TD update
```

**4. Habit Formation for Frequent Phrases:**
```c
// Frequent expression: "on the left" becomes automatic
habit_t phrase_habit = {
    .context_hash = hash("spatial_description"),
    .action_sequence = {WORD_ON, WORD_THE, WORD_LEFT},
    .strength = 0.85f  // > 0.7 threshold → automatic
};

// After 7+ repetitions: goal-directed → habitual
// Faster response, lower cognitive cost
```

**5. Three Pathways for Linguistics:**

| Pathway | Function | Linguistics Application |
|---------|----------|------------------------|
| **Direct (D1)** | "GO" signal | Select target word/form |
| **Indirect (D2)** | "NO-GO" signal | Suppress incorrect forms |
| **Hyperdirect (STN)** | Emergency stop | Abort socially inappropriate utterance |

**6. Operating Modes for Language Production:**

| Mode | Description | Linguistics Application |
|------|-------------|------------------------|
| `BG_MODE_GOAL_DIRECTED` | PFC-guided, deliberate | Novel sentence construction |
| `BG_MODE_HABITUAL` | Automatic, fast | Formulaic expressions ("How are you?") |
| `BG_MODE_EXPLORATORY` | Trying new combinations | Creative language, wordplay |
| `BG_MODE_SUPPRESSED` | Movement inhibition | Speech hesitation, self-correction |

**7. FEP Bridge for Word Selection:**
```c
// Word selection as expected free energy minimization
bg_fep_config_t config = {
    .model = BG_FEP_MODEL_EXPLOIT,  // Maximize communicative utility
    .precision_weight = 0.8f,
    .epistemic_weight = 0.2f        // Some exploration for novel words
};

// Evaluate all word candidates via EFE
bg_fep_evaluate_actions(fep_bridge, word_candidates, num_words);
selected_word = bg_fep_select_action(fep_bridge);
```

**8. Executive Bridge for Grammar Control:**
```c
// PFC top-down control for goal-directed speech
bge_bridge_register_goal(exec_bridge,
    GOAL_GRAMMATICALLY_CORRECT,
    0.9f);  // High priority

// Suppress grammatically incorrect forms
bge_bridge_inhibit_action(exec_bridge,
    incorrect_verb_form,
    INHIBIT_FULL);

// Detect goal-habit conflict (formulaic vs precise)
conflict = bge_bridge_detect_conflict(exec_bridge);
```

**9. Training Bridge for Vocabulary Learning:**
```c
typedef struct {
    bgtr_learning_type_t type;      // THREE_FACTOR or HABIT_FORMATION
    float learning_rate;            // 0.01 default
    float trace_decay;              // 0.95 (eligibility trace)
    float d1_lr_mult;               // D1 pathway learning rate
    float d2_lr_mult;               // D2 pathway learning rate
} linguistics_bg_training_config_t;

// D1/D2 asymmetric learning:
// Positive RPE → D1 LTP, D2 LTD (strengthen correct word)
// Negative RPE → D1 LTD, D2 LTP (weaken incorrect word)
```

**10. Integration with Dopamine System:**
- Training layer Loss→RPE→DA pipeline feeds BG dopamine
- SNc tonic (4.5 Hz) → baseline word selection
- SNc burst (18 Hz) → positive feedback, strengthen association
- SNc pause (0.5 Hz) → negative feedback, weaken association

#### Bridge: `nimcp_parietal_linguistics_basal_ganglia_bridge.h`

**Key Types:**
```c
typedef struct {
    basal_ganglia_t* bg;                    // Core BG module
    bg_fep_bridge_t* fep_bridge;            // FEP action selection
    bge_bridge_t* exec_bridge;              // Executive control
    bgtr_bridge_t* training_bridge;         // RL plasticity

    // Linguistics-specific
    action_candidate_t* word_actions;       // Word candidates
    habit_t* phrase_habits;                 // Learned phrases
    uint32_t vocab_size;

    // Mode tracking
    bg_operating_mode_t speech_mode;        // Goal/Habit/Explore
    float grammatical_precision;            // PFC control strength
} linguistics_bg_bridge_t;
```

**Key Functions:**
```c
// Word selection via BG competition
uint32_t ling_bg_select_word(
    linguistics_bg_bridge_t* bridge,
    const float* word_activations,
    uint32_t num_candidates);

// Grammar rule as habit
int ling_bg_register_grammar_habit(
    linguistics_bg_bridge_t* bridge,
    const char* rule_name,
    const uint32_t* action_sequence,
    uint32_t sequence_len);

// Reward-based learning
int ling_bg_process_feedback(
    linguistics_bg_bridge_t* bridge,
    float reward);

// Mode queries
bg_operating_mode_t ling_bg_get_speech_mode(
    const linguistics_bg_bridge_t* bridge);

// Conflict detection (word competition)
float ling_bg_get_word_conflict(
    const linguistics_bg_bridge_t* bridge);
```

### 2.22 Cerebellum Integration (Speech Timing & Motor Coordination)

#### Cerebellum Infrastructure

| File | Purpose |
|------|---------|
| `include/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h` | Core orchestrator |
| `include/language/bridges/nimcp_language_cerebellum_bridge.h` | **EXISTING** speech timing bridge |
| `include/core/medulla/nimcp_medulla_cerebellum_bridge.h` | Inferior olive error signaling |
| `include/cognitive/memory/core/nimcp_pr_cerebellum_bridge.h` | Procedural memory sequences |

#### Existing Language-Cerebellum Bridge (Leverage This)

**Already implemented speech timing functions:**
```c
// Speech rate control
language_cerebellum_set_speech_rate(bridge, syllables_per_sec);
float rate = language_cerebellum_get_speech_rate(bridge);

// Rhythm pattern generation
language_cerebellum_timing_pattern_t pattern;
language_cerebellum_get_timing_pattern(bridge, RHYTHM_STRESS_TIMED, &pattern);

// Duration prediction & learning
float predicted_ms = language_cerebellum_predict_duration(bridge, phoneme_id);
language_cerebellum_report_actual(bridge, phoneme_id, actual_ms);
language_cerebellum_update_model(bridge);  // Error-based learning

// Rhythm generation
language_cerebellum_start_rhythm(bridge, beats_per_minute);
language_cerebellum_sync_to_beat(bridge);
language_cerebellum_stop_rhythm(bridge);
```

**Rhythm Types (4 patterns):**
| Type | Description | Languages |
|------|-------------|-----------|
| `RHYTHM_ISOCHRONOUS` | Equal timing | Metronome-like |
| `RHYTHM_STRESS_TIMED` | Stress intervals equal | English, German |
| `RHYTHM_SYLLABLE_TIMED` | Syllable intervals equal | French, Spanish |
| `RHYTHM_MORA_TIMED` | Mora intervals equal | Japanese |

#### Value-Add for Parietal Linguistics

**1. Speech Timing & Prosody:**
```c
// Integrate with phonological WM for timing
phonological_trace_t trace;
phonological_wm_encode(pwm, word, &trace);

// Get cerebellum timing for each phoneme
for (int i = 0; i < trace.length; i++) {
    float duration = language_cerebellum_predict_duration(
        cereb_bridge, trace.phoneme_ids[i]);
    trace.durations[i] = duration;
}
```

**2. 8-DOF Motor Output for Articulation:**
```c
typedef struct {
    float jaw_opening;        // [0-1] Jaw position
    float tongue_position;    // [0-1] Front-back
    float tongue_height;      // [0-1] High-low
    float lip_rounding;       // [0-1] Spread-round
    float lip_protrusion;     // [0-1] Retracted-protruded
    float vocal_cord_tension; // [0-1] Lax-tense
    float airflow_pressure;   // [0-1] Breath support
    float velum_position;     // [0-1] Oral-nasal
} articulatory_state_t;

// Cerebellum generates motor commands
nuclei_output_t motor_cmd;
cerebellum_get_nuclei_output(cerebellum, &motor_cmd);
// Map to articulatory state for phoneme production
```

**3. Error-Based Pronunciation Learning:**
```c
// Climbing fiber signals encode pronunciation errors
climbing_fiber_signal_t error = {
    .error_type = MED_CEREB_ERROR_TIMING,      // Or AMPLITUDE, TRAJECTORY
    .magnitude = fabs(target_formant - actual_formant),
    .zone_id = ARTICULATION_ZONE
};
cerebellum_process_climbing_signal(cerebellum, &error);

// Triggers LTD at parallel fiber-Purkinje synapses
// Weakens incorrect articulation patterns
// LTD rate: 0.001, LTP rate: 0.0001
```

**4. Forward Models for Predictive Articulation:**
```c
// Predict acoustic outcome of motor command
float predicted_formants[4];
float confidence;
cerebellum_predict_outcome(cerebellum,
    articulatory_cmd, 8,  // 8 DOF
    predicted_formants, &confidence);

// Compare with target, generate error for learning
float error = compute_formant_error(target, predicted_formants);
if (error > threshold) {
    cerebellum_broadcast_error(cerebellum, error, MED_CEREB_ERROR_PREDICTION);
}
```

**5. Automatization Tracking:**
```c
typedef enum {
    AUTOMATIZATION_NOVICE,      // Requires conscious attention
    AUTOMATIZATION_ADVANCED,    // Reduced attention
    AUTOMATIZATION_PROFICIENT,  // Mostly automatic
    AUTOMATIZATION_EXPERT       // Fully automatic
} automatization_level_t;

// Track phoneme/word production automaticity
automatization_level_t level;
pr_cerebellum_bridge_get_automatization(pr_cereb, word_sequence_id, &level);
```

**6. Procedural Memory for Speech Sequences:**
```c
// Create motor sequence for word
pr_sequence_id_t seq = pr_cerebellum_bridge_create_sequence(
    pr_cereb, "word_hello", PR_TYPE_MOTOR);

// Add phoneme articulation elements
pr_cerebellum_bridge_add_element(pr_cereb, seq, &phoneme_h_cmd);
pr_cerebellum_bridge_add_element(pr_cereb, seq, &phoneme_eh_cmd);
pr_cerebellum_bridge_add_element(pr_cereb, seq, &phoneme_l_cmd);
pr_cerebellum_bridge_add_element(pr_cereb, seq, &phoneme_ow_cmd);

// Execute sequence (handles timing automatically)
pr_cerebellum_bridge_execute_next(pr_cereb, seq);
```

### 2.23 Medulla Integration (Arousal, Breathing, Protection)

#### Medulla Infrastructure

| File | Purpose |
|------|---------|
| `include/core/brain/regions/medulla/nimcp_medulla.h` | Core orchestrator |
| `include/core/medulla/nimcp_medulla_cerebellum_bridge.h` | Inferior olive error signaling |
| `include/core/brain/regions/hypothalamus/bridges/nimcp_hypothalamus_medulla_bridge.h` | Drive → autonomic |

#### Value-Add for Parietal Linguistics

**1. Arousal Modulation of Speech:**
```c
// Get current arousal level
float arousal = medulla_get_arousal_level(medulla);
arousal_level_t level = medulla_get_arousal_state(medulla);

// Arousal affects speech characteristics
typedef struct {
    float speech_rate_mult;      // 0.7 (drowsy) - 1.3 (alert)
    float motor_precision;       // Inverted U: best at ALERT
    float volume_control;        // Degraded at extremes
    float emotional_coloring;    // Increased at high arousal
} arousal_speech_effects_t;

// Map arousal level to speech effects
arousal_speech_effects_t effects = map_arousal_to_speech(level);
```

**Arousal Levels & Speech Effects:**
| Level | Speech Rate | Precision | Volume | Emotion |
|-------|-------------|-----------|--------|---------|
| COMA | 0 | 0 | 0 | 0 |
| DEEP_SLEEP | 0 | 0 | 0 | 0 |
| LIGHT_SLEEP | 0.3x | 0.2 | 0.3 | 0.5 |
| DROWSY | 0.7x | 0.5 | 0.6 | 0.7 |
| AWAKE | 0.9x | 0.8 | 0.9 | 0.8 |
| ALERT | 1.0x | 1.0 | 1.0 | 1.0 |
| HYPERAROUSAL | 1.3x | 0.7 | 1.2 | 1.5 |

**2. Breathing Coordination for Speech:**
```c
// Respiratory rate affects syllable timing
float resp_rate = medulla_get_respiratory_rate(medulla);

// Coordinate speech with breathing cycle
typedef struct {
    float breath_phase;          // 0-1 (inhale-exhale)
    float available_airflow;     // For phonation
    float phrase_boundary;       // Natural pause point
} speech_breath_coord_t;

// Plan utterance around breath boundaries
speech_breath_coord_t coord;
medulla_get_breath_state(medulla, &coord);
if (coord.available_airflow < threshold) {
    // Insert breath pause in utterance
    insert_breath_pause(&utterance);
}
```

**3. Protection Level Gating of Speech:**
```c
protection_level_t prot = medulla_get_protection_level(medulla);

// Speech capabilities per protection level
switch (prot) {
    case PROTECTION_LEVEL_NORMAL:
        // Full speech capability
        break;
    case PROTECTION_LEVEL_CAUTIOUS:
        // Slightly reduced elaboration
        break;
    case PROTECTION_LEVEL_GUARDED:
        // Essential communication only
        break;
    case PROTECTION_LEVEL_DEFENSIVE:
        // Critical messages only
        speech_mode = SPEECH_MODE_ESSENTIAL;
        break;
    case PROTECTION_LEVEL_CRITICAL:
        // Alarm vocalizations only
        speech_mode = SPEECH_MODE_ALARM;
        break;
    case PROTECTION_LEVEL_SHUTDOWN:
        // No voluntary speech
        speech_mode = SPEECH_MODE_DISABLED;
        break;
}
```

**4. Circadian Effects on Speech:**
```c
circadian_phase_t phase = medulla_get_circadian_phase(medulla);

// Time-of-day speech quality
typedef struct {
    float clarity_mult;          // Voice clarity
    float fluency_mult;          // Word finding
    float error_rate_mult;       // Pronunciation errors
    float learning_rate_mult;    // Vocabulary acquisition
} circadian_speech_effects_t;

// Peak performance: MORNING (09:00-12:00)
// Post-lunch dip: AFTERNOON (12:00-15:00)
// Evening decline: EVENING onwards
```

**Circadian Speech Effects:**
| Phase | Clarity | Fluency | Errors | Learning |
|-------|---------|---------|--------|----------|
| EARLY_MORNING | 0.7 | 0.6 | 1.4x | 0.8 |
| MORNING | 1.0 | 1.0 | 1.0x | 1.2 |
| AFTERNOON | 0.8 | 0.8 | 1.2x | 0.9 |
| EVENING | 0.9 | 0.9 | 1.1x | 1.0 |
| LATE_EVENING | 0.7 | 0.7 | 1.3x | 0.7 |
| NIGHT | 0.5 | 0.5 | 1.5x | 0.5 |
| DEEP_NIGHT | 0.3 | 0.3 | 2.0x | 0.3 |
| PRE_DAWN | 0.4 | 0.4 | 1.8x | 0.4 |

**5. Autonomic Feedback for Emotional Prosody:**
```c
// Get autonomic state
float heart_rate = medulla_get_heart_rate_analog(medulla);
float resp_depth = medulla_get_respiratory_depth(medulla);

// Map to vocal characteristics
typedef struct {
    float pitch_variation;       // F0 range
    float tempo_variation;       // Speech rate variance
    float intensity_variation;   // Volume dynamics
    float voice_quality;         // Breathiness, tension
} emotional_prosody_t;

// High arousal: increased pitch, tempo, intensity
// Low arousal: reduced variation, breathy voice
emotional_prosody_t prosody = compute_emotional_prosody(
    heart_rate, resp_depth, arousal);
```

**6. Vocal Tremor at Arousal Extremes:**
```c
// Inverted U-curve for motor precision
float precision = compute_precision_from_arousal(arousal);
// Precision peaks at ALERT (0.55-0.70 arousal)
// Degrades at both low (drowsy) and high (anxious) extremes

if (precision < 0.7f) {
    // Add tremor to voice output
    float tremor_magnitude = (1.0f - precision) * MAX_TREMOR;
    apply_vocal_tremor(&articulatory_cmd, tremor_magnitude);
}
```

#### Bridge: `nimcp_parietal_linguistics_medulla_bridge.h`

```c
typedef struct {
    medulla_t* medulla;

    // Cached effects
    arousal_speech_effects_t arousal_effects;
    circadian_speech_effects_t circadian_effects;
    speech_breath_coord_t breath_state;

    // Speech gating
    speech_mode_t current_mode;
    bool speech_allowed;

    // Prosody modulation
    emotional_prosody_t prosody;
} linguistics_medulla_bridge_t;

// Key functions
float ling_medulla_get_speech_rate_mult(linguistics_medulla_bridge_t* bridge);
float ling_medulla_get_motor_precision(linguistics_medulla_bridge_t* bridge);
bool ling_medulla_is_speech_allowed(linguistics_medulla_bridge_t* bridge);
void ling_medulla_coordinate_breath(linguistics_medulla_bridge_t* bridge,
                                     utterance_t* utterance);
void ling_medulla_apply_prosody(linguistics_medulla_bridge_t* bridge,
                                 articulatory_cmd_t* cmd);
```

#### Bridge: `nimcp_parietal_linguistics_cerebellum_bridge.h`

```c
typedef struct {
    cerebellum_adapter_t* cerebellum;
    language_cerebellum_bridge_t* lang_cereb;  // Existing bridge
    pr_cerebellum_bridge_t* pr_cereb;          // Procedural memory
    med_cerebellum_bridge_t* med_cereb;        // Inferior olive errors

    // Articulatory state
    articulatory_state_t current_articulation;

    // Timing state
    float current_speech_rate;
    language_cerebellum_rhythm_t rhythm_type;

    // Learning state
    automatization_level_t word_automatization[MAX_VOCAB];

    // Forward model state
    float predicted_formants[4];
    float prediction_confidence;
} linguistics_cerebellum_bridge_t;

// Key functions
void ling_cereb_predict_phoneme_timing(linguistics_cerebellum_bridge_t* bridge,
                                        uint32_t phoneme_id, float* duration_ms);
void ling_cereb_report_timing_error(linguistics_cerebellum_bridge_t* bridge,
                                     uint32_t phoneme_id, float error_ms);
void ling_cereb_get_articulatory_cmd(linguistics_cerebellum_bridge_t* bridge,
                                      uint32_t phoneme_id, articulatory_state_t* cmd);
void ling_cereb_learn_pronunciation(linguistics_cerebellum_bridge_t* bridge,
                                     uint32_t phoneme_id, float acoustic_error);
automatization_level_t ling_cereb_get_word_fluency(linguistics_cerebellum_bridge_t* bridge,
                                                    uint32_t word_id);
```

### 2.24 Perception Layer Integration (Speech & Visual)

#### Perception Infrastructure

| File | Purpose |
|------|---------|
| `include/perception/nimcp_cochlea.h` | Peripheral auditory pathway |
| `include/perception/nimcp_audio_cortex.h` | Primary auditory cortex (A1) |
| `include/perception/nimcp_speech_cortex.h` | Speech-specialized processing |
| `include/perception/nimcp_lip_reading.h` | Visual speech perception |
| `include/perception/nimcp_visual_cortex.h` | V1-style visual processing |
| `include/perception/bridges/nimcp_cochlea_broca_bridge.h` | Speech perception-production link |
| `include/perception/bridges/nimcp_cochlea_occipital_bridge.h` | Audiovisual binding |

#### Value-Add for Parietal Linguistics

**1. Speech Perception Pathway:**
```c
// Cochlea → Audio Cortex → Speech Cortex → Phonemes
cochlea_output_t cochlear_out;
cochlea_process(cochlea, audio_samples, num_samples, &cochlear_out);

// Audio cortex extracts features
audio_cortex_compute_mel_features(audio_ctx, &cochlear_out, mel_features);
audio_cortex_compute_mfcc(audio_ctx, mel_features, mfcc_features);

// Speech cortex detects phonemes
phoneme_event_t phonemes[MAX_PHONEMES];
uint32_t num_phonemes;
speech_cortex_detect_phonemes(speech_ctx, mfcc_features, phonemes, &num_phonemes);

// Each phoneme includes:
// - phoneme category (44 English phonemes)
// - confidence [0-1]
// - formants (F1, F2, F3, F4)
// - duration, pitch, intensity
// - position_embedding for sequence
```

**2. 44-Phoneme Inventory (English IPA):**
| Category | Phonemes |
|----------|----------|
| Vowels (12) | IY, IH, EY, EH, AE, AA, AO, OW, UH, UW, AH, ER |
| Stops (6) | P, B, T, D, K, G |
| Fricatives (9) | F, V, TH, DH, S, Z, SH, ZH, H |
| Nasals (3) | M, N, NG |
| Approximants (4) | L, R, W, Y |
| Affricates (2) | CH, JH |
| Other (2) | SILENCE, UNKNOWN |

**3. Formant Extraction for Vowel Processing:**
```c
// Extract formants for vowel classification
formant_values_t formants;
speech_cortex_extract_formants(speech_ctx, frame, &formants);
// F1: 200-900 Hz (tongue height)
// F2: 700-2300 Hz (tongue frontness)
// F3: 1300-3500 Hz (rhoticity)

// Classify vowel from F1/F2
phoneme_t vowel = speech_cortex_classify_vowel(speech_ctx, formants.f1, formants.f2);
```

**4. Prosody Perception:**
```c
// Extract prosodic features
prosody_features_t prosody;
speech_cortex_extract_prosody(speech_ctx, audio_frame, &prosody);

// Returns:
// - pitch_hz: Fundamental frequency (F0)
// - stress_level [0-1]: Syllable stress
// - tone_contour: Pitch trajectory (for tonal languages)
// - rhythm_pattern: Inter-phoneme timing
```

**5. Phonological Working Memory (Already in Speech Cortex):**
```c
// Store phonemes in Baddeley-style phonological buffer
speech_cortex_store_phonological_buffer(speech_ctx, phoneme_sequence, num_phonemes);

// Capacity: 7±2 items (Miller's law)
// Decay: ~2 seconds without rehearsal
// Rehearsal: Subvocal loop via Broca's area
```

**6. Lip Reading & Audiovisual Integration:**
```c
// Visual speech perception
lip_reading_result_t lip_result;
lip_reading_process_frame(lip_ctx, video_frame, &lip_result);

// 12 viseme categories (visual phoneme groups)
typedef enum {
    VISEME_SILENCE,      // Neutral
    VISEME_P_B_M,        // Bilabial closure
    VISEME_F_V,          // Labiodental
    VISEME_TH_DH,        // Dental
    VISEME_T_D_S_Z_N_L,  // Alveolar
    VISEME_SH_ZH_CH_JH,  // Postalveolar
    VISEME_K_G_NG,       // Velar
    VISEME_R,            // Retroflex
    VISEME_W,            // Rounded
    VISEME_IY_IH,        // High front vowels
    VISEME_EY_EH_AE,     // Mid/low front vowels
    VISEME_AA_AO_OW_UW   // Back vowels
} viseme_t;

// McGurk-style audiovisual fusion
audiovisual_integration_t av_result;
lip_reading_integrate_audiovisual(lip_ctx,
    &lip_result,         // Visual input
    &audio_phoneme,      // Auditory input
    snr_db,              // Signal-to-noise ratio
    &av_result);

// SNR-dependent weighting:
// High SNR (>10dB): audio_weight = 0.8, visual_weight = 0.2
// Low SNR (<-10dB): audio_weight = 0.2, visual_weight = 0.8
// Result: fused_phoneme with higher confidence
```

**7. Silent Speech Recognition:**
```c
// When audio is absent or too noisy
if (snr_db < -20.0f) {
    viseme_sequence_t visemes;
    lip_reading_recognize_silent_speech(lip_ctx,
        video_frames, num_frames,
        &visemes);

    // Viseme-to-phoneme disambiguation
    // ~60% word accuracy in silent conditions
    phoneme_sequence_t phonemes;
    lip_reading_visemes_to_phonemes(lip_ctx, &visemes, &phonemes);
}
```

**8. Speaker Adaptation:**
```c
// Register speaker profile for accent/style adaptation
speaker_id_t speaker = lip_reading_register_speaker(lip_ctx, "speaker_1");

// Update profile as speaker is observed
lip_reading_update_speaker_profile(lip_ctx, speaker,
    visual_features, audio_phonemes, num_frames);

// After ~300 frames (~10 seconds), accuracy improves significantly
```

**9. Visual Word Form Recognition (Reading):**
```c
// Visual cortex processes written text
visual_features_t vis_features;
visual_cortex_process(visual_ctx, text_image, &vis_features);

// Gabor-filtered edge features → grapheme recognition
// V1 → V2 → V4 → VWFA (Visual Word Form Area)

// Connect to angular gyrus for grapheme-to-phoneme
// This feeds into spatial language module
```

**10. Top-Down Predictions (Bidirectional):**
```c
// Linguistics → Perception: Predict expected phoneme
speech_cortex_set_phoneme_prior(speech_ctx, expected_phoneme, prior_strength);

// Perception → Linguistics: Report prediction error
float prediction_error = speech_cortex_get_prediction_error(speech_ctx);

// Attention modulation for specific frequencies
audio_cortex_apply_attention(audio_ctx, frequency_range, attention_weight);

// Boost visual attention to mouth region
lip_reading_set_roi_attention(lip_ctx, mouth_region, 2.0f);
```

#### Integration with Parietal Linguistics

**Phoneme Input to Spatial/Numerical Language:**
```c
// Spatial language receives phoneme stream
phoneme_event_t* phoneme_stream;
uint32_t stream_len;
speech_cortex_get_phoneme_stream(speech_ctx, &phoneme_stream, &stream_len);

// Parse for spatial prepositions
for (uint32_t i = 0; i < stream_len; i++) {
    if (is_spatial_preposition_phonemes(&phoneme_stream[i], lookahead)) {
        spatial_semantics_t sem;
        spatial_language_parse_from_phonemes(spatial_ctx,
            &phoneme_stream[i], &sem);
    }
}

// Parse for number words
for (uint32_t i = 0; i < stream_len; i++) {
    if (is_number_word_phonemes(&phoneme_stream[i], lookahead)) {
        numerical_semantics_t num;
        numerical_language_parse_from_phonemes(numerical_ctx,
            &phoneme_stream[i], &num);
    }
}
```

**Prosody for Linguistic Meaning:**
```c
// Prosody affects spatial/numerical interpretation
prosody_features_t prosody;
speech_cortex_extract_prosody(speech_ctx, frame, &prosody);

// Stress affects word boundaries
// "REcord" (noun) vs "reCORD" (verb)
// Pitch contour affects sentence type (question vs statement)

// Apply to spatial language
spatial_language_apply_prosody(spatial_ctx, &prosody);
```

#### Bridge: `nimcp_parietal_linguistics_perception_bridge.h`

```c
typedef struct {
    // Auditory pathway
    cochlea_t* cochlea;
    audio_cortex_t* audio_ctx;
    speech_cortex_t* speech_ctx;

    // Visual pathway
    visual_cortex_t* visual_ctx;
    lip_reading_ctx_t* lip_ctx;

    // Integration state
    phoneme_stream_t current_phonemes;
    viseme_stream_t current_visemes;
    audiovisual_integration_t av_state;

    // Prosody state
    prosody_features_t current_prosody;

    // Top-down predictions
    phoneme_t predicted_phoneme;
    float prediction_confidence;

    // Speaker state
    speaker_id_t active_speaker;
} linguistics_perception_bridge_t;

// Key functions
int ling_perception_get_phoneme_stream(
    linguistics_perception_bridge_t* bridge,
    phoneme_event_t** phonemes,
    uint32_t* count);

int ling_perception_get_audiovisual_phoneme(
    linguistics_perception_bridge_t* bridge,
    float snr_db,
    phoneme_event_t* fused_phoneme);

int ling_perception_get_prosody(
    linguistics_perception_bridge_t* bridge,
    prosody_features_t* prosody);

int ling_perception_set_phoneme_prediction(
    linguistics_perception_bridge_t* bridge,
    phoneme_t predicted,
    float confidence);

int ling_perception_get_prediction_error(
    linguistics_perception_bridge_t* bridge,
    float* error);

int ling_perception_adapt_to_speaker(
    linguistics_perception_bridge_t* bridge,
    speaker_id_t speaker);
```

---

### 2.25 Theory of Mind & World Model Integration

#### Theory of Mind Infrastructure

| File | Purpose |
|------|---------|
| `include/cognitive/nimcp_theory_of_mind.h` | BDI model, false belief detection |
| `include/cognitive/omni/nimcp_omni_world_model.h` | RSSM architecture, counterfactual reasoning |
| `include/cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h` | Bidirectional ToM-WM integration |

#### Value-Add for Parietal Linguistics

**1. Reference Frame Selection via Perspective-Taking:**
```c
// Query ToM for whose perspective to use
tom_agent_state_t listener_state;
tom_get_agent_belief(tom, listener_id, "spatial_reference", &listener_state);

// Use empathy simulation for perspective alignment
omni_wm_tom_bridge_empathy_simulation(tom_wm_bridge,
    listener_id,
    EMPATHY_MODE_COGNITIVE,  // Understanding beliefs
    &perspective_shift);

// Select reference frame based on listener's expected perspective
reference_frame_t frame = (perspective_shift.lateral_distance < 0.3f)
    ? REF_FRAME_ALLOCENTRIC   // We share spatial reference
    : REF_FRAME_EGOCENTRIC;   // Translate to my perspective

// Apply to spatial language
spatial_language_set_reference_frame(spatial_ctx, frame);
```

**2. Pronoun Resolution ("I"/"you" tracking):**
```c
// Track speaker vs listener mental states
tom_bdi_state_t speaker_bdi, listener_bdi;
tom_get_bdi_state(tom, speaker_id, &speaker_bdi);
tom_get_bdi_state(tom, listener_id, &listener_bdi);

// Resolve deictic pronouns
if (strcmp(word, "I") == 0) {
    // Map to current speaker's BDI
    resolved_agent = speaker_id;
} else if (strcmp(word, "you") == 0) {
    // Map to listener's BDI
    resolved_agent = listener_id;
}

// Get resolved agent's spatial perspective
tom_get_agent_belief(tom, resolved_agent, "location", &agent_location);
```

**3. Spatial Deixis ("here"/"there"/"left"/"right"):**
```c
// Use ToM lateral dynamics for perspective
omni_wm_tom_bridge_get_lateral_dynamics(tom_wm_bridge, other_agent, &dynamics);

// Adjust spatial terms based on perspective gap
float perspective_distance = dynamics.lateral_distance;
if (perspective_distance > 0.5f) {
    // Agent has different spatial reference
    // "left" for me might be "right" for them
    spatial_adjustment = compute_reference_transform(
        my_location, my_orientation,
        agent_location, agent_orientation);
}

// Apply adjustment to spatial preposition
spatial_semantics_t adjusted = spatial_language_transform(
    original_semantics, spatial_adjustment);
```

**4. Communicative Intent Detection:**
```c
// Get ToM-inferred intention from utterance
tom_intention_t inferred_intent;
tom_infer_intention(tom, speaker_id, utterance_context, &inferred_intent);

// Compare stated vs inferred intent
tom_detect_false_belief(tom, speaker_id, &reality_gap);

// If mismatch detected, flag for clarification
if (fabsf(stated_intent.confidence - inferred_intent.confidence) > 0.3f) {
    LOG_MODULE_WARN(LOG_MODULE_SPATIAL_LANG,
        "Intent mismatch: stated %.2f vs inferred %.2f",
        stated_intent.confidence, inferred_intent.confidence);
    request_clarification = true;
}
```

**5. Audience Design (Adapting Language to Listener):**
```c
// Model listener's knowledge state
tom_agent_state_t listener_knowledge;
tom_get_agent_belief(tom, listener_id, "vocabulary", &listener_knowledge);

// Check if listener knows the spatial term
bool listener_knows_term = tom_check_belief(tom, listener_id,
    "knows_word", spatial_word);

// Adapt word choice based on listener model
if (!listener_knows_term) {
    // Use simpler synonym or add explanation
    spatial_word = spatial_language_find_simpler_synonym(spatial_ctx, spatial_word);
}

// Check listener's spatial sophistication
float spatial_expertise = tom_get_trait(tom, listener_id, "spatial_ability");
if (spatial_expertise < 0.5f) {
    // Use egocentric reference (simpler)
    frame = REF_FRAME_EGOCENTRIC;
} else {
    // Can use allocentric reference
    frame = REF_FRAME_ALLOCENTRIC;
}
```

**6. Common Ground Tracking:**
```c
// Compute shared vs private knowledge
tom_compute_common_ground(tom, speaker_id, listener_id, &common_ground);

// Get belief-reality gap for shared spatial knowledge
float reality_gap;
tom_detect_false_belief(tom, listener_id, &reality_gap);

// Track what's in common ground vs new information
typedef struct {
    bool location_shared;       // Both know current locations
    bool orientation_shared;    // Both know orientations
    bool landmarks_shared;      // Common reference points
    float common_ground_score;  // Overall overlap [0-1]
} spatial_common_ground_t;

spatial_common_ground_t spatial_cg;
compute_spatial_common_ground(tom, speaker_id, listener_id, &spatial_cg);

// Only mention new information
if (!spatial_cg.location_shared) {
    // Need to establish location first
    add_location_reference(&utterance);
}
```

**7. Counterfactual Spatial Reasoning:**
```c
// Use world model counterfactual for "what if" spatial scenarios
// "If you were at the door, the table would be on your left"

omni_wm_tom_bridge_counterfactual_belief(tom_wm_bridge,
    listener_id,
    "location", "at_door",  // Hypothetical belief change
    &counterfactual_state);

// Compute spatial relations in counterfactual
spatial_semantics_t cf_semantics;
spatial_language_compute_in_frame(spatial_ctx,
    counterfactual_state.location,
    counterfactual_state.orientation,
    table_location,
    &cf_semantics);
// Result: "on your left" relative to counterfactual position
```

**8. Mental State Prediction for Spatial Instructions:**
```c
// Predict how listener's mental state will evolve after instruction
// "Go left, then turn right at the intersection"

tom_mental_state_sequence_t predicted_states;
omni_wm_tom_bridge_predict_mental_state(tom_wm_bridge,
    listener_id,
    instructions, num_steps,
    &predicted_states);

// Verify listener will reach correct understanding
for (int i = 0; i < num_steps; i++) {
    if (predicted_states.confidence[i] < 0.7f) {
        // Listener likely confused at this step
        // Add clarifying instruction
        insert_clarification(&instructions, i);
    }
}
```

#### ToM Agent Model (32 Agents)

| Feature | Capability | Linguistics Application |
|---------|------------|------------------------|
| BDI Tracking | Beliefs, Desires, Intentions per agent | Pronoun resolution, intent detection |
| False Belief | Detect belief-reality mismatch | Listener modeling errors |
| Emotion Inference | 7 primary + 5 self-conscious emotions | Emotional prosody adaptation |
| Trait Modeling | 18 personality traits | Audience design |
| Lateral Dynamics | Perspective distance metric | Reference frame selection |

#### World Model RSSM Integration

| Component | Function | Linguistics Application |
|-----------|----------|------------------------|
| Deterministic state h_t | Stable world representation | Persistent spatial context |
| Stochastic state z_t | Uncertainty modeling | Ambiguous spatial reference |
| Transition model | p(z_t | h_t-1, a_t-1) | Predict outcome of spatial instruction |
| Observation model | p(o_t | h_t, z_t) | Ground language in perception |
| Reward model | p(r_t | h_t, z_t) | Communicative success prediction |

#### Bridge: `nimcp_parietal_linguistics_tom_wm_bridge.h`

```c
typedef struct {
    theory_of_mind_t* tom;
    omni_world_model_t* world_model;
    omni_wm_tom_bridge_t* tom_wm_bridge;

    // Agent tracking
    tom_agent_id_t speaker_id;
    tom_agent_id_t listener_id;
    tom_agent_id_t self_id;

    // Perspective state
    reference_frame_t current_frame;
    float perspective_alignment;       // [0-1] how aligned are perspectives

    // Common ground
    spatial_common_ground_t common_ground;

    // Counterfactual state (for hypotheticals)
    bool counterfactual_active;
    omni_mental_state_t counterfactual_state;
} linguistics_tom_wm_bridge_t;

// Key functions
int ling_tom_wm_select_reference_frame(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t listener,
    reference_frame_t* frame);

int ling_tom_wm_resolve_pronoun(
    linguistics_tom_wm_bridge_t* bridge,
    const char* pronoun,
    tom_agent_id_t* resolved_agent);

int ling_tom_wm_compute_spatial_deixis(
    linguistics_tom_wm_bridge_t* bridge,
    const char* deictic_word,
    tom_agent_id_t perspective_agent,
    spatial_semantics_t* semantics);

int ling_tom_wm_check_communicative_intent(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t speaker,
    const char* utterance,
    bool* intent_mismatch);

int ling_tom_wm_adapt_for_listener(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t listener,
    const char* original_word,
    char* adapted_word,
    uint32_t max_len);

int ling_tom_wm_get_common_ground(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t agent_a,
    tom_agent_id_t agent_b,
    spatial_common_ground_t* common_ground);

int ling_tom_wm_counterfactual_spatial(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t agent,
    const char* hypothetical_location,
    spatial_semantics_t* cf_semantics);

int ling_tom_wm_predict_understanding(
    linguistics_tom_wm_bridge_t* bridge,
    tom_agent_id_t listener,
    const char** instructions,
    uint32_t num_instructions,
    float* confidence_per_step);
```

#### Integration Call Patterns: ToM+WM → External Systems

**Outbound Calls:**
```c
// 1. THEORY OF MIND - Agent state queries
tom_bdi_state_t bdi;
tom_get_bdi_state(tom, agent_id, &bdi);
tom_get_agent_belief(tom, agent_id, belief_key, &belief_value);
tom_infer_emotion(tom, agent_id, context, &emotion);
tom_detect_false_belief(tom, agent_id, &gap);
tom_get_trait(tom, agent_id, trait_name);

// 2. WORLD MODEL - State and prediction
omni_wm_predict_state(world_model, action, &predicted_state);
omni_wm_get_observation_model(world_model, state, &observation);
omni_wm_counterfactual(world_model, intervention, &cf_state);

// 3. TOM-WM BRIDGE - Combined operations
omni_wm_tom_bridge_empathy_simulation(bridge, agent, mode, &result);
omni_wm_tom_bridge_predict_mental_state(bridge, agent, actions, &sequence);
omni_wm_tom_bridge_counterfactual_belief(bridge, agent, key, value, &cf);
omni_wm_tom_bridge_get_lateral_dynamics(bridge, agent, &dynamics);

// 4. SPATIAL LANGUAGE - Apply perspective
spatial_language_set_reference_frame(ctx, frame);
spatial_language_transform(semantics, transform);
spatial_language_compute_in_frame(ctx, loc, orient, target, &sem);

// 5. BASAL GANGLIA - Word selection with social context
float social_value = tom_get_social_reward(tom, agent_id, word_choice);
basal_ganglia_set_social_modulation(bg, social_value);
```

**Inbound Calls:**
```c
// Parietal orchestrator
case PARIETAL_LINGUISTICS_SELECT_FRAME:
    ling_tom_wm_select_reference_frame(bridge, listener_id, &frame);
    break;

case PARIETAL_LINGUISTICS_RESOLVE_PRONOUN:
    ling_tom_wm_resolve_pronoun(bridge, pronoun, &agent);
    break;

// World model queries spatial language
spatial_semantics_t sem;
spatial_language_parse_preposition("left", &sem);
omni_wm_update_spatial_belief(world_model, agent_id, &sem);
```

---

## 2.26 Integration Call Patterns (Verified Data Flow)

This section specifies the **actual function calls** that must occur between modules to ensure real integration, not just declarations.

### 2.26.1 Spatial Language → External Systems

**Outbound Calls (Spatial Language CALLS these):**
```c
// 1. FUZZY LOGIC - Get membership for spatial semantics
float membership = fuzzy_mf_evaluate(preposition->distance_mf, actual_distance);
fuzzy_hedge_t hedge = fuzzy_apply_hedge(FUZZY_HEDGE_VERY, membership);
float defuzzified = fuzzy_defuzzify(inference_result, DEFUZZ_CENTROID);

// 2. STATISTICS - Bayesian reference frame selection
float posterior = bayesian_update(prior, likelihood, evidence);
hmm_viterbi_decode(hmm, observations, &best_path);  // Number word sequences

// 3. SNN - Encode spatial word as spike train
snn_encode_rate(snn_bridge->spatial_word_pop, word_activation, spike_train);
snn_population_step(snn_bridge->spatial_word_pop, dt);

// 4. ENGRAM - Store spatial word→frame mapping
engram_encode(engram_system, &spatial_engram, ENGRAM_TYPE_SEMANTIC);
engram_tag_emotional(engram_system, engram_id, arousal, valence);

// 5. BASAL GANGLIA - Word selection via action competition
uint32_t selected = basal_ganglia_select_action(bg, word_activations);
bg_fep_evaluate_actions(fep_bridge, word_candidates, num_words);

// 6. HYPOTHALAMUS - Check drive modulation
float curiosity = hypothalamus_get_drive(hypo, DRIVE_CURIOSITY);
float social = hypothalamus_get_drive(hypo, DRIVE_SOCIAL);

// 7. OMNI WORLD MODEL - Predict spatial outcome
omni_wm_predict_state(world_model, "go left", &predicted_state);
float precision = omni_precision_get(precision_ctx, SPATIAL_MODALITY);

// 8. SYMBOLIC LOGIC - Apply spatial rules
symbolic_assert(logic_engine, "left_of(cup, book)");
symbolic_result_t* derived = symbolic_forward_chain(logic_engine);

// 9. EXCEPTION HANDLING - All error paths
NIMCP_CHECK_THROW_IMMUNE(word != NULL, NIMCP_ERROR_NULL_POINTER, "word is NULL");

// 10. LOGGING - All operations
LOG_MODULE_DEBUG(LOG_MODULE_SPATIAL_LANG, "Parsing: %s", word);
```

**Inbound Calls (External systems CALL spatial language):**
```c
// Parietal orchestrator dispatches to spatial language
case PARIETAL_SPATIAL_LANGUAGE_PARSE:
    spatial_language_parse_preposition(request->word, &result->semantics);
    break;

// Language-parietal bridge queries spatial semantics
spatial_semantics_t sem;
language_parietal_process_spatial_word(bridge, "above", spatial_vector, vec_size);

// Omni world model requests spatial interpretation
spatial_language_interpret_for_wm(spatial_ctx, utterance, &wm_update);
```

### 2.26.2 Numerical Language → External Systems

**Outbound Calls:**
```c
// 1. NUMBER SENSE - Weber-Fechner uncertainty
float uncertainty = number_sense_get_uncertainty(ns, magnitude);
bool subitizable = number_sense_can_subitize(ns, count);

// 2. HMM - Sequence prediction
float prob = hmm_forward(hmm, number_word_sequence);
hmm_state_t next = hmm_predict_next(hmm, current_state);

// 3. FUZZY - Quantifier semantics ("most", "few")
fuzzy_mf_t* mf = numerical_language_quantifier_to_fuzzy(QUANTIFIER_MOST);
float membership = fuzzy_mf_evaluate(mf, proportion);

// 4. BASAL GANGLIA - Number word selection
uint32_t selected = basal_ganglia_select_action(bg, number_word_activations);

// 5. PLASTICITY - Strengthen number associations
stdp_update(stdp_ctx, pre_spike_time, post_spike_time, &weight_delta);
triplet_stdp_update(triplet_ctx, spike_times, &weight_delta);  // Sequences

// 6. SYMBOLIC LOGIC - Numerical consistency
symbolic_assert(logic_engine, "magnitude(twenty, 20)");
symbolic_assert(logic_engine, "magnitude(three, 3)");
// Rule fires: magnitude(twenty_three, 23)
```

**Inbound Calls:**
```c
// Parietal orchestrator
case PARIETAL_NUMERICAL_LANGUAGE_PARSE:
    numerical_language_parse_word(request->word, &result->number);
    break;

// Language bridge
float number;
language_parietal_word_to_number(bridge, "twenty-three", &number);
```

### 2.26.3 Phonological WM → External Systems

**Outbound Calls:**
```c
// 1. SNN - Phoneme spike encoding
snn_encode_temporal(snn_bridge->phoneme_pop, phoneme_times, spike_train);
snn_population_step(snn_bridge->phoneme_pop, dt);

// 2. INFORMATION THEORY - Phoneme similarity
float similarity = mutual_information(phoneme_a_dist, phoneme_b_dist);
float complexity = shannon_entropy(phoneme_distribution);

// 3. THETA-GAMMA - Encoding/retrieval gating
float theta_phase = theta_gamma_get_phase(tg_ctx);
bool encoding_window = (theta_phase >= 0 && theta_phase < M_PI_2);
theta_gamma_trigger_gamma_burst(tg_ctx);  // Phoneme binding

// 4. FFT/HILBERT - Spectral analysis
fft_execute_real(fft_plan, phoneme_signal, spectrum);
hilbert_extract_phase(hilbert_ctx, signal, &instantaneous_phase);

// 5. ENGRAM - Store phonological pattern
engram_encode(engram_system, &phonological_engram, ENGRAM_TYPE_PROCEDURAL);

// 6. HEALTH MONITOR - Track buffer utilization
health_metrics_t metrics = {.memory_usage = pwm->count * sizeof(trace_t)};
health_monitor_update(health, &metrics);
```

**Inbound Calls:**
```c
// Parietal orchestrator
case PARIETAL_PHONOLOGICAL_WM_ENCODE:
    phonological_wm_encode(pwm, request->word, &trace);
    break;

// Rehearsal triggered by working memory system
phonological_wm_rehearse(pwm);  // Subvocal loop
```

### 2.26.4 Integration Flow: Complete Learning Cycle

**Step-by-step data flow for learning new word "adjacent":**

```c
// STEP 1: Input arrives at spatial language
int result = spatial_language_parse_preposition("adjacent", &semantics);

// STEP 2: BBB validates input
bbb_result_t bbb = bbb_validate_input("adjacent", 8, BBB_INPUT_TYPE_UTF8_STRING, 1024);
NIMCP_CHECK_THROW_IMMUNE(bbb.action == BBB_ACTION_ALLOW, ...);

// STEP 3: Unknown word - check LGSS safety first
lgss_result_t safety = lgss_evaluate_word(safety_kb, "adjacent", SAFETY_DOMAIN_ALL);
if (safety.blocked) { LOG_MODULE_WARN(...); return NIMCP_ERROR_SAFETY_BLOCKED; }

// STEP 4: Encode phonologically
phonological_trace_t trace;
phonological_wm_encode(pwm, "adjacent", &trace);

// STEP 5: SNN spike encoding
snn_encode_temporal(snn_bridge->phoneme_pop, trace.phoneme_times, spike_train);
snn_population_step(snn_bridge->phoneme_pop, dt);

// STEP 6: Create eligibility trace for learning
stdp_create_eligibility_trace(stdp_ctx, pre_neuron, post_neuron);

// STEP 7: Structural plasticity - NASCENT spine
structural_plasticity_form_spine(struct_ctx, synapse_id, SPINE_STATE_NASCENT);

// STEP 8: Create engram (ENCODING state)
engram_id_t eid = engram_encode(engram_system, &word_engram, ENGRAM_TYPE_SEMANTIC);

// STEP 9: Register with KG
brain_kg_add_node(kg, "adjacent", BRAIN_KG_NODE_COGNITIVE, "New spatial word");

// STEP 10: Log the event
LOG_MODULE_INFO(LOG_MODULE_SPATIAL_LANG, "New word encoded: adjacent");

// STEP 11: Register heartbeat
heartbeat_pulse(linguistics_heartbeat);

// === LATER: Positive feedback received ===

// STEP 12: Training layer computes RPE
float rpe = training_compute_rpe(training, loss_delta);

// STEP 13: RPE → Dopamine
float dopamine = training_plasticity_rpe_to_dopamine(tpb, rpe);

// STEP 14: Dopamine modulates BG
basal_ganglia_set_dopamine(bg, dopamine);

// STEP 15: STDP update with dopamine
float weight_delta = stdp_compute_update(stdp_ctx, eligibility, dopamine);
stdp_apply_update(stdp_ctx, synapse_id, weight_delta);

// STEP 16: Triplet STDP for sequence (if high frequency)
triplet_stdp_update(triplet_ctx, spike_times, &triplet_delta);

// STEP 17: R-STDP consolidation during dopamine burst
if (dopamine > 6.0f * baseline) {
    rstdp_consolidate(rstdp_ctx, eligibility_traces);
}

// STEP 18: BG training bridge update
bgtr_bridge_process_reward(bg_training, reward);

// STEP 19: Engram transitions LABILE → CONSOLIDATING
engram_update_state(engram_system, eid, ENGRAM_STATE_CONSOLIDATING);

// STEP 20: Structural plasticity - strengthen spine
structural_plasticity_strengthen_spine(struct_ctx, synapse_id);
// After repeated use: NASCENT → STABLE

// STEP 21: Health metrics update
linguistics_health_metrics_t metrics = calculate_metrics(ctx);
health_monitor_update(health, &metrics);
```

### 2.26.5 Integration Verification Checklist

**Each integration MUST be verified with these checks:**

| Integration | Verification Method |
|-------------|---------------------|
| Fuzzy Logic | `fuzzy_mf_evaluate()` returns valid [0,1] membership |
| Statistics/HMM | `hmm_viterbi_decode()` returns valid path |
| SNN | `snn_population_get_firing_rate()` > 0 after encoding |
| STDP | `stdp_get_weight()` changes after pre-post pairing |
| Triplet STDP | Weight change larger at high frequency |
| R-STDP | Weight change scales with dopamine level |
| BCM | Threshold slides based on activity history |
| Structural | Spine state progresses NASCENT→STABLE |
| Training | RPE computed, dopamine modulated |
| Engram | State progresses ENCODING→LABILE→CONSOLIDATED |
| Hypothalamus | Drive values affect plasticity rates |
| Omni WM | Predictions generated from language input |
| Basal Ganglia | Action selected, D1/D2 activations computed |
| BBB | Invalid input blocked with BBB_ACTION_BLOCK |
| Health | Heartbeat received, metrics updated |
| KG | Node queryable after registration |
| Logging | Log entry appears in output |
| Exception | Epitope generated, immune notified |
| Symbolic | Forward/backward chaining produces results |
| LGSS | Harmful content blocked |
| Cerebellum | Timing predictions generated, errors update model |
| Medulla | Arousal affects speech rate/precision |
| Perception | Phoneme stream parsed, prosody extracted |
| Theory of Mind | BDI state queried, perspective computed |
| World Model | Counterfactual states generated |
| ToM-WM Bridge | Empathy simulation aligns perspectives |

### 2.26.6 Required Integration Tests

**Each test verifies actual data flow, not just compilation:**

```c
// TEST: Fuzzy integration actually called
TEST(SpatialLanguage, FuzzyIntegration) {
    spatial_semantics_t sem;
    spatial_language_parse_preposition("near", &sem);
    EXPECT_NE(sem.distance_mf, nullptr);  // MF was created
    float membership = fuzzy_mf_evaluate(sem.distance_mf, 1.0f);
    EXPECT_GT(membership, 0.0f);  // Fuzzy actually evaluated
}

// TEST: SNN spikes actually generated
TEST(PhonologicalWM, SNNIntegration) {
    phonological_wm_encode(pwm, "test", &trace);
    float rate = snn_population_get_firing_rate(snn_bridge->phoneme_pop);
    EXPECT_GT(rate, 0.0f);  // Spikes were generated
}

// TEST: STDP weights actually change
TEST(LinguisticsPlasticity, STDPIntegration) {
    float w_before = stdp_get_weight(stdp_ctx, synapse_id);
    // Simulate pre-before-post timing
    stdp_record_pre_spike(stdp_ctx, synapse_id, t);
    stdp_record_post_spike(stdp_ctx, synapse_id, t + 10);
    stdp_update(stdp_ctx);
    float w_after = stdp_get_weight(stdp_ctx, synapse_id);
    EXPECT_GT(w_after, w_before);  // LTP occurred
}

// TEST: Basal ganglia actually selects
TEST(LinguisticsBG, ActionSelection) {
    float activations[] = {0.8f, 0.3f, 0.5f};  // 3 word candidates
    uint32_t selected = basal_ganglia_select_action(bg, activations);
    EXPECT_EQ(selected, 0);  // Highest activation selected
    float d1 = basal_ganglia_get_direct_activation(bg, 0);
    EXPECT_GT(d1, 0.5f);  // D1 pathway activated
}

// TEST: Engram state actually progresses
TEST(LinguisticsEngram, Consolidation) {
    engram_id_t eid = engram_encode(engram, &word_engram, ENGRAM_TYPE_SEMANTIC);
    EXPECT_EQ(engram_get_state(engram, eid), ENGRAM_STATE_ENCODING);
    // Simulate 6 hours
    engram_simulate_time(engram, 6 * 3600 * 1000);
    EXPECT_EQ(engram_get_state(engram, eid), ENGRAM_STATE_CONSOLIDATED);
}

// TEST: Exception actually reaches immune system
TEST(LinguisticsException, ImmuneIntegration) {
    // Register immune listener
    immune_register_callback(immune, test_callback, &received_epitope);
    // Trigger exception
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_INPUT, "Test exception");
    // Verify immune received it
    EXPECT_EQ(received_epitope.error_code, NIMCP_ERROR_INVALID_INPUT);
}

// TEST: Health heartbeat actually registered
TEST(LinguisticsHealth, HeartbeatIntegration) {
    uint32_t beats_before = health_get_heartbeat_count(health, linguistics_module_id);
    linguistics_heartbeat_callback(ctx);
    uint32_t beats_after = health_get_heartbeat_count(health, linguistics_module_id);
    EXPECT_EQ(beats_after, beats_before + 1);
}
```

---

## 3. Module Specifications

### 3.1 Spatial Language Module (Angular Gyrus)

**File:** `include/cognitive/parietal/nimcp_parietal_spatial_language.h`

**Core Types:**
```c
typedef enum {
    SPATIAL_PREP_NEAR, SPATIAL_PREP_FAR,
    SPATIAL_PREP_LEFT, SPATIAL_PREP_RIGHT,
    SPATIAL_PREP_ABOVE, SPATIAL_PREP_BELOW,
    SPATIAL_PREP_IN, SPATIAL_PREP_ON,
    SPATIAL_PREP_BETWEEN, SPATIAL_PREP_THROUGH,
    // ... ~30 total
} spatial_preposition_t;

typedef enum {
    REF_FRAME_EGOCENTRIC,      // Body-centered
    REF_FRAME_ALLOCENTRIC,     // World-centered
    REF_FRAME_INTRINSIC,       // Object-centered
    REF_FRAME_RELATIVE         // Speaker-relative
} reference_frame_t;

typedef struct {
    spatial_preposition_t preposition;
    reference_frame_t frame;
    fuzzy_mf_t* distance_mf;      // Fuzzy distance semantics
    fuzzy_mf_t* angle_mf;         // Fuzzy angle semantics
    float frame_confidence;       // Bayesian posterior
} spatial_semantics_t;
```

**Key Functions:**
- `spatial_language_parse_preposition()` - Parse spatial word → semantics
- `spatial_language_select_frame()` - Bayesian reference frame selection
- `spatial_language_apply_hedge()` - "very near" → concentrated MF
- `spatial_language_ground_metaphor()` - "Life is a journey" → spatial mapping
- `spatial_language_predict_outcome()` - World model spatial prediction

**Fuzzy Integration:**
- Each preposition has associated membership functions
- Hedges modify semantics: VERY, SOMEWHAT, EXTREMELY
- Defuzzification for precise spatial coordinates

### 3.2 Numerical Language Module (Intraparietal Sulcus)

**File:** `include/cognitive/parietal/nimcp_parietal_numerical_language.h`

**Core Types:**
```c
typedef enum {
    NUM_WORD_CARDINAL,    // one, two, three
    NUM_WORD_ORDINAL,     // first, second, third
    NUM_WORD_MULTIPLIER,  // double, triple, quadruple
    NUM_WORD_FRACTION,    // half, third, quarter
    NUM_WORD_APPROXIMATE  // few, several, many, most
} number_word_type_t;

typedef enum {
    QUANTIFIER_UNIVERSAL,  // all, every, each
    QUANTIFIER_EXISTENTIAL,// some, a, an
    QUANTIFIER_NEGATIVE,   // no, none, neither
    QUANTIFIER_PROPORTIONAL// most, few, many, half
} linguistic_quantifier_t;

typedef struct {
    number_word_type_t type;
    float magnitude;              // Exact or approximate
    float uncertainty;            // Weber-Fechner derived
    fuzzy_mf_t* quantity_mf;      // For approximate quantities
    bool is_approximate;
} numerical_semantics_t;
```

**Key Functions:**
- `numerical_language_parse_word()` - "twenty-three" → 23
- `numerical_language_generate_word()` - 23 → "twenty-three"
- `numerical_language_parse_ordinal()` - "third" → position 3
- `numerical_language_quantifier_to_fuzzy()` - "most" → fuzzy MF
- `numerical_language_hmm_predict()` - Predict next number word

**HMM Integration:**
- Model number word sequences: P("one" | "twenty-")
- Viterbi decoding for spoken number recognition
- Forward-backward for sequence likelihood

**Weber-Fechner Integration:**
- Approximate quantities inherit number sense uncertainty
- "about twenty" → magnitude=20, uncertainty=3 (Weber 0.15)

### 3.3 Phonological Working Memory Module (Supramarginal Gyrus)

**File:** `include/cognitive/parietal/nimcp_parietal_phonological_wm.h`

**Core Types:**
```c
typedef struct {
    uint32_t phoneme_ids[64];     // IPA phoneme sequence
    float durations[64];          // Temporal pattern
    uint32_t length;
    float decay_rate;             // ~2 second trace decay
} phonological_trace_t;

typedef struct {
    phonological_trace_t buffer[7];  // Miller's 7±2
    uint32_t count;
    float rehearsal_rate;            // Subvocal loop speed
    bool is_rehearsing;
} phonological_loop_t;

typedef struct {
    float similarity_matrix[64][64]; // Phoneme confusability
    float word_length_effect;        // Longer = harder
    float phonological_similarity_effect;
} phonological_constraints_t;
```

**Key Functions:**
- `phonological_wm_encode()` - Word → phonological trace
- `phonological_wm_rehearse()` - Subvocal loop maintenance
- `phonological_wm_decay()` - Time-based forgetting
- `phonological_wm_similarity()` - Information-theoretic distance
- `phonological_wm_retrieve()` - Pattern completion recall

**Information Theory Integration:**
- Phoneme similarity via mutual information
- Entropy for phonological complexity
- KL divergence for accent differences

**Theta-Gamma Integration:**
- Theta phase (0-90°): Encoding new phonological patterns
- Theta phase (180-270°): Retrieval/rehearsal
- Gamma bursts: Phoneme feature binding

---

## 4. Integration Bridges

### 4.0 Mesh Participation Pattern (All Bridges)

**IMPORTANT:** All integration bridges in this section are **mesh participants**, not standalone modules. Each bridge implements the `linguistics_mesh_handler_t` interface defined in Section 1.5.

**Mesh Participant Registration Pattern:**
```c
// Each bridge registers with the mesh coordinator during initialization
int <bridge>_register_mesh(linguistics_mesh_coordinator_t* mesh) {
    linguistics_mesh_handler_t handler = {
        .process = <bridge>_mesh_process,       // Produce local belief
        .update = <bridge>_mesh_update,         // FEP belief update
        .get_precision = <bridge>_get_precision, // Current confidence
        .ctx = bridge_ctx
    };

    return linguistics_mesh_register_participant(
        mesh,
        <BRIDGE_MODULE_ID>,
        "<bridge_name>",
        handler);
}
```

**Mesh Communication Pattern:**
```c
// Instead of direct calls:
// OLD (hub-and-spoke): result = fuzzy_mf_evaluate(mf, value);

// NEW (mesh): Bridge contributes belief with precision
int fuzzy_bridge_mesh_process(void* ctx,
                               const linguistics_request_t* request,
                               linguistics_belief_t* belief) {
    // Produce local belief
    belief->certainty = fuzzy_mf_evaluate(ctx->mf, request->input_value);
    belief->precision = 1.0f / ctx->prediction_error_variance;
    encode_belief_vector(belief);
    return 0;
}

// Bridge updates belief based on neighbors (FEP)
int fuzzy_bridge_mesh_update(void* ctx,
                              const linguistics_belief_t* neighbors,
                              uint32_t count,
                              linguistics_belief_t* updated) {
    // Precision-weighted belief update
    for (uint32_t i = 0; i < count; i++) {
        float error = neighbors[i].certainty - updated->certainty;
        float weight = neighbors[i].precision * ctx->credibility[neighbors[i].source_id];
        updated->certainty += ctx->learning_rate * weight * error;
    }
    return 0;
}
```

**Precision Calculation per Bridge:**
| Bridge | Precision Source | Typical Range |
|--------|------------------|---------------|
| Fuzzy | MF sharpness / output entropy | 0.6-0.95 |
| SNN | Population agreement / firing rate stability | 0.7-0.95 |
| Engram | Memory strength / consolidation state | 0.5-0.9 |
| Training | Recent accuracy / loss stability | 0.4-0.9 |
| ToM | Listener model confidence | 0.5-0.85 |
| World Model | Prediction error variance | 0.6-0.9 |
| Perception | SNR / detection confidence | 0.3-0.95 |
| BG | D1/D2 competition margin | 0.5-0.9 |

### 4.1 Engram Integration Bridges

| Bridge | Purpose |
|--------|---------|
| `nimcp_spatial_language_engram_bridge.h` | Spatial word→frame mappings as SEMANTIC engrams |
| `nimcp_numerical_language_engram_bridge.h` | Number word sequences as PROCEDURAL engrams |
| `nimcp_phonological_wm_engram_bridge.h` | Phoneme patterns with IEG tagging |

**Key Integration Points:**
- Encode linguistic patterns as engrams during learning
- Pattern completion for partial cue recall
- Sleep consolidation for vocabulary stabilization
- Reconsolidation window for pronunciation updates
- Emotional tagging for salient words

### 4.2 Hypothalamus Integration Bridge

**File:** `nimcp_parietal_linguistics_hypothalamus_bridge.h`

**Drive Modulation:**
| Drive | Effect on Linguistics |
|-------|----------------------|
| CURIOSITY | ↑ plasticity for novel vocabulary |
| SOCIAL | ↑ priority for communication |
| COMPETENCE | ↑ precision for technical terms |
| SAFETY | ↑ attention to warning words |

**Circadian Effects:**
- Morning: Enhanced encoding (new vocabulary)
- Evening: Consolidation mode (rehearsal priority)
- Night: Minimal processing, sleep consolidation

### 4.3 Omni World Model Bridge

**File:** `nimcp_parietal_linguistics_omni_bridge.h`

**Bidirectional Integration:**
- **Linguistics → WM:** Spatial language updates world state predictions
- **WM → Linguistics:** Predictions inform reference frame selection

**Active Inference:**
- Policy selection for language production
- Expected free energy minimization for word choice

**Precision Weighting:**
- High precision: Unambiguous reference (use egocentric "left")
- Low precision: Ambiguous reference (query for clarification)

### 4.4 SNN Integration Bridge

**File:** `nimcp_parietal_linguistics_snn_bridge.h`

**SNN Population Organization for Linguistics:**
```c
typedef struct {
    snn_population_t* phoneme_population;    // 44 IPA phonemes
    snn_population_t* spatial_word_pop;      // ~30 prepositions
    snn_population_t* number_word_pop;       // Cardinal/ordinal words
    snn_population_t* reference_frame_pop;   // Ego/allo/intrinsic
} linguistics_snn_populations_t;
```

**Spike Encoding for Words:**
- **Phonemes**: Temporal coding (phoneme timing → spike timing)
- **Spatial words**: Population coding (reference frame → population pattern)
- **Numbers**: Rate coding (magnitude → firing rate, Weber-Fechner)

**SNN Training Modes:**
- `SNN_TRAIN_STDP`: Local Hebbian for word associations
- `SNN_TRAIN_R_STDP`: Reward-modulated for vocabulary acquisition
- `SNN_TRAIN_EPROP`: Bio-plausible backprop for sequence learning

### 4.5 Plasticity Integration Bridge

**File:** `nimcp_parietal_linguistics_plasticity_bridge.h`

**STDP Rules for Linguistics:**

| Rule | Application | Parameters |
|------|-------------|------------|
| **Pairwise STDP** | Word-meaning binding | τ=20ms, a+=0.005 |
| **Triplet STDP** | Sequence learning ("twenty-one") | τ_slow=101ms, A3+=0.0062 |
| **R-STDP** | Reinforced vocabulary | DA gain=100, burst=3x |
| **BCM** | Competitive word selection | θ sliding, winner-take-all |

**Homeostatic Integration:**
- Synaptic scaling: Maintain stable word representations
- Intrinsic plasticity: Balance frequent vs rare words
- Metaplasticity: Critical periods for L2 acquisition

**Structural Plasticity for Vocabulary:**
```
New word exposure     → NASCENT spine (thin, unstable)
Repeated practice     → STABLE spine (consolidated)
High-frequency use    → POTENTIATED spine (enlarged)
Disuse (L1 attrition) → PRUNING → ELIMINATED
```

**Eligibility Traces for Delayed Feedback:**
```c
typedef struct {
    float decay_lambda;           // 0.95 (half-life ~14ms)
    float learning_rate;          // 0.001
    bool burst_triggered_mode;    // 4-factor rule
    float burst_lr_multiplier;    // 3.0x during DA bursts
} linguistics_eligibility_config_t;
```

### 4.6 Math Utilities Integration Bridge

**File:** `nimcp_parietal_linguistics_math_bridge.h`

**FFT/Hilbert Integration for Phonological Processing:**
```c
typedef struct {
    fft_plan_t* phoneme_fft;          // For spectral decomposition
    hilbert_ctx_t* phase_extractor;    // For theta-gamma gating
    float theta_phase;                 // Current theta phase (0-2π)
    float gamma_amplitude;             // Gamma envelope
    bool encoding_phase;               // θ in [0, π/2] = encoding
} linguistics_spectral_ctx_t;
```

**Hyperbolic Embeddings for Semantic Hierarchies:**
```c
typedef struct {
    poincare_point_t* spatial_words;   // ~30 prepositions in H^5
    poincare_point_t* number_words;    // Ordinals/cardinals in H^5
    poincare_point_t* reference_frames; // Ego/allo/intrinsic in H^3
    uint32_t embedding_dim;            // Typically 5 (200x compression)
} linguistics_hyperbolic_ctx_t;

// Semantic distance via hyperbolic geometry
float linguistics_semantic_distance(
    const poincare_point_t* word_a,
    const poincare_point_t* word_b);

// Riemannian gradient descent for vocabulary learning
void linguistics_hyperbolic_sgd_step(
    poincare_point_t* word,
    const float* euclidean_grad,
    float learning_rate);
```

**MPS/Tensor Train for Word Embedding Compression:**
```c
typedef struct {
    mps_t* word_embeddings;           // Compressed vocabulary matrix
    uint32_t vocab_size;              // Number of words
    uint32_t embedding_dim;           // Original dimension
    uint32_t bond_dim;                // MPS bond dimension
    float compression_ratio;          // Achieved compression
} linguistics_mps_embeddings_t;

// 10-100x compressed word lookup
void linguistics_mps_lookup(
    const linguistics_mps_embeddings_t* embeddings,
    uint32_t word_id,
    float* output_vector);
```

**Positional Encoding for Word Sequences:**
```c
// RoPE for relative position in utterances
void linguistics_apply_rope(
    float* word_embedding,
    uint32_t position,
    uint32_t dim);

// ALiBi for long-range attention
float linguistics_alibi_bias(
    uint32_t head_idx,
    uint32_t query_pos,
    uint32_t key_pos);
```

### 4.7 Quantum Integration Bridge

**File:** `nimcp_parietal_linguistics_quantum_bridge.h`

**Quantum Walk for Semantic Spreading:**
```c
typedef struct {
    quantum_walk_ctx_t* semantic_walk;  // Walk on semantic graph
    uint32_t num_words;                  // Vocabulary size
    float* adjacency;                    // Semantic similarity matrix
    coin_operator_t coin;                // Hadamard/Grover/Fourier
} linguistics_quantum_semantic_t;

// O(√N) spreading activation
void linguistics_quantum_spread(
    linguistics_quantum_semantic_t* ctx,
    uint32_t source_word,
    uint32_t steps,
    float* activation_distribution);
```

**Quantum Annealing for Parsing:**
```c
typedef struct {
    quantum_annealer_t* annealer;
    float temperature;
    uint32_t max_iterations;
    cooling_schedule_t schedule;
} linguistics_quantum_parser_t;

// Find globally optimal parse via quantum tunneling
parse_tree_t* linguistics_quantum_parse(
    linguistics_quantum_parser_t* ctx,
    const uint32_t* word_ids,
    uint32_t num_words);
```

**Quantum MCTS for Language Generation:**
```c
typedef struct {
    quantum_mcts_t* mcts;
    float quantum_fraction;          // 0-1 hybrid ratio
    uint32_t qmc_shots;              // Monte Carlo samples
    float exploration_bonus;         // UCB exploration constant
} linguistics_quantum_generator_t;

// Generate optimal word sequence
uint32_t* linguistics_quantum_generate(
    linguistics_quantum_generator_t* ctx,
    const uint32_t* prompt_ids,
    uint32_t prompt_len,
    uint32_t max_length);
```

**Quantum Reasoning for Disambiguation:**
```c
typedef struct {
    quantum_reasoning_ctx_t* reasoner;
    ternary_belief_t* word_senses;    // TRUE/FALSE/UNKNOWN
    uint32_t max_variables;           // ≤16 for Grover, else DPLL
} linguistics_quantum_disambiguator_t;

// Grover search for satisfying interpretation
interpretation_t* linguistics_quantum_disambiguate(
    linguistics_quantum_disambiguator_t* ctx,
    const ambiguous_phrase_t* phrase);
```

**Quantum-Shannon for Information Metrics:**
```c
// Measure semantic channel capacity
float linguistics_quantum_shannon_capacity(
    const linguistics_quantum_semantic_t* ctx,
    uint32_t source_word,
    uint32_t target_word);

// Detect processing bottlenecks
bottleneck_t* linguistics_detect_bottlenecks(
    const linguistics_quantum_semantic_t* ctx,
    float capacity_threshold);
```

### 4.8 Training Layer Integration Bridge

**File:** `nimcp_parietal_linguistics_training_bridge.h`

**Training Phase Integration:**

| Phase | Linguistics Application |
|-------|------------------------|
| **T1 (Homeostatic)** | Stabilize phonological representations |
| **T2 (Dendritic)** | Branch-specific word-feature bindings |
| **T3 (Predictive)** | Next-word prediction, spatial anticipation |
| **T4 (Meta-learning)** | Rapid vocabulary domain adaptation |

**Loss Functions for Linguistics:**
```c
typedef enum {
    LING_LOSS_WORD_CLASSIFICATION,    // Cross-entropy for word ID
    LING_LOSS_SPATIAL_REGRESSION,     // MSE for spatial coordinates
    LING_LOSS_SEQUENCE_CTC,           // CTC for phoneme sequences
    LING_LOSS_CONTRASTIVE,            // Contrastive for word similarity
} linguistics_loss_type_t;
```

**Training-Plasticity Bridge Integration:**
```
Loss computed → RPE calculation → Dopamine modulation
                     ↓
        Region-specific plasticity routing
                     ↓
    ┌────────────────┼────────────────┐
    ↓                ↓                ↓
CORTICAL         HIPPOCAMPAL      PREFRONTAL
(STDP+ACh)       (BCM)            (Working memory)
```

**Curriculum for Vocabulary Learning:**
1. High-frequency spatial prepositions (in, on, at)
2. Common number words (1-10)
3. Complex prepositions (between, through)
4. Large numbers (hundreds, thousands)
5. Abstract spatial metaphors

**Meta-Learning for Vocabulary:**
```c
// Few-shot word learning
meta_config_t vocab_meta = {
    .algorithm = META_ALG_PROTOTYPICAL,
    .inner_lr = 0.01f,
    .inner_steps = 5,
    .outer_lr = 0.001f,
    .task_sampling = META_SAMPLE_CURRICULUM,
};
```

**Neuromodulator Effects on Linguistic Learning:**

| Neuromodulator | Effect | Linguistics Application |
|----------------|--------|------------------------|
| **Dopamine** | Learning rate ↑ | Reward correct word usage |
| **ACh** | Attention gating | Focus on salient vocabulary |
| **5-HT** | Patience/inhibition | Difficult word persistence |
| **NE** | Arousal/urgency | Emergency communication |

**Callback Integration:**
```c
// Detect vocabulary learning plateau
void linguistics_training_callback(tcb_event_type_t event,
                                    const tcb_metrics_t* metrics) {
    if (event == TCB_EVENT_LOSS_COMPUTED) {
        if (metrics->loss_delta < 0.001f && metrics->epoch > 10) {
            // Plateau detected - reduce LR or switch curriculum stage
            return TCB_ACTION_REDUCE_LR;
        }
    }
}
```

---

## 5. File Structure

```
include/cognitive/parietal/
├── linguistics/
│   ├── nimcp_parietal_linguistics_types.h       # Shared types
│   ├── nimcp_parietal_linguistics_mesh.h        # MESH COORDINATOR (central)
│   ├── nimcp_parietal_spatial_language.h        # Angular Gyrus
│   ├── nimcp_parietal_numerical_language.h      # IPS
│   ├── nimcp_parietal_phonological_wm.h         # Supramarginal
│   └── bridges/
│       ├── nimcp_spatial_language_engram_bridge.h
│       ├── nimcp_numerical_language_engram_bridge.h
│       ├── nimcp_phonological_wm_engram_bridge.h
│       ├── nimcp_parietal_linguistics_hypothalamus_bridge.h
│       ├── nimcp_parietal_linguistics_omni_bridge.h
│       ├── nimcp_parietal_linguistics_snn_bridge.h
│       ├── nimcp_parietal_linguistics_plasticity_bridge.h
│       ├── nimcp_parietal_linguistics_training_bridge.h
│       ├── nimcp_parietal_linguistics_math_bridge.h          # FFT, Hyperbolic, MPS
│       ├── nimcp_parietal_linguistics_quantum_bridge.h       # Walk, Annealing, MCTS
│       ├── nimcp_parietal_linguistics_health_bridge.h        # Health/Heartbeat
│       ├── nimcp_parietal_linguistics_kg_bridge.h            # KG Wiring
│       ├── nimcp_parietal_linguistics_logging_bridge.h       # Structured logging
│       ├── nimcp_parietal_linguistics_symbolic_bridge.h      # LGSS/Symbolic logic
│       ├── nimcp_parietal_linguistics_basal_ganglia_bridge.h # BG action selection
│       ├── nimcp_parietal_linguistics_cerebellum_bridge.h    # Speech timing/articulation
│       ├── nimcp_parietal_linguistics_medulla_bridge.h       # Arousal/breathing/protection
│       ├── nimcp_parietal_linguistics_perception_bridge.h    # Speech/visual perception
│       └── nimcp_parietal_linguistics_tom_wm_bridge.h        # Theory of Mind + World Model

src/cognitive/parietal/
├── linguistics/
│   ├── nimcp_parietal_linguistics_mesh.c        # MESH COORDINATOR (central)
│   ├── nimcp_parietal_spatial_language.c
│   ├── nimcp_parietal_numerical_language.c
│   ├── nimcp_parietal_phonological_wm.c
│   └── bridges/
│       ├── nimcp_spatial_language_engram_bridge.c
│       ├── nimcp_numerical_language_engram_bridge.c
│       ├── nimcp_phonological_wm_engram_bridge.c
│       ├── nimcp_parietal_linguistics_hypothalamus_bridge.c
│       ├── nimcp_parietal_linguistics_omni_bridge.c
│       ├── nimcp_parietal_linguistics_snn_bridge.c
│       ├── nimcp_parietal_linguistics_plasticity_bridge.c
│       ├── nimcp_parietal_linguistics_training_bridge.c
│       ├── nimcp_parietal_linguistics_math_bridge.c
│       ├── nimcp_parietal_linguistics_quantum_bridge.c
│       ├── nimcp_parietal_linguistics_health_bridge.c        # Health/Heartbeat
│       ├── nimcp_parietal_linguistics_kg_bridge.c            # KG Wiring
│       ├── nimcp_parietal_linguistics_logging_bridge.c       # Structured logging
│       ├── nimcp_parietal_linguistics_symbolic_bridge.c      # LGSS/Symbolic logic
│       ├── nimcp_parietal_linguistics_basal_ganglia_bridge.c # BG action selection
│       ├── nimcp_parietal_linguistics_cerebellum_bridge.c    # Speech timing/articulation
│       ├── nimcp_parietal_linguistics_medulla_bridge.c       # Arousal/breathing/protection
│       ├── nimcp_parietal_linguistics_perception_bridge.c    # Speech/visual perception
│       └── nimcp_parietal_linguistics_tom_wm_bridge.c        # Theory of Mind + World Model

test/unit/cognitive/parietal/linguistics/
├── test_parietal_linguistics_mesh.cpp           # MESH COORDINATOR tests
├── test_parietal_spatial_language.cpp
├── test_parietal_numerical_language.cpp
├── test_parietal_phonological_wm.cpp
└── bridges/
    ├── test_linguistics_snn_bridge.cpp
    ├── test_linguistics_plasticity_bridge.cpp
    ├── test_linguistics_training_bridge.cpp
    ├── test_linguistics_math_bridge.cpp
    ├── test_linguistics_quantum_bridge.cpp
    ├── test_linguistics_health_bridge.cpp                    # Health/Heartbeat
    ├── test_linguistics_kg_bridge.cpp                        # KG Wiring
    ├── test_linguistics_logging_bridge.cpp                   # Logging
    ├── test_linguistics_symbolic_bridge.cpp                  # LGSS/Symbolic
    ├── test_linguistics_basal_ganglia_bridge.cpp             # BG action selection
    ├── test_linguistics_cerebellum_bridge.cpp                # Speech timing
    ├── test_linguistics_medulla_bridge.cpp                   # Arousal/breathing
    ├── test_linguistics_perception_bridge.cpp                # Speech/visual perception
    └── test_linguistics_tom_wm_bridge.cpp                    # Theory of Mind + World Model

test/integration/cognitive/parietal/linguistics/
├── test_linguistics_mesh_integration.cpp        # MESH COORDINATOR integration
├── test_spatial_language_integration.cpp
├── test_numerical_language_integration.cpp
├── test_phonological_wm_integration.cpp
├── test_linguistics_snn_plasticity_integration.cpp
├── test_linguistics_training_integration.cpp
├── test_linguistics_math_integration.cpp
├── test_linguistics_quantum_integration.cpp
├── test_linguistics_health_integration.cpp                   # Health/Resilience
├── test_linguistics_kg_integration.cpp                       # KG Wiring
├── test_linguistics_symbolic_integration.cpp                 # LGSS/Symbolic logic
├── test_linguistics_basal_ganglia_integration.cpp            # BG action selection
├── test_linguistics_cerebellum_integration.cpp               # Speech timing/articulation
├── test_linguistics_medulla_integration.cpp                  # Arousal/breathing/protection
├── test_linguistics_perception_integration.cpp               # Speech/visual perception
└── test_linguistics_tom_wm_integration.cpp                   # Theory of Mind + World Model
```

---

## 6. Implementation Phases

### Phase 1: Foundation (Types, Core Modules & Infrastructure)
1. Create `nimcp_parietal_linguistics_types.h` - shared enums, constants
2. Implement `nimcp_parietal_spatial_language.h/.c` - basic spatial prepositions
3. Implement `nimcp_parietal_numerical_language.h/.c` - basic number words
4. Implement `nimcp_parietal_phonological_wm.h/.c` - phonological loop
5. **Exception Handling Pattern** - All functions use `NIMCP_CHECK_THROW_IMMUNE()` for validation
6. **Logging Integration** - Module-specific logging with `LOG_MODULE_*` macros
7. Unit tests for each module (including exception path tests)

### Phase 2: Fuzzy & Statistics Integration
1. Integrate fuzzy MFs for spatial prepositions
2. Integrate HMM for number word sequences
3. Integrate information theory for phonological similarity
4. Integrate Bayesian inference for reference frame selection
5. Integration tests

### Phase 3: SNN Integration
1. Create SNN populations for linguistics (phonemes, spatial words, numbers)
2. Implement spike encoding/decoding for linguistic features
3. Configure SNN topologies (feedforward for sequences, recurrent for loop)
4. Implement `nimcp_parietal_linguistics_snn_bridge.h/.c`
5. Test spike-based linguistic representations

### Phase 4: STDP & Plasticity Integration
1. Configure pairwise STDP for word-meaning associations
2. Implement triplet STDP for sequence learning ("twenty-one")
3. Implement R-STDP for reward-modulated vocabulary acquisition
4. Configure BCM for competitive word selection
5. Implement structural plasticity for vocabulary lifecycle:
   - NASCENT: New word exposure
   - STABLE: Consolidated vocabulary
   - POTENTIATED: High-frequency words
   - PRUNING/ELIMINATED: Disused words
6. Implement homeostatic mechanisms for vocabulary balance
7. Implement `nimcp_parietal_linguistics_plasticity_bridge.h/.c`
8. Test plasticity-driven learning

### Phase 5: Training Layer Integration
1. Configure training phases (T1-T4) for linguistics
2. Implement Loss-RPE-Dopamine pipeline for vocabulary learning
3. Configure region-specific plasticity routing (cortical, hippocampal)
4. Implement curriculum learning for vocabulary progression:
   - Stage 1: High-frequency words
   - Stage 2: Common number words
   - Stage 3: Complex prepositions
   - Stage 4: Large numbers
   - Stage 5: Abstract metaphors
5. Implement meta-learning for few-shot vocabulary acquisition
6. Configure training callbacks for plateau detection
7. Implement `nimcp_parietal_linguistics_training_bridge.h/.c`
8. Test training-driven vocabulary acquisition

### Phase 6: Math Utilities Integration
1. Integrate FFT/Hilbert for phonological spectral analysis
   - Theta-gamma coupling detection
   - Phase-gated encoding/retrieval
2. Implement hyperbolic embeddings for semantic hierarchies
   - Poincaré ball representation (5D)
   - Riemannian SGD for vocabulary learning
   - 200x compression for spatial/number word embeddings
3. Implement MPS/Tensor Train compression for vocabulary
   - 10-100x compression for word embeddings
   - Fast compressed lookup
4. Integrate positional encoding (RoPE, ALiBi)
   - Word order representation
   - Long-range attention support
5. Implement `nimcp_parietal_linguistics_math_bridge.h/.c`
6. Test spectral and hyperbolic operations

### Phase 7: Quantum Algorithm Integration
1. Implement quantum walk for semantic spreading
   - O(√N) speedup for word association
   - Coin operators (Hadamard, Grover)
2. Implement quantum annealing for parsing
   - Escape local minima via tunneling
   - Optimal reference frame selection
3. Implement quantum MCTS for language generation
   - Quantum-enhanced rollouts
   - Better word sequence sampling
4. Implement quantum reasoning for disambiguation
   - Grover search for word sense
   - Ternary logic for linguistic uncertainty
5. Integrate quantum-Shannon metrics
   - Semantic channel capacity
   - Bottleneck detection
6. Implement `nimcp_parietal_linguistics_quantum_bridge.h/.c`
7. Test quantum-enhanced linguistic processing

### Phase 8: Engram Memory Integration
1. Implement spatial language engram bridge
2. Implement numerical language engram bridge
3. Implement phonological WM engram bridge
4. Test consolidation (LABILE → CONSOLIDATED) over simulated time
5. Test sleep-dependent replay and consolidation
6. Test reconsolidation window for vocabulary updates
7. Integration tests

### Phase 9: Hypothalamus & Omni Integration
1. Implement hypothalamus drive modulation bridge
   - Curiosity drive → plasticity boost for novel words
   - Social drive → communication priority
2. Implement omni world model bridge
   - Spatial language → world state predictions
   - Precision weighting for ambiguity resolution
3. Test drive effects on learning rates
4. Test world model predictions for spatial language
5. Integration tests

### Phase 10: Parietal Orchestrator Integration
1. Add new request types to `parietal_request_type_t`:
   - `PARIETAL_SPATIAL_LANGUAGE_PARSE`
   - `PARIETAL_NUMERICAL_LANGUAGE_PARSE`
   - `PARIETAL_PHONOLOGICAL_WM_ENCODE`
   - `PARIETAL_LINGUISTICS_LEARN`
2. Implement request handlers in parietal orchestrator
3. Update existing language-parietal bridge
4. Full system integration tests
5. End-to-end vocabulary learning scenarios

### Phase 11: Infrastructure Integration
1. **Exception Handling & Immune System**
   - Implement `NIMCP_THROW_TO_IMMUNE()` in all error paths
   - Define linguistics-specific epitope patterns
   - Configure recovery actions (GC, ROLLBACK, QUARANTINE)
   - Test exception → antibody response flow

2. **BBB (Blood-Brain Barrier) Integration**
   - Implement input validation for all word/text inputs
   - UTF-8 validation, length limits, sanitization
   - Memory boundary protection for engram storage
   - Integrate with `bbb_validate_input()` API

3. **Health Monitoring & Heartbeat**
   - Register linguistics modules with health monitor
   - Implement `linguistics_heartbeat_callback()`
   - Configure watchdog timeout (500ms default)
   - Define recovery levels per module
   - Implement health bridge `nimcp_parietal_linguistics_health_bridge.h/.c`

4. **KG Wiring System**
   - Register all linguistics nodes in brain KG
   - Define edges between modules (PROJECTS_TO, EXCITES, DEPENDS_ON)
   - Wire to external systems (hypothalamus, omni, engram)
   - Implement KG bridge `nimcp_parietal_linguistics_kg_bridge.h/.c`

5. **Full Logging Integration**
   - Define module IDs (SPATIAL_LANG, NUMERICAL_LANG, etc.)
   - Implement structured logging for all major operations
   - JSON format for metrics collection
   - Performance/latency logging
   - Implement logging bridge `nimcp_parietal_linguistics_logging_bridge.h/.c`

6. **Symbolic Logic & LGSS Integration**
   - Define spatial reasoning rules (transitivity, symmetry)
   - Define numerical consistency rules
   - Integrate LGSS safety checks for harmful content
   - Forward/backward chaining for spatial inference
   - FEP bridge integration for prediction errors
   - Implement symbolic bridge `nimcp_parietal_linguistics_symbolic_bridge.h/.c`

7. **Integration Testing**
   - Test exception → immune → recovery flow
   - Test BBB validation blocking malformed input
   - Test health degradation → recovery escalation
   - Test KG query for linguistics modules
   - Test LGSS safety blocking
   - End-to-end resilience scenarios

### Phase 12: Basal Ganglia Integration
1. **Word Selection as Action Selection**
   - Map word candidates to `action_candidate_t` structures
   - Configure D1 (GO) and D2 (NO-GO) pathway weights
   - Implement word competition via `basal_ganglia_select_action()`
   - Handle multi-word selection for phrase production

2. **Procedural Grammar Learning**
   - Define grammar rules as habit sequences
   - Register rules via `basal_ganglia_register_habit()`
   - Track habit strength progression (→0.7 threshold)
   - Implement DLS-specific learning for grammar automaticity

3. **Reward-Based Vocabulary Learning**
   - Connect training layer RPE to BG dopamine
   - Implement three-factor learning (Pre × Post × DA)
   - Configure D1/D2 asymmetric learning rates
   - Track eligibility traces for temporal credit assignment

4. **FEP Bridge for Word Selection**
   - Evaluate words via expected free energy
   - Configure explore/exploit balance for vocabulary
   - Integrate precision weighting for word uncertainty

5. **Executive Bridge for Grammar**
   - Register grammatical correctness as PFC goal
   - Implement inhibition of incorrect word forms
   - Handle goal-habit conflicts (formulaic vs precise)
   - Support task switching costs for syntax changes

6. **Operating Mode Management**
   - Track GOAL_DIRECTED vs HABITUAL speech mode
   - Implement mode transitions based on cognitive load
   - Support EXPLORATORY mode for creative language
   - SUPPRESSED mode for speech hesitation/correction

7. **Implement Bridge**
   - Create `nimcp_parietal_linguistics_basal_ganglia_bridge.h/.c`
   - Wire to existing BG FEP/Executive/Training bridges
   - Integrate with parietal linguistics modules

8. **Testing**
   - Test word selection via D1/D2 competition
   - Test grammar habit formation and automaticity
   - Test reward-based vocabulary strengthening
   - Test mode transitions (goal → habit → explore)
   - Test conflict resolution for ambiguous word choices

### Phase 13: Cerebellum & Medulla Integration
1. **Leverage Existing Language-Cerebellum Bridge**
   - Use `language_cerebellum_set_speech_rate()` for tempo control
   - Use `language_cerebellum_get_timing_pattern()` for rhythm types
   - Use `language_cerebellum_predict_duration()` for phoneme timing
   - Integrate error-based learning via `language_cerebellum_update_model()`

2. **Speech Timing Integration**
   - Connect phonological WM trace durations to cerebellum predictions
   - Configure rhythm type per language (stress-timed for English)
   - Implement syllable-to-timing mapping
   - Test timing accuracy (target: <10ms error)

3. **Articulatory Motor Commands**
   - Map 8-DOF nuclei output to articulatory state
   - Implement phoneme → articulatory command mapping
   - Connect to cerebellum forward models for prediction
   - Test motor coordination precision

4. **Pronunciation Error Learning**
   - Generate climbing fiber signals from acoustic errors
   - Implement formant comparison for error magnitude
   - Configure LTD/LTP rates for articulation learning
   - Track automatization progression (NOVICE → EXPERT)

5. **Procedural Memory for Speech Sequences**
   - Create motor sequences for common words/phrases
   - Integrate with pr_cerebellum_bridge
   - Track automatization level per word
   - Test sequence execution timing

6. **Medulla Arousal Integration**
   - Query arousal level and map to speech effects
   - Implement speech rate multiplier (0.7x-1.3x)
   - Implement motor precision curve (inverted U)
   - Test vocal tremor at arousal extremes

7. **Breathing Coordination**
   - Query respiratory state from medulla
   - Coordinate utterance boundaries with breath phases
   - Insert breath pauses at natural boundaries
   - Test breath-speech synchronization

8. **Protection Level Speech Gating**
   - Query protection level and gate speech appropriately
   - Implement speech mode transitions (FULL → ESSENTIAL → ALARM → DISABLED)
   - Test emergency speech suppression

9. **Circadian Speech Effects**
   - Query circadian phase and apply effects
   - Modulate clarity, fluency, error rate, learning rate
   - Test time-of-day performance variation

10. **Implement Bridges**
    - Create `nimcp_parietal_linguistics_cerebellum_bridge.h/.c`
    - Create `nimcp_parietal_linguistics_medulla_bridge.h/.c`
    - Wire to existing cerebellum and medulla modules

11. **Testing**
    - Test timing prediction accuracy (<10ms error)
    - Test error-based pronunciation learning (weight changes)
    - Test automatization progression over repetitions
    - Test arousal effects on speech rate/precision
    - Test protection level speech gating
    - Test breath-speech coordination
    - Test circadian effects on speech quality

### Phase 14: Perception Layer Integration
1. **Phoneme Stream Integration**
   - Connect to `speech_cortex_detect_phonemes()` for phoneme input
   - Parse phoneme stream for spatial prepositions
   - Parse phoneme stream for number words
   - Handle 44-phoneme English inventory

2. **Formant-Based Vowel Processing**
   - Use `speech_cortex_extract_formants()` for F1/F2 values
   - Classify vowels via `speech_cortex_classify_vowel()`
   - Integrate vowel quality with word recognition

3. **Prosody Integration**
   - Use `speech_cortex_extract_prosody()` for pitch/stress/rhythm
   - Apply stress patterns to word boundary detection
   - Use pitch contour for sentence type (question vs statement)
   - Integrate rhythm with spatial/numerical parsing

4. **Audiovisual Speech Integration**
   - Connect to lip reading system for viseme input
   - Implement McGurk-style fusion via `lip_reading_integrate_audiovisual()`
   - Handle SNR-dependent weighting (audio vs visual)
   - Support silent speech recognition for noisy conditions

5. **Speaker Adaptation**
   - Register speakers via `lip_reading_register_speaker()`
   - Update profiles with `lip_reading_update_speaker_profile()`
   - Adapt phoneme recognition to speaker accent/style

6. **Visual Word Form Recognition**
   - Connect to visual cortex for grapheme features
   - Support reading pathway (grapheme → phoneme conversion)
   - Integrate with angular gyrus spatial language

7. **Top-Down Predictions**
   - Set phoneme predictions from linguistic context
   - Query prediction errors for learning
   - Modulate attention to relevant frequencies/regions

8. **Phonological Working Memory Integration**
   - Connect to existing speech cortex phonological buffer
   - Coordinate with parietal phonological WM module
   - Handle 7±2 item capacity with rehearsal

9. **Implement Bridge**
   - Create `nimcp_parietal_linguistics_perception_bridge.h/.c`
   - Wire to cochlea, audio cortex, speech cortex, lip reading
   - Support bidirectional data flow

10. **Testing**
    - Test phoneme stream parsing for spatial words
    - Test phoneme stream parsing for number words
    - Test audiovisual fusion at various SNR levels
    - Test silent speech recognition (visual-only)
    - Test prosody extraction and application
    - Test speaker adaptation over 300 frames
    - Test top-down prediction error computation

### Phase 15: Theory of Mind & World Model Integration
1. **Reference Frame Selection via Perspective-Taking**
   - Query ToM for listener's spatial perspective
   - Use `omni_wm_tom_bridge_empathy_simulation()` for perspective alignment
   - Select egocentric vs allocentric frame based on lateral distance
   - Track perspective alignment score [0-1]

2. **Pronoun Resolution ("I"/"you" tracking)**
   - Map pronouns to BDI agent states via `tom_get_bdi_state()`
   - Track speaker vs listener mental states
   - Resolve deictic pronouns to correct agent
   - Handle perspective shifts during dialogue

3. **Spatial Deixis ("here"/"there"/"left"/"right")**
   - Use `omni_wm_tom_bridge_get_lateral_dynamics()` for perspective gap
   - Compute reference frame transformation when gap > 0.5
   - Adjust spatial preposition semantics based on whose perspective
   - Support both egocentric and allocentric interpretations

4. **Communicative Intent Detection**
   - Compare stated vs ToM-inferred intention
   - Use `tom_detect_false_belief()` for reality gap detection
   - Flag mismatches for clarification
   - Log intent inconsistencies

5. **Audience Design (Adapting Language to Listener)**
   - Model listener's vocabulary via `tom_get_agent_belief()`
   - Adapt word choice based on listener knowledge
   - Query listener's spatial expertise trait
   - Use simpler reference frames for low-expertise listeners

6. **Common Ground Tracking**
   - Compute shared vs private spatial knowledge
   - Track what's already established in discourse
   - Only mention new information
   - Update common ground after each utterance

7. **Counterfactual Spatial Reasoning**
   - Use `omni_wm_tom_bridge_counterfactual_belief()` for hypotheticals
   - Compute spatial relations in counterfactual states
   - Support "what if" spatial scenarios
   - Handle hypothetical perspective changes

8. **Mental State Prediction for Instructions**
   - Use `omni_wm_tom_bridge_predict_mental_state()` for prediction
   - Verify listener will understand each instruction step
   - Add clarifications where confidence < 0.7
   - Track prediction confidence per step

9. **Implement Bridge**
   - Create `nimcp_parietal_linguistics_tom_wm_bridge.h/.c`
   - Wire to ToM, world model, and ToM-WM bridge
   - Support bidirectional data flow
   - Integrate with spatial language reference frame selection

10. **Testing**
    - Test reference frame selection via perspective-taking
    - Test pronoun resolution with multiple agents
    - Test spatial deixis with perspective gaps
    - Test audience design with varying listener expertise
    - Test common ground tracking over dialogue
    - Test counterfactual spatial reasoning
    - Test mental state prediction for instruction sequences
    - Test intent mismatch detection

### Phase 16: Mesh Coordinator Integration

**This is the architectural centerpiece that transforms hub-and-spoke into mesh.**

1. **Core Mesh Coordinator Implementation**
   - Create `nimcp_parietal_linguistics_mesh.h/.c`
   - Implement `linguistics_mesh_coordinator_t` struct from Section 1.5
   - Wire to gossip_beliefs, fep_orchestrator, collective_workspace, convergence_ctx
   - Implement `linguistics_mesh_create()`, `linguistics_mesh_destroy()`
   - Implement participant registration API

2. **Mesh Request/Response API**
   - Implement `linguistics_mesh_request()` with timeout
   - Implement request broadcast via `bio_router_broadcast()`
   - Implement belief collection and aggregation
   - Implement response generation from converged beliefs

3. **FEP Convergence Engine**
   - Implement FEP belief update loop (see Section 1.5)
   - Configure learning rate (0.1), convergence threshold (0.001)
   - Implement precision-weighted update rule: `μ' = μ - lr * Π * ε`
   - Wire to `fep_orchestrator_step()` for free energy minimization

4. **Gossip Belief Propagation**
   - Configure gossip network for linguistics participants
   - Set gossip probability (0.3), credibility weights
   - Implement `gossip_introduce_belief()` for each participant
   - Implement `gossip_propagate_round()` in convergence loop
   - Handle Byzantine fault tolerance (≤1/3 faulty)

5. **Convergence Detection**
   - Wire to `inner_dialogue_convergence_analyze()`
   - Configure agreement threshold (0.75)
   - Detect deadlock (oscillating disagreement)
   - Detect rumination (repetitive patterns)
   - Implement voting fallback via `swarm_consensus_propose()`

6. **CRDT Collective Workspace**
   - Configure workspace for Top-K linguistic interpretations
   - Implement vector clock causality tracking
   - Implement conflict resolution via salience (precision × confidence)
   - Wire to `collective_workspace_merge()`

7. **Participant Registration for All Bridges**
   - Update each bridge to implement `linguistics_mesh_handler_t`:
     - Engram bridges (Spatial, Numerical, Phonological)
     - Hypothalamus, Omni, SNN, Plasticity bridges
     - Training, Math, Quantum bridges
     - Health, KG, Logging, Symbolic bridges
     - Basal Ganglia, Cerebellum, Medulla bridges
     - Perception, ToM+WM bridges
   - Each bridge provides: `process()`, `update()`, `get_precision()`

8. **Neuromodulator Channel Routing**
   - Configure channel selection per message type
   - Dopamine (~2s): Reward signals
   - Acetylcholine (~50ms): Fast attention
   - Norepinephrine (~3s): Priority alerts
   - Serotonin (~10s): Slow context shifts

9. **KG Integration for Module Discovery**
   - Use `brain_kg_query_neighbors()` to find mesh participants
   - Dynamically discover available modules
   - Handle module registration/deregistration

10. **Testing**
    - Test mesh coordinator creation/destruction
    - Test participant registration (all bridges)
    - Test request broadcast reaches all participants
    - Test belief generation per participant
    - Test gossip propagation (beliefs spread)
    - Test FEP convergence (agreement_score increases)
    - Test convergence detection (> 0.75 threshold)
    - Test deadlock detection and voting fallback
    - Test precision weighting (high precision = more influence)
    - Test CRDT workspace merge correctness
    - Test neuromodulator channel routing
    - Test timeout handling
    - Test Byzantine fault tolerance (inject faulty beliefs)
    - Test end-to-end mesh request → converged response
    - Performance: <100ms for 12-participant mesh to converge

---

## 7. Testing Strategy

### Unit Tests
- Each function tested in isolation
- Mock dependencies (fuzzy, stats, engram)
- Test edge cases (empty input, max values)

### Integration Tests
- Module interactions (spatial + fuzzy)
- Bridge communications (engram encoding/recall)
- Multi-system flows (parse → encode → consolidate → recall)

### End-to-End Tests
- Full linguistic processing pipelines
- Learning scenarios (novel vocabulary)
- Consolidation scenarios (sleep replay)

### Performance Tests
- Latency benchmarks (real-time language processing)
- Memory usage (engram capacity)
- GPU acceleration (where applicable)

---

## 8. End-to-End Integration Example

### Scenario: Learning a New Spatial Word "adjacent"

**Step 1: Initial Encoding (T1 Phase)**
```
Input: "The cup is adjacent to the book"

1. Phonological WM encodes /əˈdʒeɪsənt/ as spike train
2. SNN phoneme population activates
3. NASCENT spine forms for "adjacent"→spatial_relation
4. Engram created (ENCODING state)
```

**Step 2: Semantic Binding (T2 Phase)**
```
1. Triplet STDP strengthens "adjacent" → NEAR reference frame
2. Fuzzy MF assigned: Gaussian(μ=0, σ=0.5m)
3. BCM competition: "adjacent" vs "beside" vs "next to"
4. Winner-take-all selects appropriate contexts
```

**Step 3: Reinforcement (T3 Phase)**
```
1. Correct usage detected → positive RPE
2. Dopamine burst → R-STDP consolidation
3. Eligibility trace captures timing
4. Loss decreases → training callback continues
```

**Step 4: Consolidation (Sleep/T4)**
```
1. Engram transitions: LABILE → CONSOLIDATING
2. Sleep replay at 15x speed
3. NASCENT spine → STABLE spine
4. Systems consolidation: hippocampus → cortex
5. Curriculum advances to next word
```

**Step 5: Retrieval & Reconsolidation**
```
1. Partial cue "adj..." triggers pattern completion
2. Engram recalled (RECONSOLIDATING for 6 hours)
3. Pronunciation can be updated
4. World model predicts spatial configuration
```

### Data Flow Diagram (MESH ARCHITECTURE)

**Note:** Unlike hub-and-spoke, all bridges communicate peer-to-peer through the mesh coordinator, converging via FEP + gossip protocol.

```
┌────────────────────────────────────────────────────────────────────────────────────┐
│                         PARIETAL LINGUISTICS MESH NETWORK                           │
├────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌───────────────────────────────────────────────────────────────────────────────┐ │
│  │                          MESH COORDINATOR                                      │ │
│  │   • bio_router_broadcast()  • gossip_beliefs  • fep_orchestrator              │ │
│  │   • collective_workspace    • convergence_detector  • swarm_consensus         │ │
│  └───────────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                              │
│            Request broadcast ────────┼──────── Converged response                   │
│                                      ▼                                              │
│  ┌───────────────────────────────────────────────────────────────────────────────┐ │
│  │                     GOSSIP BELIEF LAYER (Peer-to-Peer)                         │ │
│  │                                                                                │ │
│  │    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐        │ │
│  │    │ Spatial│◄──►│Numer.  │◄──►│Phonol. │◄──►│  SNN   │◄──►│Plastic.│        │ │
│  │    │Π=0.85 │    │Π=0.8  │    │Π=0.75 │    │Π=0.9  │    │Π=0.6  │        │ │
│  │    └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘        │ │
│  │         │◄────────────┼────────────►│◄────────────┼────────────►│            │ │
│  │         ▼             ▼             ▼             ▼             ▼            │ │
│  │    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐        │ │
│  │    │Training│◄──►│ Engram │◄──►│  ToM   │◄──►│World M │◄──►│ Fuzzy  │        │ │
│  │    │Π=0.7  │    │Π=0.75 │    │Π=0.7  │    │Π=0.85 │    │Π=0.8  │        │ │
│  │    └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘    └────┬───┘        │ │
│  │         │◄────────────┼────────────►│◄────────────┼────────────►│            │ │
│  │         ▼             ▼             ▼             ▼             ▼            │ │
│  │    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐        │ │
│  │    │  BG    │◄──►│Cerebell│◄──►│Medulla │◄──►│Percept │◄──►│Symbolic│        │ │
│  │    │Π=0.82 │    │Π=0.7  │    │Π=0.5  │    │Π=0.88 │    │Π=0.95 │        │ │
│  │    └────────┘    └────────┘    └────────┘    └────────┘    └────────┘        │ │
│  │                                                                                │ │
│  │    Each bridge: process() → belief + precision                                │ │
│  │                  update()  → FEP: μ' = μ - lr × Π × ε                         │ │
│  └───────────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                              │
│                                      ▼                                              │
│  ┌───────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CONVERGENCE ENGINE                                          │ │
│  │                                                                                │ │
│  │   1. Broadcast request → all participants                                      │ │
│  │   2. Each produces belief + precision                                          │ │
│  │   3. Gossip propagate (peer-to-peer, prob=0.3)                                │ │
│  │   4. FEP update: precision-weighted belief adjustment                          │ │
│  │   5. Check: agreement_score ≥ 0.75 → converged                                │ │
│  │   6. If deadlocked → voting fallback (BFT)                                    │ │
│  │   7. Return precision-weighted consensus belief                                │ │
│  │                                                                                │ │
│  │   Neuromodulator Channels:                                                     │ │
│  │   • DA (~2s): Reward    • ACh (~50ms): Attention                              │ │
│  │   • NE (~3s): Priority  • 5-HT (~10s): Context                                │ │
│  └───────────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                              │
│  ┌───────────────────────────────────┼───────────────────────────────────────────┐ │
│  │                                   ▼                                            │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │ │
│  │  │   CRDT      │  │ Convergence │  │   Voting    │  │    KG       │           │ │
│  │  │ Workspace   │  │  Detector   │  │  Fallback   │  │  Discovery  │           │ │
│  │  │ • Top-K     │  │ • Agreement │  │ • Byzantine │  │ • Neighbors │           │ │
│  │  │ • Vec clock │  │ • Deadlock  │  │ • Consensus │  │ • Topology  │           │ │
│  │  │ • Salience  │  │ • Ruminate  │  │ • 1/3 fault │  │ • Register  │           │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘           │ │
│  └───────────────────────────────────────────────────────────────────────────────┘ │
│                                                                                     │
│  ┌───────────────────────────────────────────────────────────────────────────────┐ │
│  │                        INFRASTRUCTURE LAYER                                    │ │
│  │  ┌───────────┐   ┌───────────┐   ┌───────────┐   ┌───────────┐                │ │
│  │  │ Exception │──▶│  Health   │──▶│  Logging  │──▶│    BBB    │                │ │
│  │  │ • Epitopes│   │ •Heartbeat│   │ •Structured│   │ •Validate │                │ │
│  │  │ • Immune  │   │ •Watchdog │   │ •Module ID │   │ •Protect  │                │ │
│  │  └───────────┘   └───────────┘   └───────────┘   └───────────┘                │ │
│  └───────────────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Bio-Async Module IDs

| Module | ID | Range |
|--------|-----|-------|
| Spatial Language | 0x0826 | 0x8260-0x826F |
| Numerical Language | 0x0827 | 0x8270-0x827F |
| Phonological WM | 0x0828 | 0x8280-0x828F |
| Linguistics SNN Bridge | 0x0829 | 0x8290-0x829F |
| Linguistics Plasticity Bridge | 0x082A | 0x82A0-0x82AF |
| Linguistics Training Bridge | 0x082B | 0x82B0-0x82BF |
| Linguistics Hypothalamus Bridge | 0x082C | 0x82C0-0x82CF |
| Linguistics Omni Bridge | 0x082D | 0x82D0-0x82DF |
| Linguistics Engram Bridges | 0x082E | 0x82E0-0x82EF |
| Linguistics Math Bridge | 0x082F | 0x82F0-0x82FF |
| Linguistics Quantum Bridge | 0x0830 | 0x8300-0x830F |
| Linguistics Health Bridge | 0x0831 | 0x8310-0x831F |
| Linguistics KG Bridge | 0x0832 | 0x8320-0x832F |
| Linguistics Logging Bridge | 0x0833 | 0x8330-0x833F |
| Linguistics Symbolic Bridge | 0x0834 | 0x8340-0x834F |
| Linguistics Basal Ganglia Bridge | 0x0835 | 0x8350-0x835F |
| Linguistics Cerebellum Bridge | 0x0836 | 0x8360-0x836F |
| Linguistics Medulla Bridge | 0x0837 | 0x8370-0x837F |
| Linguistics Perception Bridge | 0x0838 | 0x8380-0x838F |
| Linguistics ToM+WM Bridge | 0x0839 | 0x8390-0x839F |
| **Linguistics Mesh Coordinator** | **0x083A** | **0x83A0-0x83AF** |

(Note: Existing Language-Parietal bridge is 0x0825)

---

## 10. Dependencies

### Required Headers - Core Parietal
- `nimcp_parietal.h` - Parietal orchestrator
- `nimcp_number_sense.h` - Weber-Fechner, subitizing
- `nimcp_spatial_reasoning.h` - Mental rotation, coordinate transforms

### Required Headers - Fuzzy & Statistics
- `nimcp_fuzzy_types.h` - 14 MF types, 8 hedges
- `nimcp_fuzzy_inference.h` - Mamdani/Sugeno/Tsukamoto
- `nimcp_statistics.h` - Bayesian inference, distributions
- `nimcp_ml_statistics.h` - HMM, GMM, Gaussian Processes
- `nimcp_information_theory.h` - Entropy, MI, KL divergence

### Required Headers - SNN
- `nimcp_snn.h` - Master SNN facade
- `nimcp_snn_types.h` - Population organization
- `nimcp_snn_network.h` - Network orchestration
- `nimcp_snn_encoding.h` - Spike encoding/decoding
- `nimcp_snn_training.h` - STDP, R-STDP, eProp training

### Required Headers - Plasticity
- `nimcp_stdp.h` - Pairwise STDP (Bi & Poo 1998)
- `nimcp_triplet_stdp.h` - Triplet STDP (Pfister & Gerstner 2006)
- `nimcp_homeostatic.h` - Synaptic scaling, intrinsic plasticity, BCM
- `nimcp_structural_plasticity.h` - Spine lifecycle (NASCENT→ELIMINATED)
- `nimcp_eligibility_trace.h` - Temporal credit assignment
- `nimcp_bcm.h` - BCM rule for competitive learning

### Required Headers - Training
- `nimcp_training_module.h` - Core training infrastructure
- `nimcp_brain_training_integration.h` - Unified coordinator (TM-3)
- `nimcp_training_plasticity_bridge.h` - Loss→RPE→Dopamine (TPB-1)
- `nimcp_training_callbacks.h` - Event-driven control (TCB-1)
- `nimcp_meta_learning.h` - MAML, Reptile, Prototypical Networks
- `nimcp_curriculum_learning.h` - SPL, teacher-guided, uncertainty-based
- `nimcp_loss_functions.h` - MSE, CE, CTC, Contrastive
- `nimcp_optimizers.h` - SGD, Adam, AdamW

### Required Headers - Memory & Cognitive
- `nimcp_engram.h` - Memory engram system
- `nimcp_systems_consolidation.h` - Hippocampus→Cortex transfer
- `nimcp_working_memory.h` - Working memory integration
- `nimcp_theta_gamma.h` - Encoding/retrieval gating

### Required Headers - Hypothalamus & Omni
- `nimcp_hypothalamus_orchestrator.h` - Drive coordination
- `nimcp_hypothalamus_drives.h` - Curiosity, social, etc.
- `nimcp_omni_world_model.h` - RSSM predictions
- `nimcp_omni_precision.h` - Precision weighting
- `nimcp_omni_active_inference.h` - Action selection

### Required Headers - Math Utilities
- `nimcp_complex_math.h` - Phasors, phase coherence
- `nimcp_fft.h` - FFT for spectral analysis
- `nimcp_hilbert.h` - Instantaneous phase/amplitude
- `nimcp_signal_filter.h` - Bandpass filters for brain rhythms
- `nimcp_integration.h` - RK4/Adaptive for dynamics
- `nimcp_svd_simple.h` - SVD for dimensionality reduction
- `nimcp_mps.h` - MPS/Tensor Train compression
- `nimcp_tensor.h` - N-D tensor operations
- `nimcp_hyperbolic.h` - Poincaré ball geometry
- `nimcp_positional_encoding.h` - RoPE, ALiBi, sinusoidal
- `nimcp_gabor.h` - Visual word form processing

### Required Headers - Quantum Algorithms
- `nimcp_quantum_walk.h` - Quantum walk for spreading activation
- `nimcp_quantum_monte_carlo.h` - QMC sampling
- `nimcp_quantum_annealing.h` - Escape local minima
- `nimcp_quantum_ternary.h` - Ternary quantum states
- `nimcp_quantum_mcts.h` - Quantum-enhanced tree search
- `nimcp_quantum_reasoning.h` - Grover search, ternary logic
- `nimcp_quantum_shannon.h` - Information-theoretic metrics
- `nimcp_qmc_gpu.h` - GPU-accelerated quantum Monte Carlo

### Required Headers - Exception & Immune System
- `nimcp_exception.h` - Base exception with 64-byte epitopes
- `nimcp_exception_immune.h` - Exception-to-immune bridge
- `nimcp_exception_macros.h` - NIMCP_THROW_TO_IMMUNE(), NIMCP_CHECK_THROW_IMMUNE()

### Required Headers - BBB & Security
- `nimcp_blood_brain_barrier.h` - Four-layer defense system

### Required Headers - Health & Resilience
- `nimcp_health_monitor.h` - Real-time health metrics
- `nimcp_health_agent.h` - Independent watchdog agent
- `nimcp_heartbeat.h` - Liveness detection

### Required Headers - KG Wiring
- `nimcp_brain_kg.h` - Brain knowledge graph

### Required Headers - Logging
- `nimcp_logging.h` - Async logging with lock-free ring buffer

### Required Headers - Symbolic Logic
- `nimcp_symbolic_logic.h` - Core logic engine (FOL)
- `nimcp_symbolic_logic_lgss_loader.h` - JSON rule loading
- `nimcp_symbolic_logic_safety.h` - mprotect-locked Safety KB

### Required Headers - Basal Ganglia
- `nimcp_basal_ganglia.h` - Main orchestrator
- `nimcp_striatum.h` - D1/D2 MSN pathways
- `nimcp_substantia_nigra.h` - Dopamine production (SNc/SNr)
- `nimcp_globus_pallidus.h` - GPi/GPe output nuclei
- `nimcp_subthalamic.h` - Hyperdirect pathway (STN)
- `nimcp_basal_ganglia_fep_bridge.h` - FEP action selection
- `nimcp_basal_ganglia_executive_bridge.h` - PFC control
- `nimcp_basal_ganglia_training_bridge.h` - RL plasticity
- `nimcp_basal_ganglia_thalamus_bridge.h` - Motor output relay

### Required Headers - Cerebellum
- `nimcp_cerebellum_adapter.h` - Core orchestrator
- `nimcp_language_cerebellum_bridge.h` - **EXISTING** speech timing bridge
- `nimcp_medulla_cerebellum_bridge.h` - Inferior olive error signaling
- `nimcp_pr_cerebellum_bridge.h` - Procedural memory sequences
- `nimcp_cerebellum_thalamic_bridge.h` - Motor output routing
- `nimcp_soma_cerebellum_bridge.h` - Proprioceptive feedback

### Required Headers - Medulla
- `nimcp_medulla.h` - Core orchestrator
- `nimcp_hypothalamus_medulla_bridge.h` - Drive → autonomic
- `nimcp_medulla_immune_bridge.h` - Cytokine effects on arousal

### Required Headers - Perception Layer
- `nimcp_cochlea.h` - Peripheral auditory pathway
- `nimcp_audio_cortex.h` - Primary auditory cortex (A1)
- `nimcp_speech_cortex.h` - Speech-specialized processing (44 phonemes)
- `nimcp_lip_reading.h` - Visual speech perception (12 visemes)
- `nimcp_visual_cortex.h` - V1-style visual processing
- `nimcp_cochlea_broca_bridge.h` - Speech perception-production link
- `nimcp_cochlea_occipital_bridge.h` - Audiovisual binding

### Required Headers - Theory of Mind & World Model
- `nimcp_theory_of_mind.h` - BDI model, 32-agent tracking, false belief detection
- `nimcp_omni_world_model.h` - RSSM architecture, counterfactual reasoning
- `nimcp_omni_wm_tom_bridge.h` - Bidirectional ToM-WM integration
- `nimcp_omni_precision.h` - Precision weighting for ambiguity
- `nimcp_omni_active_inference.h` - Policy selection

### Required Headers - Mesh Architecture (NEW)
- `nimcp_bio_router.h` - Bio-async broadcast (`bio_router_broadcast()`)
- `nimcp_gossip_beliefs.h` - Gossip protocol belief propagation
- `nimcp_fep_orchestrator.h` - Free energy minimization for convergence
- `nimcp_fep_neuromod.h` - Precision weighting (trust = 1/σ²)
- `nimcp_collective_workspace.h` - CRDT shared state with vector clocks
- `nimcp_swarm_consensus.h` - Byzantine FT voting fallback
- `nimcp_inner_dialogue_convergence.h` - Agreement/deadlock/rumination detection
- `nimcp_brain_kg_helpers.h` - Topology-aware module discovery

### Build Integration
- Add to `src/lib/CMakeLists.txt`:
  - New source files for linguistics modules
  - New source files for all bridges
- Add to `test/unit/cognitive/parietal/CMakeLists.txt`:
  - Unit tests for each module
- Add to `test/integration/cognitive/parietal/CMakeLists.txt`:
  - Integration tests for bridge combinations
- Link dependencies:
  - `nimcp_snn`, `nimcp_plasticity`, `nimcp_training`
  - `nimcp_fuzzy`, `nimcp_statistics`
  - `nimcp_engram`, `nimcp_omni`, `nimcp_hypothalamus`

---

## 11. Success Criteria

### Core Module Criteria
1. **Spatial Language:** Parse 30+ prepositions with fuzzy semantics
2. **Numerical Language:** Parse/generate numbers 0-999,999,999 with ordinals
3. **Phonological WM:** Maintain 7±2 items with realistic decay (~2s)

### SNN Integration Criteria
4. **SNN Populations:** Phoneme, spatial word, number word populations created
5. **Spike Encoding:** Words correctly encoded/decoded via rate/temporal coding
6. **SNN Training:** STDP, R-STDP, eProp modes functional

### Plasticity Integration Criteria
7. **Pairwise STDP:** Word-meaning associations form with correct timing
8. **Triplet STDP:** Sequence learning ("twenty-one") shows frequency dependence
9. **R-STDP:** Dopamine modulation affects learning rate (DA gain = 100)
10. **BCM:** Competitive word selection shows winner-take-all dynamics
11. **Structural Plasticity:** Spine lifecycle (NASCENT→STABLE→POTENTIATED→PRUNING)
12. **Homeostatic:** Synaptic scaling maintains stable firing rates

### Training Integration Criteria
13. **Training Phases:** T1-T4 phases execute correctly for linguistics
14. **Loss-RPE-DA:** Loss changes correctly convert to dopamine modulation
15. **Curriculum Learning:** Vocabulary progresses through difficulty stages
16. **Meta-Learning:** Few-shot word learning via prototypical networks
17. **Callbacks:** Plateau detection triggers LR reduction

### Memory Integration Criteria
18. **Engram Encoding:** Words encoded with emotional tagging
19. **Consolidation:** LABILE→CONSOLIDATED transition over simulated 6 hours
20. **Sleep Replay:** Consolidation rate increases 2x during sleep
21. **Reconsolidation:** 6-hour labile window after recall

### Math Utilities Criteria
22. **FFT/Hilbert:** Theta-gamma coupling detected with PAC > 0.3
23. **Hyperbolic:** Semantic distances computed in Poincaré ball (5D)
24. **MPS Compression:** 10-100x compression with >95% accuracy
25. **Positional Encoding:** RoPE applied correctly to word sequences

### Quantum Algorithm Criteria
26. **Quantum Walk:** O(√N) spreading activation (faster than classical)
27. **Quantum Annealing:** Escape local minima in parsing
28. **Quantum MCTS:** Better language generation than classical MCTS
29. **Quantum Reasoning:** Grover search finds word sense (≤16 variables)
30. **Quantum-Shannon:** Channel capacity metrics computed correctly

### System Integration Criteria
31. **Hypothalamus:** Curiosity drive boosts plasticity for novel words
32. **Omni WM:** World model predicts spatial outcomes from language
33. **Bio-Async:** All modules communicate via bio-async messaging
34. **All tests pass:** Unit, integration, and e2e tests
35. **Performance:** <10ms latency for single word processing

### Exception & Immune System Criteria
36. **Exception Handling:** All error paths use `NIMCP_THROW_TO_IMMUNE()` or `NIMCP_CHECK_THROW_IMMUNE()`
37. **Epitope Generation:** 64-byte immune epitopes correctly identify module/function/severity
38. **Recovery Actions:** GC, ROLLBACK, QUARANTINE actions trigger correctly
39. **Immune Response:** Antibody responses (B cell → PLASMA) produce correct recovery

### BBB & Security Criteria
40. **Input Validation:** All word inputs validated (UTF-8, length <1024, no null bytes)
41. **BBB Blocking:** Malformed inputs correctly blocked with BBB_ACTION_BLOCK
42. **Memory Protection:** Engram storage regions protected from unauthorized access

### Health & Resilience Criteria
43. **Heartbeat Registration:** All modules register heartbeat (100ms interval)
44. **Health Metrics:** Latency, error rate, memory usage tracked per module
45. **Watchdog Recovery:** Missed heartbeats (5x) trigger recovery escalation
46. **Recovery Levels:** LIGHT/MEDIUM/HEAVY/FULL recovery levels execute correctly

### KG Wiring Criteria
47. **Node Registration:** All linguistics modules registered as KG nodes
48. **Edge Wiring:** Module dependencies (PROJECTS_TO, EXCITES, DEPENDS_ON) wired
49. **KG Queryable:** `brain_kg_query()` returns linguistics modules

### Logging Criteria
50. **Module Logging:** All modules use `LOG_MODULE_*` macros with correct module IDs
51. **Structured Logging:** JSON format logs for metrics (latency, confidence, etc.)
52. **Log Levels:** Appropriate levels (DEBUG for dev, INFO for milestones, WARN/ERROR for issues)

### Symbolic Logic & LGSS Criteria
53. **Spatial Rules:** Transitivity, symmetry rules execute correctly (e.g., left_of ↔ right_of)
54. **Numerical Rules:** Compound number rules compute correctly (twenty + three = 23)
55. **LGSS Safety:** Harmful content blocked by LGSS with SAFETY_DOMAIN_HARMFUL_SPEECH
56. **Forward Chaining:** Given spatial facts, derive new spatial states
57. **Backward Chaining:** Given target location, find path via preposition chain

### Basal Ganglia Integration Criteria
58. **Word Selection:** D1/D2 pathway competition selects correct word from candidates
59. **D1 Activation:** Direct pathway (GO) activates for contextually appropriate words
60. **D2 Activation:** Indirect pathway (NO-GO) suppresses competing/incorrect words
61. **Grammar Habits:** Grammar rules reach habit strength ≥0.7 after repeated use
62. **Habit Automaticity:** Habitual phrases produce faster response (<50% latency)
63. **RPE→Dopamine:** Training layer RPE correctly modulates BG dopamine
64. **D1/D2 Learning:** Positive RPE → D1 LTP + D2 LTD; Negative RPE → opposite
65. **Mode Transitions:** GOAL_DIRECTED → HABITUAL transition at cognitive load >0.9
66. **FEP Word Selection:** Expected free energy correctly evaluates word utilities
67. **Executive Inhibition:** PFC suppresses grammatically incorrect forms
68. **Conflict Detection:** High conflict (>0.7) detected for ambiguous word choices
69. **Phrase Habits:** Frequent phrases ("on the left") become automatic habits

### Integration Verification Criteria (Data Flow)
70. **Fuzzy Called:** `fuzzy_mf_evaluate()` returns membership in [0,1] for spatial prepositions
71. **SNN Fires:** `snn_population_get_firing_rate()` > 0 after phoneme encoding
72. **STDP Updates:** Weight changes after pre-post spike pairing (Δw ≠ 0)
73. **Triplet Frequency:** Weight change larger at 40Hz than at 5Hz
74. **R-STDP Dopamine:** Weight change scales with dopamine level (r > 0.9 correlation)
75. **Engram Progresses:** State transitions ENCODING → LABILE → CONSOLIDATED over simulated time
76. **Hypothalamus Modulates:** High curiosity drive increases plasticity rate by >20%
77. **Omni Predicts:** World model generates non-null prediction from spatial language
78. **BG Selects:** `basal_ganglia_select_action()` returns valid action ID, D1/D2 > 0
79. **Training RPE:** `training_compute_rpe()` returns non-zero for loss changes
80. **BBB Validates:** Valid input → ALLOW, invalid → BLOCK (100% accuracy)
81. **Health Heartbeats:** Heartbeat count increments after each callback
82. **KG Queryable:** `brain_kg_query("spatial_language")` returns valid node
83. **Logs Written:** Module log entries appear in log output
84. **Exception→Immune:** Epitope received by immune system within 10ms
85. **Symbolic Chains:** Forward chaining produces ≥1 derived fact from spatial assertions

### Cerebellum Integration Criteria
86. **Timing Prediction:** `language_cerebellum_predict_duration()` returns valid ms value
87. **Timing Accuracy:** Predicted vs actual phoneme duration error <10ms after learning
88. **Error Learning:** `language_cerebellum_update_model()` reduces timing error over trials
89. **Rhythm Patterns:** All 4 rhythm types (isochronous, stress, syllable, mora) produce distinct patterns
90. **Speech Rate:** `language_cerebellum_set_speech_rate()` affects timing predictions proportionally
91. **Articulatory Output:** 8-DOF motor commands generated for each phoneme
92. **Forward Model:** Predicted formants correlate with actual (r > 0.8)
93. **Climbing Fiber:** Error signals trigger LTD (weight decrease) at P-F synapses
94. **Automatization:** Word fluency progresses NOVICE → EXPERT over 100+ repetitions
95. **Procedural Sequences:** Motor sequences execute with correct inter-element timing

### Medulla Integration Criteria
96. **Arousal Query:** `medulla_get_arousal_level()` returns value in [0,1]
97. **Speech Rate Mult:** Arousal maps to speech rate multiplier (0.7x-1.3x range)
98. **Motor Precision:** Precision peaks at ALERT level, degrades at extremes (inverted U)
99. **Vocal Tremor:** Tremor magnitude increases when precision < 0.7
100. **Protection Gating:** CRITICAL level blocks voluntary speech, NORMAL allows full speech
101. **Circadian Effects:** MORNING phase shows better clarity/fluency than DEEP_NIGHT
102. **Breath Coordination:** Utterances pause at breath boundaries (available_airflow < threshold)
103. **Respiratory Sync:** Speech syllable rate correlates with respiratory rhythm

### Perception Layer Integration Criteria
104. **Phoneme Detection:** `speech_cortex_detect_phonemes()` returns valid phoneme sequence
105. **Phoneme Confidence:** Each phoneme has confidence in [0,1]
106. **Formant Extraction:** F1, F2 values in valid ranges (200-2500 Hz)
107. **Vowel Classification:** F1/F2 correctly maps to vowel categories
108. **Prosody Extraction:** Pitch (Hz), stress [0-1], rhythm extracted
109. **Viseme Detection:** `lip_reading_classify_viseme()` returns one of 12 visemes
110. **McGurk Fusion:** Audiovisual integration produces fused phoneme
111. **SNR Weighting:** Low SNR increases visual weight, high SNR increases audio weight
112. **Silent Speech:** Visual-only recognition produces viseme sequence
113. **Speaker Adaptation:** Profile update improves accuracy over 300 frames
114. **Spatial Parsing:** Phoneme stream correctly identifies spatial prepositions
115. **Number Parsing:** Phoneme stream correctly identifies number words
116. **Top-Down Prediction:** Set prediction affects phoneme recognition
117. **Prediction Error:** Error computed when prediction mismatches perception

### Theory of Mind & World Model Integration Criteria
118. **Reference Frame Selection:** `ling_tom_wm_select_reference_frame()` returns frame based on listener perspective
119. **Perspective Alignment:** Lateral distance < 0.3 → allocentric frame selected
120. **Pronoun Resolution:** "I"/"you" correctly resolved to speaker/listener agent IDs
121. **BDI State Access:** `tom_get_bdi_state()` returns valid beliefs, desires, intentions
122. **Spatial Deixis Transform:** Perspective gap > 0.5 → reference transform computed
123. **Intent Detection:** `tom_infer_intention()` returns intent with confidence [0-1]
124. **Intent Mismatch:** Stated vs inferred intent difference > 0.3 flagged
125. **Audience Design:** `tom_get_agent_belief()` returns listener vocabulary knowledge
126. **Word Adaptation:** Unknown word to listener → simpler synonym selected
127. **Listener Expertise:** Low spatial_ability trait → egocentric frame used
128. **Common Ground:** `tom_compute_common_ground()` returns shared vs private knowledge
129. **New Information:** Only novel info (not in common ground) included in utterance
130. **Counterfactual:** `omni_wm_tom_bridge_counterfactual_belief()` computes hypothetical states
131. **Counterfactual Spatial:** Spatial relations computed correctly in counterfactual
132. **Mental State Prediction:** `omni_wm_tom_bridge_predict_mental_state()` returns confidence per step
133. **Instruction Clarity:** Confidence < 0.7 → clarification inserted
134. **Empathy Simulation:** `omni_wm_tom_bridge_empathy_simulation()` aligns perspectives
135. **False Belief Detection:** `tom_detect_false_belief()` returns reality gap [0-1]
136. **ToM Agent Limit:** Up to 32 agents tracked simultaneously
137. **Perspective Tracking:** perspective_alignment score in [0-1] range

### Mesh Architecture Integration Criteria
138. **Mesh Coordinator Creation:** `linguistics_mesh_create()` returns valid coordinator
139. **Participant Registration:** All 15+ bridges register successfully with mesh
140. **Mesh Handler Interface:** Each bridge implements `process()`, `update()`, `get_precision()`
141. **Request Broadcast:** `bio_router_broadcast()` reaches all registered participants
142. **Belief Generation:** Each participant produces valid `linguistics_belief_t` with precision
143. **Gossip Propagation:** `gossip_propagate_round()` spreads beliefs to neighbors
144. **FEP Convergence:** Free energy F decreases monotonically during update loop
145. **Belief Update:** `μ' = μ - lr * Π * ε` correctly precision-weights neighbor beliefs
146. **Agreement Score:** `agreement_score` reaches ≥0.75 within max_iterations
147. **Convergence Detection:** `inner_dialogue_convergence_analyze()` detects converged state
148. **Deadlock Detection:** Oscillating disagreement correctly detected
149. **Voting Fallback:** `swarm_consensus_propose()` resolves deadlock via BFT voting
150. **CRDT Merge:** `collective_workspace_merge()` correctly combines beliefs with vector clocks
151. **Precision Influence:** Higher precision participants have proportionally more influence on consensus
152. **Byzantine Tolerance:** Mesh converges correctly with ≤1/3 faulty participants
153. **Neuromodulator Routing:** Messages routed through correct channel (DA/ACh/NE/5-HT)
154. **KG Discovery:** `brain_kg_query_neighbors()` discovers available mesh participants
155. **Mesh Latency:** 12-participant mesh converges in <100ms
156. **Timeout Handling:** Mesh returns best available belief if timeout before convergence
157. **Collective Response:** Final response is precision-weighted average of converged beliefs

---

## 12. Verification Plan

### Build Verification
```bash
cd /home/bbrelin/nimcp/build && cmake .. && make -j4
```

### Test Verification - Unit Tests
```bash
# Core module tests
./test/unit/cognitive/parietal/linguistics/test_parietal_spatial_language
./test/unit/cognitive/parietal/linguistics/test_parietal_numerical_language
./test/unit/cognitive/parietal/linguistics/test_parietal_phonological_wm

# Bridge tests
./test/unit/cognitive/parietal/linguistics/bridges/test_linguistics_snn_bridge
./test/unit/cognitive/parietal/linguistics/bridges/test_linguistics_plasticity_bridge
./test/unit/cognitive/parietal/linguistics/bridges/test_linguistics_training_bridge
./test/unit/cognitive/parietal/linguistics/bridges/test_linguistics_engram_bridge
```

### Test Verification - Integration Tests
```bash
# Module integration
./test/integration/cognitive/parietal/linguistics/test_spatial_language_integration
./test/integration/cognitive/parietal/linguistics/test_numerical_language_integration
./test/integration/cognitive/parietal/linguistics/test_phonological_wm_integration

# SNN + Plasticity integration
./test/integration/cognitive/parietal/linguistics/test_linguistics_snn_plasticity_integration

# Training integration
./test/integration/cognitive/parietal/linguistics/test_linguistics_training_integration
```

### Test Verification - End-to-End Tests
```bash
# Full vocabulary learning scenario
./test/e2e/cognitive/parietal/linguistics/test_vocabulary_learning_e2e

# Sleep consolidation scenario
./test/e2e/cognitive/parietal/linguistics/test_sleep_consolidation_e2e
```

### Manual Verification Scenarios

**1. Spatial Language**
- Parse prepositions: "in", "on", "near", "adjacent", "between"
- Apply hedges: "very near", "somewhat left"
- Reference frame selection with ambiguous inputs

**2. Numerical Language**
- Parse: "twenty-three", "one million", "third"
- Generate: 0, 999999999, negative numbers
- Approximate quantities: "few", "many", "most"

**3. Phonological WM**
- Capacity test: 7±2 items
- Decay test: ~2 second trace decay
- Similarity effects: confusable phonemes

**4. SNN Integration**
- Verify spike encoding for words
- Verify population activity patterns
- Test STDP learning with spike timing

**5. Plasticity Integration**
- Pairwise STDP: Pre-before-post → LTP
- Triplet STDP: High frequency → enhanced LTP
- R-STDP: Dopamine burst → 3x learning
- BCM: Winner-take-all competition
- Structural: NASCENT→STABLE transition

**6. Training Integration**
- Loss decrease → positive RPE → dopamine
- Curriculum progression through stages
- Meta-learning few-shot word acquisition
- Callback plateau detection

**7. Math Utilities Integration**
- FFT: Phonological spectral decomposition
- Hilbert: Theta-gamma phase extraction
- Hyperbolic: Semantic distance in Poincaré ball
- MPS: Compressed word embedding lookup (10-100x)
- Positional: RoPE word order encoding

**8. Quantum Integration**
- Quantum walk: O(√N) spreading activation
- Quantum annealing: Escape local parse minima
- Quantum MCTS: Better word sequence generation
- Quantum reasoning: Grover search for word sense
- Quantum-Shannon: Channel capacity metrics

**9. Engram Integration**
- Encoding with emotional tag
- LABILE→CONSOLIDATED over 6 hours simulated
- Sleep consolidation 2x rate boost
- Reconsolidation 6-hour window

**10. System Integration**
- Curiosity drive → enhanced plasticity
- World model spatial prediction
- Full vocabulary learning scenario end-to-end

**11. Exception & Immune System**
- Trigger exception via NULL pointer → verify epitope generation
- Verify immune system receives exception notification
- Verify B cell → PLASMA state transition produces antibody
- Verify recovery action (GC/ROLLBACK/QUARANTINE) executes
- Test exception severity levels (WARNING vs ERROR vs CRITICAL)

**12. BBB Integration**
- Submit valid UTF-8 word → BBB_ACTION_ALLOW
- Submit word with null bytes → BBB_ACTION_BLOCK
- Submit word exceeding max length → BBB_ACTION_BLOCK
- Submit malformed UTF-8 → BBB_ACTION_BLOCK
- Verify blocked requests logged correctly

**13. Health & Resilience**
- Verify heartbeat registration for each module
- Simulate missed heartbeats → verify watchdog trigger
- Verify health metrics (latency, error rate) update
- Trigger HEALTH_DEGRADED → verify recovery escalation
- Test LIGHT → MEDIUM → HEAVY → FULL recovery progression

**14. KG Wiring**
- Query KG for "parietal_spatial_language" → verify node exists
- Query KG for edges from spatial → numerical → verify PROJECTS_TO edge
- Verify all bridge nodes registered
- Test KG traversal from hypothalamus → linguistics modules

**15. Logging**
- Verify LOG_MODULE_DEBUG appears in debug build
- Verify LOG_MODULE_INFO appears in release build
- Verify structured JSON logs parse correctly
- Verify latency metrics captured in logs
- Test log rotation / ring buffer overflow handling

**16. Symbolic Logic & LGSS**
- Assert left_of(A, B) → verify backward chain derives right_of(B, A)
- Assert above(A, B), above(B, C) → verify forward chain derives above(A, C)
- Parse "twenty-three" → verify compound number rule applied
- Submit harmful content → verify LGSS blocks with SAFETY_DOMAIN_HARMFUL_SPEECH
- Test unknown word → verify safety check passes (no false positives)

**17. Basal Ganglia Integration**
- Present word candidates {near, adjacent, beside} with activations → verify BG selects highest
- Verify D1 pathway activation for selected word > 0.5
- Verify D2 pathway activation for rejected words > 0.3 (suppression)
- Repeat phrase "on the left" 10x → verify habit strength increases
- After habit strength ≥0.7 → verify mode = BG_MODE_HABITUAL
- Measure response latency: habitual < goal-directed
- Simulate positive feedback → verify D1 weight increases
- Simulate negative feedback → verify D2 weight increases
- Test grammatically incorrect form → verify executive bridge suppresses
- Present ambiguous context → verify conflict level > 0.5
- Test FEP word selection → verify expected free energy computed
- Verify dopamine level tracks training RPE signal

**18. Cerebellum Integration**
- Call `language_cerebellum_predict_duration(phoneme_id)` → verify returns >0 ms
- Report actual duration, call update_model() → verify error decreases over 10 trials
- Set speech rate to 3 syl/sec → verify timing predictions scale by 0.5x
- Get timing pattern for STRESS_TIMED → verify stress/unstress ratio ~2:1
- Generate 8-DOF motor command → verify all DOF in valid range [0,1]
- Trigger climbing fiber error → verify LTD occurs (weight decreases)
- Execute word sequence 100x → verify automatization reaches PROFICIENT+
- Predict formants → compare with actual → verify r > 0.8 correlation

**19. Medulla Integration**
- Query arousal at ALERT level → verify speech rate mult ~1.0
- Query arousal at DROWSY level → verify speech rate mult ~0.7
- Query arousal at HYPERAROUSAL → verify precision < 0.8 (degraded)
- Set protection to CRITICAL → verify `is_speech_allowed()` returns false
- Set protection to NORMAL → verify `is_speech_allowed()` returns true
- Query circadian at MORNING → verify clarity mult > circadian at DEEP_NIGHT
- Simulate low airflow → verify breath pause inserted in utterance
- Verify vocal tremor magnitude > 0 when precision < 0.7

**20. Perception Layer Integration**
- Process audio → verify phoneme stream returned with >0 phonemes
- Check phoneme confidence → verify all in [0,1] range
- Extract formants for vowel → verify F1 in [200,900], F2 in [700,2300]
- Classify vowel from F1=300, F2=2200 → verify returns IY (high front)
- Extract prosody → verify pitch_hz > 0, stress in [0,1]
- Process video frame → verify viseme returned from 12 categories
- Fuse audio+visual at SNR=15dB → verify audio_weight > visual_weight
- Fuse audio+visual at SNR=-15dB → verify visual_weight > audio_weight
- Process video-only → verify silent speech returns viseme sequence
- Register speaker, update 300 frames → verify adaptation_level increases
- Feed phoneme stream containing "left" → verify spatial preposition detected
- Feed phoneme stream containing "twenty" → verify number word detected
- Set prediction to phoneme X, present phoneme Y → verify prediction_error > 0

**21. Theory of Mind & World Model Integration**
- Call `ling_tom_wm_select_reference_frame(listener)` → verify returns valid frame
- Simulate listener at same location → verify allocentric frame selected (lateral < 0.3)
- Simulate listener at different location → verify egocentric frame considered (lateral > 0.5)
- Resolve "I" pronoun → verify returns speaker agent ID
- Resolve "you" pronoun → verify returns listener agent ID
- Call `tom_get_bdi_state(agent)` → verify beliefs, desires, intentions populated
- Compute spatial deixis "left" with perspective gap → verify transform applied
- Compare stated vs inferred intent → verify mismatch flagged when diff > 0.3
- Query listener vocabulary → verify `tom_get_agent_belief()` returns valid knowledge
- Present unknown word to low-expertise listener → verify simpler synonym selected
- Query listener spatial_ability trait → verify affects frame selection
- Compute common ground between 2 agents → verify shared vs private distinguished
- Generate utterance → verify only novel info (not in common ground) included
- Call `omni_wm_tom_bridge_counterfactual_belief(agent, "at_door")` → verify state computed
- Compute spatial relation in counterfactual → verify relation from hypothetical position
- Predict mental states for 5-step instruction → verify confidence per step returned
- Insert instruction with confidence < 0.7 → verify clarification added
- Call `omni_wm_tom_bridge_empathy_simulation()` → verify perspective_shift returned
- Call `tom_detect_false_belief()` → verify reality_gap in [0,1] returned
- Register 32 agents → verify all tracked (max capacity)
- Query perspective_alignment → verify in [0,1] range

**22. Mesh Architecture Integration**
- Create mesh coordinator → verify valid handle returned
- Register all 15+ bridge participants → verify registration success
- Submit mesh request "parse spatial: left of table" → verify broadcast occurs
- Verify each participant produces belief with precision in (0,1]
- Call `gossip_propagate_round()` → verify beliefs spread to neighbors
- After 10 iterations → verify agreement_score increased
- After convergence → verify agreement_score ≥ 0.75
- Verify final belief = precision-weighted average of participant beliefs
- Inject 4/12 faulty beliefs → verify mesh still converges correctly (BFT)
- Simulate deadlock (oscillating beliefs) → verify deadlock detected
- After deadlock → verify voting fallback via `swarm_consensus_propose()`
- Verify neuromodulator channel: attention requests → ACh (~50ms decay)
- Verify neuromodulator channel: reward signals → DA (~2s decay)
- Query mesh stats → verify latency, iteration count, convergence state
- Test timeout (set max_iterations=5 with slow convergence) → verify best belief returned
- Verify CRDT workspace: conflicting interpretations merged via salience
- Run full spatial parse through mesh → verify all participants contribute
- Compare mesh response vs hub-and-spoke → verify mesh produces coherent result
- Performance: 12 participants converge in <100ms (typical case)
- Performance: Byzantine case (4 faulty) converges in <200ms

- All 16 implementation phases verified

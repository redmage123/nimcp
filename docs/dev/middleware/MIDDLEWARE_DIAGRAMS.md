# NIMCP Middleware Architecture - Visual Diagrams

**Version:** 1.0
**Date:** 2025-11-19
**Related:** MIDDLEWARE_ARCHITECTURE.md, MIDDLEWARE_API_REFERENCE.md

---

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────────────┐
│                    HIGH-LEVEL COGNITIVE SYSTEMS                      │
│  (src/cognitive/)                                                    │
├─────────────────────────────────────────────────────────────────────┤
│ Ethics  │ Working  │ Emotions │ Reasoning │ Knowledge │ Introspection│
│ Engine  │ Memory   │ System   │ Logic     │ System    │ Context      │
│         │          │          │           │           │              │
│ Action  │ Float    │ Emotion  │ Logic     │ Multi-    │ Uncertainty  │
│ Context │ Features │ Tags     │ Gates     │ Domain    │ Estimation   │
└─────────────────────────────────────────────────────────────────────┘
                                  ▲
                                  │
                    ┌─────────────┴─────────────┐
                    │  MIDDLEWARE ABSTRACTION   │
                    └─────────────┬─────────────┘
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        MIDDLEWARE LAYER                              │
│  (src/middleware/)                                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐             │
│  │   ENCODING    │  │   FEATURES   │  │   PATTERNS   │             │
│  ├───────────────┤  ├──────────────┤  ├──────────────┤             │
│  │ Rate Coding   │  │ Extractor    │  │ Synchrony    │             │
│  │ Temporal Code │  │ Population   │  │ Sequences    │             │
│  │ Population    │  │ Temporal     │  │ Phase Lock   │             │
│  └───────────────┘  └──────────────┘  └──────────────┘             │
│                                                                       │
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐             │
│  │   ROUTING     │  │   BUFFERS    │  │ NORMALIZATION│             │
│  ├───────────────┤  ├──────────────┤  ├──────────────┤             │
│  │ Thalamic      │  │ Sliding      │  │ Z-Score      │             │
│  │ Attention Gate│  │ Accumulator  │  │ Adaptive     │             │
│  │ Region Router │  │ Event Buffer │  │ Homeostatic  │             │
│  └───────────────┘  └──────────────┘  └──────────────┘             │
│                                                                       │
│  ┌───────────────────────────────────────────────────┐               │
│  │               EVENT BUS (Pub/Sub)                 │               │
│  │  Cognitive Events → Dispatcher → Subscribers      │               │
│  └───────────────────────────────────────────────────┘               │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
                                  ▲
                                  │
                    ┌─────────────┴─────────────┐
                    │  NEURAL INFRASTRUCTURE    │
                    └─────────────┬─────────────┘
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   LOW-LEVEL NEURAL INFRASTRUCTURE                    │
│  (src/core/)                                                         │
├─────────────────────────────────────────────────────────────────────┤
│ Neurons  │ Synapses │ Plasticity │ Networks │ Brain    │ Integration│
│ (LIF,    │ (STDP,   │ (BCM,      │ (Spiking │ Regions  │ (Multimodal│
│ Izhike-  │ STP,     │ Hebbian,   │ Networks,│ (V1, A1, │ Sensory    │
│ vich,    │ Compu-   │ Oja)       │ Adaptive)│ PFC, HC) │ Fusion)    │
│ AdEx, HH)│ table)   │            │          │          │            │
│          │          │            │          │          │            │
│ Spike    │ Synaptic │ Weight     │ Network  │ Oscilla- │ Attention  │
│ Trains   │ Currents │ Updates    │ Output   │ tions    │ Mechanism  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Data Flow Pipeline

```
INPUT (Sensory Data)
  │
  ▼
┌─────────────────────────────────┐
│  Sensory Cortices               │
│  (V1 Visual, A1 Audio, STG)     │
└─────────────────────────────────┘
  │
  │ [Raw Features: pixels→edges, audio→spectral]
  ▼
┌─────────────────────────────────┐
│  Multimodal Integration         │
│  (Attention-weighted fusion)    │
└─────────────────────────────────┘
  │
  │ [Integrated Features]
  ▼
┌─────────────────────────────────┐
│  Neural Network                 │
│  (Spiking dynamics, plasticity) │
└─────────────────────────────────┘
  │
  │ [Spike Trains, Network Output]
  ▼
┌─────────────────────────────────────────────────────┐
│             MIDDLEWARE PROCESSING                   │
│  ┌───────────────────────────────────────────────┐  │
│  │ 1. ENCODING                                   │  │
│  │    Spikes → Firing Rates / Temporal Patterns  │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │ 2. FEATURE EXTRACTION                         │  │
│  │    Neural Activity → Feature Vectors          │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │ 3. PATTERN DETECTION                          │  │
│  │    Detect: Synchrony, Sequences, Phase Lock   │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │ 4. NORMALIZATION                              │  │
│  │    Z-Score, Min-Max, Adaptive Scaling         │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │ 5. ROUTING                                    │  │
│  │    Thalamic Router → Multiple Targets         │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
  │
  │ [Normalized Feature Vectors]
  ▼
┌─────────────────────────────────────────────────────┐
│         COGNITIVE MODULE DISTRIBUTION               │
├─────────────────────────────────────────────────────┤
│  ┌───────────┐  ┌──────────┐  ┌─────────┐          │
│  │ Working   │  │ Ethics   │  │ Salience│          │
│  │ Memory    │  │ Engine   │  │ Eval    │          │
│  └─────┬─────┘  └────┬─────┘  └────┬────┘          │
│        │             │             │                │
│        └─────────────┴─────────────┘                │
│                      │                              │
│                      ▼                              │
│            ┌──────────────────┐                     │
│            │   EVENT BUS      │                     │
│            │  (Pub/Sub)       │                     │
│            └──────────────────┘                     │
│                                                      │
└─────────────────────────────────────────────────────┘
  │
  │ [Decisions, Actions, State Updates]
  ▼
OUTPUT (Behavior, Memory, Ethical Constraints)
```

---

## Biological Mapping

```
BIOLOGICAL BRAIN                    NIMCP MIDDLEWARE
═══════════════════════════════════════════════════════════════

┌──────────────────────┐           ┌──────────────────────┐
│      THALAMUS        │    ←→     │  Thalamic Router     │
│  (Routing & Gating)  │           │  + Attention Gate    │
└──────────────────────┘           └──────────────────────┘
    • Routes sensory input              • Routes features
    • Gates cortical access             • Applies attention gating
    • Synchronizes oscillations         • Priority-based routing

┌──────────────────────┐           ┌──────────────────────┐
│   BASAL GANGLIA      │    ←→     │  Pattern Detector    │
│  (Action Selection)  │           │  (Sequence Detection)│
└──────────────────────┘           └──────────────────────┘
    • Winner-take-all                   • Detect patterns
    • Reinforcement learning            • Sequence matching
    • Pattern recognition               • Action selection

┌──────────────────────┐           ┌──────────────────────┐
│  ASSOCIATION CORTEX  │    ←→     │  Feature Extractor   │
│  (Feature Binding)   │           │  + Multimodal Integr.│
└──────────────────────┘           └──────────────────────┘
    • Multimodal integration            • Extract features
    • Feature abstraction               • Temporal integration
    • Hierarchical processing           • Population coding

┌──────────────────────┐           ┌──────────────────────┐
│    HIPPOCAMPUS       │    ←→     │  Sequence Detector   │
│ (Pattern Separation) │           │  + Pattern Separator │
└──────────────────────┘           └──────────────────────┘
    • Pattern separation                • Detect replay
    • Pattern completion                • Sequence patterns
    • Memory consolidation              • Temporal signatures

┌──────────────────────┐           ┌──────────────────────┐
│ PREFRONTAL CORTEX    │    ←→     │  Event Dispatcher    │
│ (Executive Control)  │           │  + Router Gating     │
└──────────────────────┘           └──────────────────────┘
    • Top-down attention                • Event coordination
    • Working memory gating             • Executive routing
    • Task switching                    • Priority management
```

---

## Component Interaction Diagram

```
┌────────────────────────────────────────────────────────────────┐
│                        BRAIN INSTANCE                          │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Neural Network                                                │
│  ┌──────────────────────────────────────────────────┐          │
│  │  Neurons  →  Synapses  →  Spike Trains           │          │
│  └──────────────────────────────────────────────────┘          │
│                          │                                     │
│                          ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              MIDDLEWARE PIPELINE                        │  │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐        │  │
│  │  │Extract │→ │Normalize│→│Detect  │→ │Route   │        │  │
│  │  └────────┘  └────────┘  └────────┘  └────────┘        │  │
│  └─────────────────────────────────────────────────────────┘  │
│                          │                                     │
│          ┌───────────────┼───────────────┐                    │
│          ▼               ▼               ▼                    │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐          │
│  │   Working    │ │    Ethics    │ │   Salience   │          │
│  │   Memory     │ │    Engine    │ │   Evaluator  │          │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘          │
│         │                │                │                   │
│         └────────────────┴────────────────┘                   │
│                          │                                     │
│                          ▼                                     │
│                  ┌───────────────┐                             │
│                  │  Event Bus    │                             │
│                  └───────────────┘                             │
│                          │                                     │
│                          ▼                                     │
│          ┌───────────────┴───────────────┐                    │
│          ▼                               ▼                    │
│  ┌──────────────┐              ┌──────────────┐               │
│  │ Consolidation│              │  Executive   │               │
│  │   Handler    │              │  Controller  │               │
│  └──────────────┘              └──────────────┘               │
│                                                                │
└────────────────────────────────────────────────────────────────┘

Event Flow:
1. Working Memory adds item → Publishes WORKING_MEMORY_ADD event
2. Event Bus dispatches to subscribers:
   - Executive Controller updates cognitive load
   - Mental Health Monitor checks stress
   - Consolidation Handler considers for long-term storage
3. All modules react asynchronously (loose coupling)
```

---

## Feature Extraction Example

```
Brain Region: Prefrontal Cortex (256 neurons)
Task: Extract features for ethics evaluation

┌────────────────────────────────────────┐
│   Prefrontal Cortex Neural Activity   │
│  (256 neurons, last 100ms)             │
├────────────────────────────────────────┤
│  Neuron 0:  ▁▃▁▅▁▃▁▁▁▅▁▃  (12 spikes)  │
│  Neuron 1:  ▁▁▁▁▁▃▁▅▁▁▁▁  (5 spikes)   │
│  Neuron 2:  ▃▅▃▅▃▅▃▁▁▁▁▁  (18 spikes)  │
│  ...                                   │
│  Neuron 255: ▁▁▁▁▁▁▁▁▁▁▁▁  (0 spikes)  │
└────────────────────────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────────────────────┐
│         MIDDLEWARE PROCESSING STAGES                   │
├────────────────────────────────────────────────────────┤
│                                                         │
│  STAGE 1: RATE ENCODING                                │
│  ┌──────────────────────────────────────────────────┐  │
│  │  For each neuron:                                │  │
│  │    rate[i] = (spikes in 100ms) / 0.1s            │  │
│  │  Result: [120, 50, 180, ..., 0] Hz               │  │
│  └──────────────────────────────────────────────────┘  │
│                  │                                      │
│                  ▼                                      │
│  STAGE 2: FEATURE EXTRACTION                           │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Feature 1: Mean Rate = 65 Hz                    │  │
│  │  Feature 2: Synchrony = 0.42                     │  │
│  │  Feature 3: Theta Power = 0.31                   │  │
│  │  Feature 4: Sparsity = 0.68                      │  │
│  │  → Vector: [65.0, 0.42, 0.31, 0.68]              │  │
│  └──────────────────────────────────────────────────┘  │
│                  │                                      │
│                  ▼                                      │
│  STAGE 3: NORMALIZATION                                │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Z-Score normalization:                          │  │
│  │    μ = [70, 0.5, 0.3, 0.7] (population stats)    │  │
│  │    σ = [15, 0.2, 0.1, 0.15]                      │  │
│  │    normalized[i] = (value[i] - μ[i]) / σ[i]      │  │
│  │  → Vector: [-0.33, -0.4, 0.1, -0.13]             │  │
│  └──────────────────────────────────────────────────┘  │
│                  │                                      │
└──────────────────┼──────────────────────────────────────┘
                   ▼
┌────────────────────────────────────────┐
│         OUTPUT FEATURE VECTOR          │
├────────────────────────────────────────┤
│  data: [-0.33, -0.4, 0.1, -0.13]       │
│  dim: 4                                │
│  timestamp_ms: 12345678                │
│  source: "REGION_PREFRONTAL_CORTEX"    │
└────────────────────────────────────────┘
                   │
                   ▼
┌────────────────────────────────────────┐
│         COGNITIVE MODULE               │
│         (Ethics Engine)                │
├────────────────────────────────────────┤
│  action_context_t ctx = {              │
│    .features = [-0.33, -0.4, 0.1, ...],│
│    .num_features = 4                   │
│  };                                    │
│  eval = ethics_evaluate(engine, &ctx); │
│                                        │
│  → Decision: ALLOW (confidence: 0.85)  │
└────────────────────────────────────────┘
```

---

## Event-Driven Architecture

```
TIME: t=0ms
═══════════════════════════════════════════════════════

Working Memory Module:
  └─→ Adds new item (visual features)
      └─→ Publishes: COGNITIVE_EVENT_WORKING_MEMORY_ADD
          ├─→ Event Data: {features, salience, timestamp}
          └─→ Source: "working_memory"

TIME: t=1ms
───────────────────────────────────────────────────────

Event Dispatcher:
  └─→ Receives event from Working Memory
      └─→ Looks up subscribers for WORKING_MEMORY_ADD:
          ├─→ Executive Controller (callback: on_wm_add)
          ├─→ Mental Health Monitor (callback: check_cognitive_load)
          └─→ Consolidation Handler (callback: consider_consolidation)

TIME: t=2ms
───────────────────────────────────────────────────────

Executive Controller:
  └─→ Callback: on_wm_add(event, exec_ctx)
      └─→ Updates cognitive load: 5/7 items
      └─→ Adjusts task switching priority
      └─→ Publishes: COGNITIVE_EVENT_ATTENTION_SHIFT

Mental Health Monitor:
  └─→ Callback: check_cognitive_load(event, monitor_ctx)
      └─→ Analyzes: 5/7 items (71% full)
      └─→ Assessment: Normal load, no intervention
      └─→ Updates stress metrics

Consolidation Handler:
  └─→ Callback: consider_consolidation(event, consol_ctx)
      └─→ Checks salience: 0.82 (high)
      └─→ Adds to consolidation queue
      └─→ Schedules: Next consolidation window

TIME: t=3ms
───────────────────────────────────────────────────────

Event Dispatcher:
  └─→ Processes COGNITIVE_EVENT_ATTENTION_SHIFT
      └─→ Subscribers:
          ├─→ Thalamic Router (adjust gates)
          └─→ Attention Mechanism (update focus)

TIME: t=5ms
───────────────────────────────────────────────────────

All events processed. System returns to steady state.

TOTAL LATENCY: 5ms
MODULES INVOLVED: 5 (WM, Dispatcher, Executive, MH Monitor, Consolidation)
EVENTS GENERATED: 2 (WM_ADD, ATTENTION_SHIFT)
COUPLING: Loose (modules don't know about each other)
```

---

## Performance Optimization - Memory Layout

```
BAD: Array of Structs (Poor Cache Locality)
═══════════════════════════════════════════

typedef struct {
    float rate;       // 4 bytes
    float synchrony;  // 4 bytes
    float oscillation;// 4 bytes
    float entropy;    // 4 bytes
} neuron_features_t; // 16 bytes per neuron

neuron_features_t features[1000];

Memory Layout:
[rate₀|sync₀|osc₀|ent₀][rate₁|sync₁|osc₁|ent₁]...

Accessing all rates:
  Cache Miss! → [rate₀] → Skip 12 bytes → [rate₁] → Skip 12 bytes...
  Only 25% cache utilization

SIMD: Cannot vectorize (non-contiguous data)


GOOD: Struct of Arrays (SIMD-Friendly)
═══════════════════════════════════════

typedef struct {
    float* rates;       // Contiguous array [1000]
    float* synchrony;   // Contiguous array [1000]
    float* oscillation; // Contiguous array [1000]
    float* entropy;     // Contiguous array [1000]
    uint32_t num_neurons;
} population_features_t;

Memory Layout:
Rates:       [rate₀|rate₁|rate₂|rate₃][rate₄|rate₅|rate₆|rate₇]...
Synchrony:   [sync₀|sync₁|sync₂|sync₃][sync₄|sync₅|sync₆|sync₇]...
Oscillation: [osc₀|osc₁|osc₂|osc₃][osc₄|osc₅|osc₆|osc₇]...

Accessing all rates:
  Cache Hit! → Load 4 floats at once → Process → Next 4 floats
  100% cache utilization

SIMD Processing:
  __m128 rates_vec = _mm_load_ps(&features.rates[i]);
  // Process 4 rates simultaneously
  4× speedup!
```

---

## Middleware Pipeline Composition

```
SCENARIO: Process visual input for ethics evaluation
══════════════════════════════════════════════════════

Pipeline Configuration:
┌────────────────────────────────────────────────────────┐
│                   MIDDLEWARE PIPELINE                  │
├────────────────────────────────────────────────────────┤
│                                                         │
│  Stage 1: Feature Extraction                           │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Type: FEATURE_FIRING_RATE + FEATURE_SYNCHRONY  │  │
│  │  Window: 100ms                                   │  │
│  │  Input: Visual cortex neurons (128)             │  │
│  │  Output: [rate₀...rate₁₂₇, sync] (129 features) │  │
│  └──────────────────────────────────────────────────┘  │
│                       │                                 │
│                       ▼                                 │
│  Stage 2: Normalization                                │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Type: NORM_ZSCORE                               │  │
│  │  Clip: [-3, +3]                                  │  │
│  │  Input: 129 raw features                         │  │
│  │  Output: 129 normalized features                 │  │
│  └──────────────────────────────────────────────────┘  │
│                       │                                 │
│                       ▼                                 │
│  Stage 3: Pattern Detection                            │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Type: PATTERN_SYNCHRONY                         │  │
│  │  Threshold: 0.7 (high synchrony)                 │  │
│  │  Input: Normalized features                      │  │
│  │  Output: Features + [synchrony_detected] flag    │  │
│  └──────────────────────────────────────────────────┘  │
│                       │                                 │
│                       ▼                                 │
│  Stage 4: Routing                                      │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Type: Thalamic Router                           │  │
│  │  Targets:                                        │  │
│  │    - ROUTE_TARGET_ETHICS (priority: 1.0)         │  │
│  │    - ROUTE_TARGET_WORKING_MEMORY (priority: 0.8) │  │
│  │    - ROUTE_TARGET_SALIENCE (priority: 0.6)       │  │
│  │  Gate: Prefrontal control (strength: 0.9)        │  │
│  └──────────────────────────────────────────────────┘  │
│                                                         │
└─────────────────────────────────────────────────────────┘

Execution:
  Input: Visual cortex activity (128 neurons, 100ms window)
    ↓
  [Extract] → 129 features (rates + synchrony)
    ↓
  [Normalize] → Z-score normalized [-3, +3]
    ↓
  [Detect] → Synchrony detected: 0.75 (above threshold)
    ↓
  [Route] → Dispatched to:
              ├─→ Ethics Engine (priority 1.0, gate 0.9) ✓ ROUTED
              ├─→ Working Memory (priority 0.8, gate 0.9) ✓ ROUTED
              └─→ Salience Eval (priority 0.6, gate 0.9) ~ GATED (50%)

Output: Features successfully delivered to 2.5 targets (weighted)

Benefits:
  ✓ Modular: Can rearrange stages
  ✓ Reusable: Same pipeline for audio, speech
  ✓ Testable: Mock each stage independently
  ✓ Extensible: Add new stages without changing code
```

---

## Thread Safety & Concurrency

```
SCENARIO: Multiple threads using middleware
═══════════════════════════════════════════

Thread 1: Visual Processing
Thread 2: Audio Processing
Thread 3: Working Memory Update

┌─────────────────────────────────────────────────────┐
│                   SHARED RESOURCES                  │
│  (Require Synchronization)                          │
├─────────────────────────────────────────────────────┤
│                                                      │
│  Event Dispatcher                                   │
│  ┌────────────────────────────────────────────────┐ │
│  │  Mutex: dispatcher_mutex                       │ │
│  │  Subscribers List (shared)                     │ │
│  │  Event Queue (shared)                          │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  Thalamic Router                                    │
│  ┌────────────────────────────────────────────────┐ │
│  │  Mutex: router_mutex                           │ │
│  │  Route Table (shared)                          │ │
│  │  Gate States (atomic floats)                   │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  Feature Pool (Pre-allocated Buffers)              │
│  ┌────────────────────────────────────────────────┐ │
│  │  Mutex: pool_mutex                             │ │
│  │  Available Buffers (thread-safe queue)         │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                THREAD-LOCAL RESOURCES               │
│  (No Synchronization Needed)                        │
├─────────────────────────────────────────────────────┤
│                                                      │
│  Thread 1: Visual Processing                        │
│  ┌────────────────────────────────────────────────┐ │
│  │  feature_extractor_t extractor_visual          │ │
│  │  rate_coder_t coder_visual                     │ │
│  │  signal_normalizer_t normalizer_visual         │ │
│  │  (All independent, no sharing)                 │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  Thread 2: Audio Processing                         │
│  ┌────────────────────────────────────────────────┐ │
│  │  feature_extractor_t extractor_audio           │ │
│  │  rate_coder_t coder_audio                      │ │
│  │  signal_normalizer_t normalizer_audio          │ │
│  │  (All independent, no sharing)                 │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
└─────────────────────────────────────────────────────┘

Synchronization Points:

1. Event Publishing (Thread 1 publishes event)
   ┌──────────────────────────────┐
   │ lock(dispatcher_mutex)       │
   │ add_to_event_queue(event)    │
   │ unlock(dispatcher_mutex)     │
   └──────────────────────────────┘

2. Routing (Threads 1, 2 route simultaneously)
   ┌──────────────────────────────┐
   │ lock(router_mutex)           │
   │ route_to_targets(features)   │
   │ unlock(router_mutex)         │
   └──────────────────────────────┘

3. Buffer Allocation (All threads need buffers)
   ┌──────────────────────────────┐
   │ lock(pool_mutex)             │
   │ buffer = pool_acquire()      │
   │ unlock(pool_mutex)           │
   │ // Use buffer (no lock)      │
   │ lock(pool_mutex)             │
   │ pool_release(buffer)         │
   │ unlock(pool_mutex)           │
   └──────────────────────────────┘

Performance:
  ✓ Lock-free: Feature extraction, encoding, normalization
  ✓ Coarse-grained locks: Event dispatcher, router (low contention)
  ✓ Pre-allocated buffers: Minimize allocation overhead
  ✓ Atomic operations: Gate states (high-frequency updates)
```

---

## Summary

These diagrams illustrate:

1. **Layered Architecture**: Clear separation between infrastructure, middleware, and cognition
2. **Data Flow**: How information flows through the middleware pipeline
3. **Biological Mapping**: Correspondence between brain structures and middleware components
4. **Component Interaction**: How modules communicate via events and routing
5. **Feature Extraction**: Detailed example of spike trains → feature vectors
6. **Event-Driven Architecture**: Asynchronous, loosely-coupled communication
7. **Performance Optimization**: Memory layout for cache efficiency and SIMD
8. **Pipeline Composition**: How to chain middleware stages
9. **Thread Safety**: Concurrency patterns and synchronization

**See Also:**
- MIDDLEWARE_ARCHITECTURE.md - Complete design specification
- MIDDLEWARE_API_REFERENCE.md - Detailed API documentation

---

**End of Diagrams**

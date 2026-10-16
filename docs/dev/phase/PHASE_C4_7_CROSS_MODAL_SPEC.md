# Phase C4.7: Cross-Modal Information Flow - Specification

**Status**: Ready for Implementation
**Priority**: High
**Estimated LOC**: 2,500+ lines

---

## Executive Summary

Phase C4.7 extends Shannon information theory to track information flow **between different sensory modalities** (visual, audio, speech) and cognitive systems. This enables:

1. **Cross-modal bottleneck detection** (where information is lost between modalities)
2. **Multi-sensory integration optimization** (McGurk effect, audiovisual speech)
3. **Modality transfer efficiency** (how much information survives the transfer)
4. **Cross-modal routing optimization** (adaptive pathways between cortices)

---

## Biological Motivation

### Multisensory Integration in the Brain

**Key Brain Regions**:
1. **Superior Colliculus** - Visual + auditory integration for orienting
2. **Superior Temporal Sulcus (STS)** - Audiovisual speech integration
3. **Posterior Parietal Cortex** - Visuospatial + motor integration
4. **Prefrontal Cortex** - All modalities → decision making

**Clinical Examples**:
- **McGurk Effect**: Visual /ga/ + Audio /ba/ → Perceived /da/ (cross-modal integration)
- **Audiovisual Speech**: Seeing lips improves speech comprehension in noise (30% boost)
- **Synesthesia**: Cross-modal leakage (see sounds, hear colors)
- **Autism**: Reduced cross-modal integration (impaired social communication)

**References**:
- Stein & Stanford (2008) - "Multisensory integration"
- Calvert & Thesen (2004) - "Multisensory integration: methodological approaches"
- Driver & Noesselt (2008) - "Multisensory interplay reveals crossmodal influences"

---

## Architecture

### Current NIMCP Modalities

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  Visual Cortex   │     │  Audio Cortex    │     │  Speech Cortex   │
│  (V1/V2/V4/IT)   │     │  (A1/STG)        │     │  (Wernicke/Broca)│
│                  │     │                  │     │                  │
│  - Gabor filters │     │  - FFT           │     │  - Phonemes      │
│  - CNN layers    │     │  - Mel filters   │     │  - Formants      │
│  - Attention     │     │  - MFCC          │     │  - Lexicon       │
│  - Memory        │     │  - Temporal      │     │  - Prosody       │
└──────────────────┘     └──────────────────┘     └──────────────────┘
         │                        │                        │
         └────────────┬───────────┴───────────┬───────────┘
                      │                       │
              ┌───────▼───────────────────────▼──────┐
              │  Phase C4.7: CROSS-MODAL LAYER      │
              │  - Information flow tracking        │
              │  - Bottleneck detection            │
              │  - Transfer efficiency             │
              │  - Routing optimization            │
              └───────┬──────────────────────────────┘
                      │
              ┌───────▼───────────────────────────────┐
              │  BRAIN INTEGRATION                    │
              │  - Hippocampus (memory)              │
              │  - Prefrontal Cortex (decision)      │
              │  - Working Memory (buffer)           │
              │  - Attention System (selection)      │
              └──────────────────────────────────────┘
```

---

## Core Concepts

### 1. Cross-Modal Channel

A **cross-modal channel** is a pathway from one sensory cortex to another:

```
Visual Cortex (features_v) → [Channel] → Audio Cortex (features_a)
```

**Shannon metrics**:
- **Source Entropy**: H(V) - Information in visual features
- **Destination Entropy**: H(A) - Information in audio features
- **Mutual Information**: I(V;A) - Shared information
- **Transfer Efficiency**: I(V;A) / H(V) - How much visual info reaches audio
- **Channel Capacity**: C_va - Maximum bits/sec from V → A

### 2. Multi-Modal Integration

Multiple modalities converge on a single target:

```
Visual (V) ────┐
               ├──→ [Integration] → Hippocampus (H)
Audio (A) ─────┤
               └─→ I(V,A;H) = Mutual information of both with H
Speech (S) ────┘
```

**Shannon metrics**:
- **Joint Entropy**: H(V,A,S) - Total information from all modalities
- **Integration Information**: I(V;A) + I(V;S) + I(A;S) - Redundancy
- **Synergy**: Information that only exists when modalities combine

### 3. Cross-Modal Bottleneck

A bottleneck occurs when:
- **Transfer efficiency < threshold** (e.g., <50%)
- **Mutual information << source entropy** (lots of information lost)
- **Channel capacity insufficient** for information rate

Example: Visual cortex produces 100 bits/sec, but only 30 bits/sec reach speech cortex.

---

## API Design

### Cross-Modal Channel Structure

```c
/**
 * @brief Cross-modal information flow channel
 *
 * WHAT: Track information flow between two modalities
 * WHY:  Detect bottlenecks, measure transfer efficiency
 * HOW:  Compare source/destination entropy, compute mutual information
 */
typedef struct {
    const char* source_modality;      ///< Source cortex (e.g., "visual")
    const char* dest_modality;        ///< Destination cortex (e.g., "audio")

    // Shannon metrics
    float source_entropy;             ///< H(source) in bits
    float dest_entropy;               ///< H(dest) in bits
    float mutual_information;         ///< I(source;dest) in bits
    float transfer_efficiency;        ///< I(source;dest) / H(source)
    float channel_capacity;           ///< Maximum bits/sec
    float information_rate;           ///< Actual bits/sec

    // Bottleneck detection
    bool is_bottleneck;               ///< True if efficiency < threshold
    float bottleneck_severity;        ///< 0-1, higher = more severe

    // Timing
    uint64_t timestamp_ms;            ///< When metrics were computed
    uint32_t sample_count;            ///< Number of samples analyzed
} cross_modal_channel_t;

/**
 * @brief Multi-modal integration metrics
 *
 * WHAT: Measure how well multiple modalities integrate
 * WHY:  Optimize multi-sensory perception (McGurk effect, etc.)
 * HOW:  Compute joint entropy, redundancy, synergy
 */
typedef struct {
    uint32_t num_modalities;          ///< Number of input modalities (2-4)
    const char* modality_names[4];    ///< Names of modalities

    // Individual entropies
    float individual_entropy[4];      ///< H(M_i) for each modality

    // Joint metrics
    float joint_entropy;              ///< H(M1, M2, ..., Mn)
    float total_mutual_info;          ///< Sum of pairwise I(M_i;M_j)
    float redundancy;                 ///< Overlap between modalities
    float synergy;                    ///< Information only in combination

    // Integration efficiency
    float integration_efficiency;     ///< joint / sum(individual)
    bool is_integrating_well;         ///< True if efficiency > threshold
} multi_modal_integration_t;

/**
 * @brief Cross-modal routing graph
 *
 * WHAT: Track all cross-modal pathways in brain
 * WHY:  Find optimal routes, detect global bottlenecks
 * HOW:  Graph of modalities → Shannon metrics on edges
 */
typedef struct {
    uint32_t num_modalities;          ///< Number of cortices
    const char* modality_names[10];   ///< Cortex names

    // Adjacency matrix of channels
    cross_modal_channel_t* channels[10][10];  ///< [source][dest]

    // Global metrics
    float total_capacity;             ///< Sum of all channel capacities
    float total_information_rate;     ///< Actual info flow rate
    float network_efficiency;         ///< rate / capacity
    uint32_t num_bottlenecks;         ///< Count of bottleneck channels

    // Optimization guidance
    uint32_t bottleneck_source_ids[10];     ///< Which sources are bottlenecked
    uint32_t bottleneck_dest_ids[10];       ///< Which dests are bottlenecked
} cross_modal_routing_graph_t;
```

---

## API Functions

### Channel Analysis

```c
/**
 * @brief Analyze cross-modal information transfer
 *
 * WHAT: Compute Shannon metrics for modality transfer
 * WHY:  Detect bottlenecks between cortices
 * HOW:  Sample features from both, compute H(source), H(dest), I(source;dest)
 *
 * @param source_features Source modality features
 * @param source_dim Source feature dimensionality
 * @param dest_features Destination modality features
 * @param dest_dim Destination feature dimensionality
 * @param num_samples Number of samples to analyze
 * @param config Shannon configuration
 * @return Cross-modal channel metrics
 *
 * COMPLEXITY: O(N × D) where N=samples, D=dimensions
 */
cross_modal_channel_t cross_modal_analyze_channel(
    const float* source_features,
    uint32_t source_dim,
    const float* dest_features,
    uint32_t dest_dim,
    uint32_t num_samples,
    const shannon_config_t* config
);

/**
 * @brief Detect if channel is a bottleneck
 *
 * WHAT: Check if transfer efficiency is below threshold
 * WHY:  Identify problematic cross-modal pathways
 * HOW:  Compare efficiency against config threshold
 *
 * @param channel Cross-modal channel
 * @param efficiency_threshold Minimum acceptable efficiency (default: 0.5)
 * @return true if bottleneck detected
 */
bool cross_modal_is_bottleneck(
    const cross_modal_channel_t* channel,
    float efficiency_threshold
);
```

### Multi-Modal Integration

```c
/**
 * @brief Analyze multi-modal integration
 *
 * WHAT: Measure how well multiple modalities combine
 * WHY:  Optimize multi-sensory perception
 * HOW:  Compute joint entropy H(M1,M2,...), redundancy, synergy
 *
 * @param features Array of feature pointers (one per modality)
 * @param dims Array of feature dimensions
 * @param num_modalities Number of modalities (2-4)
 * @param num_samples Number of samples per modality
 * @param modality_names Names of modalities (for debugging)
 * @param config Shannon configuration
 * @return Multi-modal integration metrics
 *
 * COMPLEXITY: O(2^M × N × D) where M=modalities, N=samples, D=dimensions
 * NOTE: Exponential in num_modalities, limit to 2-4
 */
multi_modal_integration_t cross_modal_analyze_integration(
    const float** features,
    const uint32_t* dims,
    uint32_t num_modalities,
    uint32_t num_samples,
    const char** modality_names,
    const shannon_config_t* config
);

/**
 * @brief Compute synergy between modalities
 *
 * WHAT: Information that only exists when modalities combine
 * WHY:  Understand emergent multi-sensory properties (McGurk effect)
 * HOW:  Synergy = H(M1,M2) - H(M1) - H(M2) + I(M1;M2)
 *
 * @param integration Multi-modal integration metrics
 * @return Synergy in bits (can be positive or negative)
 *
 * BIOLOGICAL: Synergy explains why seeing lips helps hearing speech
 */
float cross_modal_compute_synergy(const multi_modal_integration_t* integration);
```

### Routing Graph

```c
/**
 * @brief Create cross-modal routing graph
 *
 * WHAT: Initialize graph of all cross-modal pathways
 * WHY:  Track global information flow across brain
 * HOW:  Allocate adjacency matrix, initialize metrics
 *
 * @param modality_names Names of cortices (e.g., "visual", "audio", "speech")
 * @param num_modalities Number of cortices
 * @return Routing graph or NULL on failure
 */
cross_modal_routing_graph_t* cross_modal_create_routing_graph(
    const char** modality_names,
    uint32_t num_modalities
);

/**
 * @brief Update routing graph with new channel data
 *
 * WHAT: Add/update channel metrics in graph
 * WHY:  Keep routing graph current with latest measurements
 * HOW:  Store channel in adjacency matrix [source][dest]
 *
 * @param graph Routing graph
 * @param source_id Source modality index
 * @param dest_id Destination modality index
 * @param channel Channel metrics
 * @return true on success
 */
bool cross_modal_update_routing_graph(
    cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    const cross_modal_channel_t* channel
);

/**
 * @brief Detect all bottlenecks in routing graph
 *
 * WHAT: Find all channels with low transfer efficiency
 * WHY:  Identify global cross-modal problems
 * HOW:  Scan adjacency matrix, collect bottlenecks
 *
 * @param graph Routing graph
 * @param bottlenecks Output array of bottleneck channels
 * @param max_bottlenecks Maximum bottlenecks to return
 * @param num_bottlenecks Number of bottlenecks found
 * @return true on success
 */
bool cross_modal_detect_bottlenecks(
    const cross_modal_routing_graph_t* graph,
    cross_modal_channel_t* bottlenecks,
    uint32_t max_bottlenecks,
    uint32_t* num_bottlenecks
);

/**
 * @brief Find optimal route between modalities
 *
 * WHAT: Find highest-capacity path from source to dest
 * WHY:  Route information around bottlenecks
 * HOW:  Dijkstra's algorithm with capacity as weight
 *
 * @param graph Routing graph
 * @param source_id Source modality
 * @param dest_id Destination modality
 * @param path Output path (array of modality IDs)
 * @param max_path_length Maximum path length
 * @param path_length Actual path length
 * @return Total capacity of path, or 0 if no path
 *
 * COMPLEXITY: O(V² log V) where V = num_modalities
 */
float cross_modal_find_optimal_route(
    const cross_modal_routing_graph_t* graph,
    uint32_t source_id,
    uint32_t dest_id,
    uint32_t* path,
    uint32_t max_path_length,
    uint32_t* path_length
);

/**
 * @brief Destroy routing graph
 */
void cross_modal_destroy_routing_graph(cross_modal_routing_graph_t* graph);
```

---

## Brain Integration

### Brain Structure Extensions

```c
// In nimcp_brain.h / nimcp_brain.c

typedef struct {
    // Phase C4.7: Cross-Modal Information Flow
    bool enable_cross_modal_monitoring;
    cross_modal_routing_graph_t* cross_modal_graph;

    // Cross-modal configuration
    float cross_modal_bottleneck_threshold;  // Default: 0.5 (50% efficiency)
    uint32_t cross_modal_sample_size;        // Default: 100 samples

    // Last metrics
    multi_modal_integration_t last_integration_metrics;
} brain_struct;
```

### Public API

```c
/**
 * @brief Enable cross-modal information flow monitoring
 *
 * WHAT: Turn on cross-modal Shannon analysis
 * WHY:  Track information transfer between modalities
 * HOW:  Initialize routing graph, enable sampling
 *
 * @param brain Brain instance
 * @param enable true to enable, false to disable
 */
void brain_enable_cross_modal_monitoring(brain_t brain, bool enable);

/**
 * @brief Get cross-modal routing graph
 *
 * WHAT: Access current cross-modal metrics
 * WHY:  Inspect bottlenecks, plan optimization
 * HOW:  Return pointer to routing graph
 *
 * @param brain Brain instance
 * @return Routing graph or NULL if disabled
 */
const cross_modal_routing_graph_t* brain_get_cross_modal_graph(brain_t brain);

/**
 * @brief Get multi-modal integration metrics
 *
 * WHAT: How well are modalities integrating?
 * WHY:  Optimize multi-sensory perception
 * HOW:  Return last computed integration metrics
 *
 * @param brain Brain instance
 * @param metrics Output integration metrics
 * @return true on success
 */
bool brain_get_multi_modal_integration(
    brain_t brain,
    multi_modal_integration_t* metrics
);
```

---

## Example Use Cases

### Use Case 1: Audiovisual Speech (McGurk Effect)

```c
// Visual cortex processes lip movements
float visual_features[128];
visual_cortex_process(visual_cortex, lip_video, 640, 480, 1, visual_features);

// Audio cortex processes speech audio
float audio_features[128];
audio_cortex_process(audio_cortex, speech_audio, num_samples, 1, audio_features);

// Analyze cross-modal transfer: Visual → Speech
cross_modal_channel_t v2s_channel = cross_modal_analyze_channel(
    visual_features, 128,
    audio_features, 128,
    100,  // 100 samples
    &shannon_config
);

printf("Visual → Speech Transfer:\n");
printf("  Source Entropy: %.2f bits\n", v2s_channel.source_entropy);
printf("  Mutual Info: %.2f bits\n", v2s_channel.mutual_information);
printf("  Efficiency: %.2f%%\n", v2s_channel.transfer_efficiency * 100.0f);

if (cross_modal_is_bottleneck(&v2s_channel, 0.5f)) {
    printf("  ⚠️ BOTTLENECK DETECTED! Visual info not reaching speech cortex.\n");
}
```

### Use Case 2: Multi-Sensory Integration

```c
// Collect features from 3 modalities
const float* features[3] = {visual_features, audio_features, speech_features};
const uint32_t dims[3] = {128, 128, 64};
const char* names[3] = {"visual", "audio", "speech"};

// Analyze integration
multi_modal_integration_t integration = cross_modal_analyze_integration(
    features, dims, 3, 100, names, &shannon_config
);

printf("Multi-Modal Integration:\n");
printf("  Joint Entropy: %.2f bits\n", integration.joint_entropy);
printf("  Redundancy: %.2f bits\n", integration.redundancy);
printf("  Synergy: %.2f bits\n", cross_modal_compute_synergy(&integration));
printf("  Efficiency: %.2f%%\n", integration.integration_efficiency * 100.0f);

if (integration.is_integrating_well) {
    printf("  ✅ Modalities integrating well\n");
} else {
    printf("  ⚠️ Poor integration - modalities not cooperating\n");
}
```

### Use Case 3: Routing Optimization

```c
// Create routing graph
const char* modalities[4] = {"visual", "audio", "speech", "hippocampus"};
cross_modal_routing_graph_t* graph = cross_modal_create_routing_graph(modalities, 4);

// Update graph with measurements
cross_modal_update_routing_graph(graph, 0, 1, &v2a_channel);  // visual → audio
cross_modal_update_routing_graph(graph, 0, 2, &v2s_channel);  // visual → speech
cross_modal_update_routing_graph(graph, 1, 2, &a2s_channel);  // audio → speech
cross_modal_update_routing_graph(graph, 2, 3, &s2h_channel);  // speech → hippocampus

// Detect bottlenecks
cross_modal_channel_t bottlenecks[10];
uint32_t num_bottlenecks;
cross_modal_detect_bottlenecks(graph, bottlenecks, 10, &num_bottlenecks);

printf("Detected %u bottlenecks:\n", num_bottlenecks);
for (uint32_t i = 0; i < num_bottlenecks; i++) {
    printf("  %s → %s: %.2f%% efficient (severity: %.2f)\n",
           bottlenecks[i].source_modality,
           bottlenecks[i].dest_modality,
           bottlenecks[i].transfer_efficiency * 100.0f,
           bottlenecks[i].bottleneck_severity);
}

// Find optimal route: visual → hippocampus
uint32_t path[4];
uint32_t path_length;
float capacity = cross_modal_find_optimal_route(graph, 0, 3, path, 4, &path_length);

printf("Optimal route (visual → hippocampus): ");
for (uint32_t i = 0; i < path_length; i++) {
    printf("%s%s", modalities[path[i]], i < path_length-1 ? " → " : "\n");
}
printf("Total capacity: %.2f bits/sec\n", capacity);

cross_modal_destroy_routing_graph(graph);
```

---

## Implementation Plan

### Phase 1: Core Data Structures & Channel Analysis
- `src/include/information/nimcp_cross_modal.h` (~400 lines)
- `src/information/nimcp_cross_modal.c` (~800 lines)
- Implement `cross_modal_analyze_channel()`
- Implement `cross_modal_is_bottleneck()`

### Phase 2: Multi-Modal Integration
- Implement `cross_modal_analyze_integration()`
- Implement `cross_modal_compute_synergy()`
- Handle 2-4 modalities efficiently

### Phase 3: Routing Graph
- Implement `cross_modal_create_routing_graph()`
- Implement `cross_modal_update_routing_graph()`
- Implement `cross_modal_detect_bottlenecks()`
- Implement `cross_modal_find_optimal_route()` (Dijkstra)

### Phase 4: Brain Integration
- Modify `brain_struct` to add cross-modal fields
- Implement `brain_enable_cross_modal_monitoring()`
- Implement `brain_get_cross_modal_graph()`
- Implement `brain_get_multi_modal_integration()`

### Phase 5: Test Suite
- Unit tests: 40+ tests
- Integration tests: 15+ tests
- Regression tests: 20+ tests

---

## Success Criteria

1. ✅ All API functions implemented
2. ✅ 100% test coverage (unit + integration + regression)
3. ✅ Backward compatible (opt-in, no breaking changes)
4. ✅ Brain integration complete
5. ✅ Documentation comprehensive
6. ✅ NIMCP standards compliant

---

## Expected Outcomes

1. **Cross-Modal Bottleneck Detection**: Identify where information is lost
2. **Multi-Sensory Optimization**: Improve integration efficiency
3. **Adaptive Routing**: Route information around bottlenecks
4. **Scientific Insights**: Understand McGurk effect, synesthesia, autism

---

**Ready for Implementation** 🚀

---

**NIMCP Phase C4.7 Specification**
**2025-11-14**

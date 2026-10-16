# NIMCP 2.7 Phase 3: Advanced NLP with Attention & Neuromodulation

## Vision

Phase 3 combines attention mechanisms, spike-based processing, and neuromodulation to create a biologically-inspired NLP system that processes language through actual neural dynamics.

## Architecture Overview

```
Input (Word Embeddings)
        ↓
    [Attention Layer]
    Query-Key-Value synapses
    Compute attention weights
        ↓
  [Fractal Network]
  500 neurons, scale-free
  Hub neurons = concepts
        ↓
   [Neuromodulation]
   Dopamine + Pink Noise
   Context-dependent learning
        ↓
   Output (Semantics)
```

## Phase 3 Features

### 1. Attention-Based Synapse Computation

**What**: Implement transformer-style attention at the synapse level using programmable synapses

**Why**:
- Allows network to focus on relevant words in a sentence
- Each synapse can compute query-key similarity
- Biologically plausible (attention modulates synaptic transmission)

**How**:
```c
// Synapse compute function for attention
float synapse_compute_attention(
    synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    synapse_compute_context_t* context
) {
    // Extract query and key from neuron states
    float query = post_neuron->state;
    float key = pre_neuron->state;

    // Compute attention weight: softmax(QK^T / sqrt(d))
    float attention_score = query * key / sqrtf(EMBEDDING_DIM);
    float attention_weight = expf(attention_score) / context->attention_normalizer;

    // Modulate transmission
    return syn->weight * attention_weight * pre_activity;
}
```

**Implementation**:
- Create `nimcp_attention_synapses.h/c` in `src/core/synapse_compute/`
- Define attention compute function and context structures
- Integrate with network builder for easy configuration

### 2. Spike-Based NLP Processing

**What**: Process sentences through actual spike dynamics instead of simulation

**Why**:
- More biologically realistic
- Temporal dynamics capture word order
- Natural integration with STDP learning

**How**:
```c
// Convert word embedding to input spikes
void embed_to_spikes(
    const float* embedding,
    uint32_t dim,
    neuron_t* input_neurons,
    uint32_t num_neurons,
    uint64_t timestamp
) {
    // Map each embedding dimension to neuron firing rate
    for (uint32_t i = 0; i < dim && i < num_neurons; i++) {
        // Positive values → higher firing rate
        float rate = fmaxf(0.0f, embedding[i]);
        input_neurons[i].state += rate;

        // Trigger spike if above threshold
        if (input_neurons[i].state > input_neurons[i].threshold) {
            neural_network_record_spike(...);
        }
    }
}

// Process sentence word-by-word
void process_sentence_spikes(
    neural_network_t network,
    const word_embedding_t* embeddings,
    const uint32_t* word_indices,
    uint32_t sentence_len
) {
    for (uint32_t i = 0; i < sentence_len; i++) {
        // Inject word as spikes
        embed_to_spikes(...);

        // Let network dynamics settle (100ms per word)
        for (uint64_t t = 0; t < 100; t++) {
            neural_network_compute_step(network, t);
        }
    }
}
```

**Implementation**:
- Create `nimcp_spike_nlp.h/c` in `src/nlp/`
- Implement embedding-to-spike conversion
- Add sentence processing with temporal dynamics

### 3. Neuromodulation Integration with Pink Noise

**What**: Integrate pink noise into neuromodulator system for contextual learning

**Why**:
- Pink noise provides multi-timescale exploration
- Neuromodulators (dopamine, serotonin) gate learning
- Together: biologically realistic contextual learning

**How**:
```c
// Pink noise modulated neuromodulator
typedef struct {
    float dopamine_baseline;      // Base dopamine level
    float dopamine_current;        // Current level
    pink_noise_generator_t noise; // Pink noise for fluctuations
    float noise_amplitude;         // Amount of noise to add
} neuromod_pink_noise_t;

// Update neuromodulator with pink noise
void neuromod_update_with_pink_noise(
    neuromod_pink_noise_t* mod,
    float reward_signal  // External reward/error signal
) {
    // Generate pink noise sample
    float noise_sample;
    pink_noise_generate(mod->noise, &noise_sample, 1);

    // Update dopamine: baseline + reward + noise
    mod->dopamine_current =
        mod->dopamine_baseline +
        reward_signal * 0.5f +  // Reward influence
        noise_sample * mod->noise_amplitude;  // Pink noise exploration

    // Clamp to [0, 1]
    mod->dopamine_current = fminf(1.0f, fmaxf(0.0f, mod->dopamine_current));
}

// Use in synapse learning
void synapse_learn_modulated(
    synapse_t* syn,
    float dopamine,
    float pre_spike_time,
    float post_spike_time
) {
    // STDP with dopamine modulation
    float dt = post_spike_time - pre_spike_time;
    float stdp_change = compute_stdp(dt);

    // Modulate learning by dopamine
    syn->weight += dopamine * stdp_change * LEARNING_RATE;
}
```

**Implementation**:
- Extend `src/plasticity/neuromodulators/nimcp_neuromodulators.h/c`
- Add pink noise generator field to neuromodulator state
- Implement modulated learning functions

### 4. Comprehensive Demo: Sentiment Analysis

**What**: Full working demo that classifies sentence sentiment using all Phase 3 features

**Architecture**:
```
Input: "The movie was great!"
  ↓
[Word Embeddings]
  the → [0.1, 0.2, ...]
  movie → [0.3, 0.1, ...]
  was → [0.05, 0.15, ...]
  great → [0.8, 0.9, ...]  ← High positive embedding
  ↓
[Attention Layer]
  Query from "great" focuses on "movie"
  Attention weights: [0.1, 0.6, 0.1, 0.2]
  ↓
[Fractal Network Processing]
  Spikes propagate through hub neurons
  Hub A (positive concepts) activates strongly
  Hub B (negative concepts) remains quiet
  ↓
[Neuromodulation]
  Dopamine + pink noise modulates learning
  Strengthens "great → positive" pathways
  ↓
Output: Positive sentiment (confidence: 0.87)
```

**Example Code**:
```c
// Create network with attention synapses
network_builder_config_t config = network_builder_default();
config.num_neurons = 500;
config.use_topology = true;
config.topology_config.type = TOPOLOGY_SCALE_FREE;
config.use_attention_synapses = true;  // NEW
config.use_neuromodulation = true;     // NEW
config.neuromod_pink_noise_amplitude = 0.1f;  // NEW

neural_network_t net = network_builder_build(&config);

// Process sentence with attention + spikes + neuromodulation
sentiment_result_t result = process_sentiment_sentence(
    net,
    embeddings,
    sentence,
    sentence_len
);

printf("Sentiment: %s (%.2f)\n",
    result.sentiment == POSITIVE ? "Positive" : "Negative",
    result.confidence);
```

## Implementation Plan

### Step 1: Attention Synapses (Priority: HIGH)
**Files**: `src/core/synapse_compute/nimcp_attention_synapses.h/c`
**Effort**: 2-3 hours
**Dependencies**: None (uses existing synapse_compute infrastructure)

### Step 2: Spike-Based NLP Processing (Priority: HIGH)
**Files**: `src/nlp/nimcp_spike_nlp.h/c`
**Effort**: 2-3 hours
**Dependencies**: Core neural network

### Step 3: Pink Noise Neuromodulation (Priority: MEDIUM)
**Files**: Extend `src/plasticity/neuromodulators/nimcp_neuromodulators.h/c`
**Effort**: 1-2 hours
**Dependencies**: Pink noise generator, neuromodulator system

### Step 4: Network Builder Extensions (Priority: MEDIUM)
**Files**: Extend `src/core/topology/nimcp_network_builder.h/c`
**Effort**: 1 hour
**Dependencies**: Steps 1-3

### Step 5: Sentiment Analysis Demo (Priority: HIGH)
**Files**: `examples/sentiment_analysis_demo.c`
**Effort**: 2-3 hours
**Dependencies**: All above

### Step 6: Tests & Documentation (Priority: HIGH)
**Files**: `src/tests/attention_tests.cpp`, `docs/features/PHASE_3_*.md`
**Effort**: 2-3 hours
**Dependencies**: All above

**Total Estimated Effort**: 10-15 hours

## Success Criteria

Phase 3 is complete when:
- ✅ Attention synapses compute query-key similarity
- ✅ Sentences process through actual spike dynamics
- ✅ Neuromodulators modulated by pink noise
- ✅ Network builder supports attention and neuromod configuration
- ✅ Sentiment analysis demo works end-to-end
- ✅ All tests passing
- ✅ Complete documentation

## Performance Targets

- **Sentence Processing**: < 100ms for 10-word sentence (1000 neurons)
- **Attention Computation**: O(S) where S = number of synapses
- **Memory Overhead**: < 20% increase vs Phase 2
- **Learning Convergence**: 90% accuracy after 1000 training sentences

## Future Enhancements (Phase 4)

Potential next steps:
1. **GPU Acceleration**: CUDA kernels for attention and spike processing
2. **Real Embeddings**: Load word2vec/GloVe embeddings from file
3. **Multi-Head Attention**: Multiple parallel attention mechanisms
4. **Recurrent Processing**: Temporal context across sentences
5. **Transfer Learning**: Pre-train on large corpus, fine-tune on task

## References

- Vaswani et al. (2017) - "Attention Is All You Need" (Transformer architecture)
- Schultz et al. (1997) - Dopamine and reward prediction
- Montague et al. (2004) - Computational roles of dopamine
- Doya (2002) - Metalearning and neuromodulation

---

**Author**: NIMCP Development Team
**Date**: 2025-11-08
**Status**: Planning Phase
**Version**: 2.7.0 Phase 3 Plan

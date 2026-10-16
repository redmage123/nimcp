# SNN Backpropagation Training Integration - Design Document

## Overview

This document describes the comprehensive SNN backpropagation training integration for NIMCP, providing gradient-based learning for Spiking Neural Networks.

**Files Created:**
- Header: `/home/bbrelin/nimcp/include/training/nimcp_snn_backprop.h`
- Implementation: `/home/bbrelin/nimcp/src/training/nimcp_snn_backprop.c`

## 1. Surrogate Gradient Methods

SNNs use discrete spikes, which are non-differentiable. The spike function derivative is a Dirac delta:

```
S(t) = Θ(V(t) - V_thresh)  where Θ is Heaviside step
dS/dV = δ(V - V_thresh)     Dirac delta (infinite at threshold)
```

**Solution:** Replace with smooth surrogate gradients σ'(V):

| Method | Formula | Reference | Properties |
|--------|---------|-----------|------------|
| **SuperSpike** | σ'(x) = 1/(β\|x\|+1)² | Zenke & Ganguli 2018 | Fast, bounded, biologically motivated |
| **Fast Sigmoid** | σ'(x) = x/(1+\|x\|)² | Shrestha & Orchard 2018 | Faster computation |
| **Sigmoid** | σ'(x) = β·σ(βx)·(1-σ(βx)) | Standard | Smooth, well-behaved |
| **Arctan** | σ'(x) = 1/(1+(πx)²) | Bohte et al. 2002 | Bell curve shape |
| **Triangular** | σ'(x) = max(0, 1-\|x\|/a) | Piecewise | Simple, linear |
| **Rectangular** | σ'(x) = 1 if \|x\|<a else 0 | Straight-through | STE variant |
| **Exponential** | σ'(x) = β·exp(-β\|x\|) | Exponential decay | Smooth falloff |

**API:**
```c
float snn_surrogate_gradient(const snn_backprop_ctx_t* ctx, float membrane_v);
nimcp_tensor_t* snn_surrogate_gradient_tensor(const snn_backprop_ctx_t* ctx,
                                               const nimcp_tensor_t* membrane_v);
int snn_backprop_set_surrogate(snn_backprop_ctx_t* ctx,
                               snn_surrogate_method_t method);
```

## 2. Training Algorithms

### 2.1 BPTT (Backpropagation Through Time)

**What:** Full gradient computation by unrolling through time
**Why:** Most accurate gradients
**How:** Store activations, backprop through LIF dynamics

**Configuration:**
```c
typedef struct {
    uint32_t unroll_steps;           // Timesteps to unroll
    bool truncate;                   // Use truncated BPTT
    uint32_t truncation_length;      // Truncation window
    bool detach_spike_grad;          // Reduce memory
    bool accumulate_over_time;       // Accumulate gradients
} snn_bptt_config_t;
```

**Memory:** O(timesteps × batch_size × n_neurons)

### 2.2 E-prop (Eligibility Propagation)

**What:** Online learning with local eligibility traces
**Why:** Biologically plausible, memory-efficient
**How:** Combine eligibility traces with global learning signal

**Reference:** Bellec et al. "A solution to the learning dilemma for recurrent networks of spiking neurons", Nature Communications 2020

**Configuration:**
```c
typedef struct {
    float eligibility_tau;           // Trace decay (ms)
    float learning_signal_delay;     // Learning signal delay (ms)
    bool use_symmetric_eprop;        // Symmetric e-prop variant
    bool adaptive_learning_signal;   // Adapt signal strength
    float kappa;                     // Dampening [0, 1]
} snn_eprop_config_t;
```

**Memory:** O(n_synapses) - much more efficient than BPTT

### 2.3 RTRL (Real-Time Recurrent Learning)

**What:** True online gradient computation
**Why:** No activation storage needed
**How:** Maintain gradient estimates in real-time

**Configuration:**
```c
typedef struct {
    bool sparse_jacobian;            // Sparse Jacobian approximation
    float sparsity_threshold;        // Threshold for sparsity
    uint32_t max_jacobian_rank;      // Low-rank approximation
} snn_rtrl_config_t;
```

### 2.4 Other Algorithms

- **SLAYER:** Spike layer error reassignment
- **DECOLLE:** Deep continuous local learning
- **Hybrid:** Local STDP + global gradient

## 3. Loss Functions for SNNs

| Loss Type | Description | Use Case |
|-----------|-------------|----------|
| **Spike Count** | L2 on spike count per neuron | Rate-coded regression |
| **First Spike Time** | L2 on first spike timing | Temporal coding |
| **Rate-Coded MSE** | MSE on decoded firing rates | Classification/regression |
| **Rate-Coded Cross-Entropy** | Cross-entropy on rates | Classification |
| **Temporal Cross-Entropy** | Cross-entropy over time | Sequence classification |
| **van Rossum** | Spike distance metric | Spike pattern matching |
| **Victor-Purpura** | Spike distance with timing | Precise timing tasks |
| **Membrane Potential** | MSE on final membrane V | Readout optimization |
| **Custom** | User-defined | Specialized tasks |

**Configuration:**
```c
typedef struct {
    snn_loss_type_t type;            // Loss function type
    nimcp_loss_reduction_t reduction; // mean/sum/none

    // Spike count parameters
    float target_rate;               // Target firing rate (Hz)
    float rate_regularization;       // L2 penalty on rate

    // Timing parameters
    float timing_precision;          // Temporal window (ms)
    float earliest_spike_time;       // Earliest valid spike (ms)
    float latest_spike_time;         // Latest valid spike (ms)

    // Distance metric parameters
    float tau_vr;                    // van Rossum time constant
    float cost_vp;                   // Victor-Purpura spike cost

    // Custom loss
    nimcp_loss_forward_fn custom_forward;
    nimcp_loss_backward_fn custom_backward;
    void* custom_user_data;
} snn_loss_config_t;
```

## 4. Training Pipeline Integration

### 4.1 Gradient Manager Integration

Leverages existing `nimcp_gradient_manager.h` for:
- Gradient accumulation (multi-step batches)
- Gradient scaling (mixed precision)
- Gradient clipping (prevent explosion)
- NaN/Inf detection
- Statistics tracking

**API:**
```c
int snn_backprop_connect_gradient_manager(
    snn_backprop_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);
```

### 4.2 Loss Function Integration

Uses existing `nimcp_loss_functions.h` infrastructure:
- Multiple loss types
- Forward/backward computation
- Reduction modes
- Gradient clipping
- Tensor operations

### 4.3 Optimizer Integration

Connects to NIMCP optimizers (when implemented):
- SGD with momentum
- Adam
- RMSprop
- AdaGrad

## 5. API Examples

### 5.1 Basic Training Loop

```c
// Create network
snn_config_t net_config;
snn_config_ncp(&net_config, 8, 16, 8, 4);  // NCP architecture
snn_network_t* network = snn_network_create(&net_config);

// Configure training
snn_backprop_config_t train_config = snn_backprop_default_config(SNN_TRAIN_BPTT);
train_config.surrogate.method = SNN_SURROGATE_SUPERSPIKE;
train_config.surrogate.beta = 1.0f;
train_config.loss.type = SNN_LOSS_RATE_CODED_MSE;
train_config.learning_rate = 0.001f;
train_config.batch_size = 32;

// Create trainer
snn_backprop_ctx_t* trainer = snn_backprop_create(network, &train_config);

// Training loop
for (int epoch = 0; epoch < num_epochs; epoch++) {
    for (int batch = 0; batch < num_batches; batch++) {
        float* inputs = get_batch_inputs(batch);   // [batch_size × n_inputs]
        float* targets = get_batch_targets(batch); // [batch_size × n_outputs]

        snn_train_result_t result;
        snn_backprop_train_step(trainer, inputs, targets, 32, 100.0f, &result);

        printf("Loss: %.6f, Grad Norm: %.6f\n", result.loss, result.gradient_norm);
    }
}

// Cleanup
snn_backprop_destroy(trainer);
snn_network_destroy(network);
```

### 5.2 E-prop Training (Biologically Plausible)

```c
// Configure for E-prop
snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_EPROP);
config.eprop.eligibility_tau = 20.0f;  // 20ms traces (Bellec et al. 2020)
config.eprop.use_symmetric_eprop = true;
config.eprop.kappa = 0.1f;

snn_backprop_ctx_t* trainer = snn_backprop_create(network, &config);

// Online training (no need to store activations)
for (int step = 0; step < num_steps; step++) {
    float input[8];
    float target[4];

    // Get single sample
    get_sample(step, input, target);

    // Online update
    snn_train_result_t result;
    snn_backprop_train_step(trainer, input, target, 1, 50.0f, &result);
}
```

### 5.3 Tensor-Based Training

```c
// Create input/target tensors
uint32_t dims[] = {32, 100, 8};  // [batch, sequence, features]
nimcp_tensor_t* input_tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);

uint32_t target_dims[] = {32, 4};  // [batch, outputs]
nimcp_tensor_t* target_tensor = nimcp_tensor_create(target_dims, 2, NIMCP_DTYPE_F32);

// Fill tensors with data
// ...

// Train with tensors
snn_train_result_t result;
snn_backprop_train_step_tensor(trainer, input_tensor, target_tensor, &result);

printf("Loss: %.6f\n", result.loss);
```

### 5.4 Custom Loss Function

```c
// Custom loss: penalize early spikes
float custom_loss_forward(const float* predictions, const float* targets,
                          size_t count, void* user_data) {
    float loss = 0.0f;
    float* params = (float*)user_data;
    float early_penalty = params[0];

    for (size_t i = 0; i < count; i++) {
        float error = predictions[i] - targets[i];
        loss += error * error;

        // Penalize early spikes
        if (predictions[i] > 0.5f && i < count/2) {
            loss += early_penalty;
        }
    }
    return loss / count;
}

void custom_loss_backward(const float* predictions, const float* targets,
                          float* gradients, size_t count, void* user_data) {
    for (size_t i = 0; i < count; i++) {
        gradients[i] = 2.0f * (predictions[i] - targets[i]) / count;
    }
}

// Configure custom loss
snn_loss_config_t loss_config = snn_loss_default_config(SNN_LOSS_CUSTOM);
loss_config.custom_forward = custom_loss_forward;
loss_config.custom_backward = custom_loss_backward;
float params[] = {0.1f};  // early_penalty
loss_config.custom_user_data = params;

train_config.loss = loss_config;
```

## 6. Memory Management

### 6.1 BPTT Memory

For full BPTT with sequence length T, batch size B, N neurons:

- **Activations:** T × B × N × 4 tensors (V, spikes, currents, thresholds)
- **Gradients:** N × M (N neurons, M synapses)
- **Total:** ~16 bytes × T × B × N + 4 bytes × N × M

**Example:** 100 timesteps, batch 32, 1000 neurons, 50K synapses
- Activations: 100 × 32 × 1000 × 4 × 4 bytes = 51.2 MB
- Gradients: 50000 × 4 bytes = 200 KB
- **Total: ~51.4 MB per batch**

### 6.2 E-prop Memory

E-prop only needs eligibility traces:
- **Eligibility:** M synapses × 4 bytes
- **No activation storage required**

**Example:** 50K synapses
- Eligibility: 50000 × 4 bytes = 200 KB
- **Total: ~200 KB (constant, independent of sequence length)**

### 6.3 Memory Optimization

```c
snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);

// Truncate BPTT to reduce memory
config.bptt.truncate = true;
config.bptt.truncation_length = 50;  // Only 50 steps instead of full sequence

// Detach spike gradients (stop gradient flow through spikes)
config.bptt.detach_spike_grad = true;

// Use online mode
config.temporal_mode = SNN_TEMPORAL_ONLINE;

// Set memory limit
config.max_memory_bytes = 100 * 1024 * 1024;  // 100 MB limit
```

## 7. Integration with Existing NIMCP Infrastructure

### 7.1 SNN Network
- Uses existing `snn_network_t` from `nimcp_snn_network.h`
- Accesses neuron states via `snn_population_t`
- Modifies weights through existing synapse infrastructure

### 7.2 Gradient Manager
- Connects to `nimcp_gradient_manager_ctx_t`
- Uses gradient accumulation for multi-step batches
- Leverages gradient clipping and scaling

### 7.3 Loss Functions
- Integrates with `nimcp_loss_context_t`
- Uses existing loss implementations (MSE, cross-entropy)
- Extends with SNN-specific losses

### 7.4 Tensor Library
- All activations/gradients stored as `nimcp_tensor_t`
- Uses tensor operations for vectorization
- Leverages tensor calculus for gradient computation

## 8. Testing Strategy

### 8.1 Unit Tests (Recommended)

```c
// Test surrogate gradients
void test_surrogate_superspike() {
    snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    config.surrogate.method = SNN_SURROGATE_SUPERSPIKE;
    config.surrogate.beta = 1.0f;

    snn_backprop_ctx_t ctx = {.config = config};

    // At threshold: should be maximum
    float grad_at_thresh = snn_surrogate_gradient(&ctx, 0.0f);
    assert(grad_at_thresh == 1.0f);

    // Far from threshold: should decay
    float grad_far = snn_surrogate_gradient(&ctx, 10.0f);
    assert(grad_far < 0.01f);
}

// Test gradient computation
void test_backward_pass() {
    // Create simple 2-neuron network
    // Forward pass
    // Backward pass
    // Check gradients match numerical gradients
}

// Test E-prop eligibility traces
void test_eprop_eligibility() {
    // Initialize traces
    // Apply spike
    // Check decay over time
}
```

### 8.2 Integration Tests

- Test with real SNN networks (100-1000 neurons)
- Verify convergence on simple tasks (XOR, MNIST)
- Compare BPTT vs E-prop convergence
- Memory profiling for different configurations

### 8.3 Benchmark Tests

- Training speed (samples/sec)
- Memory usage vs sequence length
- Gradient computation time
- Comparison with existing SNN frameworks

## 9. Biological Plausibility Spectrum

From most to least biologically plausible:

1. **E-prop** - Local eligibility traces, online learning
2. **RTRL** - Real-time learning, no activation storage
3. **SLAYER** - Spike-based error assignment
4. **Truncated BPTT** - Limited temporal credit assignment
5. **Full BPTT** - Non-local, requires activation storage

**Recommendation:** Use E-prop for biologically-motivated research, BPTT for maximum performance.

## 10. References

### Key Papers

1. **SuperSpike:** Zenke & Ganguli, "SuperSpike: Supervised Learning in Multilayer Spiking Neural Networks", Neural Computation 2018
2. **E-prop:** Bellec et al., "A solution to the learning dilemma for recurrent networks of spiking neurons", Nature Communications 2020
3. **SLAYER:** Shrestha & Orchard, "SLAYER: Spike Layer Error Reassignment in Time", NeurIPS 2018
4. **DECOLLE:** Kaiser et al., "Synaptic Plasticity Dynamics for Deep Continuous Local Learning", arXiv 2020

### Additional References

- Bohte et al., "Error-backpropagation in temporally encoded networks of spiking neurons", Neurocomputing 2002
- Neftci et al., "Surrogate Gradient Learning in Spiking Neural Networks", IEEE Signal Processing Magazine 2019

## 11. Next Steps

### Implementation Priority

1. **Core surrogate gradients** (DONE - in stub)
2. **BPTT forward/backward** - Full implementation
3. **Loss functions** - SNN-specific losses
4. **E-prop** - Eligibility traces
5. **Optimization** - SIMD, parallel processing
6. **Testing** - Unit, integration, benchmarks

### Future Enhancements

- Multi-GPU support
- Distributed training
- Mixed precision training
- Adaptive surrogate methods
- Neuromorphic hardware backends

---

**Status:** Header and implementation stub complete. Ready for full implementation.

**Contact:** NIMCP Development Team

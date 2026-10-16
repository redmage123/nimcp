# CNN Training Integration for NIMCP

## Overview

This document describes the CNN (Convolutional Neural Network) training integration for NIMCP, designed to enable hybrid CNN+SNN architectures and provide a pretrain-then-convert workflow for deploying models on neuromorphic hardware.

## Files Created

- **Header**: `/home/bbrelin/nimcp/include/training/nimcp_cnn_training.h` (945 lines)
- **Implementation**: `/home/bbrelin/nimcp/src/training/nimcp_cnn_training.c` (769 lines, stubs)

## Design Philosophy

### Biological Inspiration

The CNN training module bridges classical deep learning with biological neural computation:

1. **Convolutional Layers ≈ Visual Cortex V1**
   - Gabor-like filters model orientation-selective simple cells (Hubel & Wiesel 1962)
   - Hierarchical feature extraction mimics V1 → V2 → V4 → IT pathway
   - Receptive fields correspond to kernel sizes

2. **Pooling ≈ Complex Cells**
   - Max pooling models winner-take-all position invariance
   - Complex cells integrate over spatial positions

3. **Batch Normalization ≈ Divisive Normalization**
   - Lateral inhibition in visual cortex
   - Homeostatic activity regulation

4. **Dropout ≈ Synaptic Pruning**
   - Developmental synaptic elimination
   - Stochastic transmission failures

5. **CNN-to-SNN Conversion ≈ Developmental Learning**
   - Prenatal: Genetically-wired CNN structure (innate detectors)
   - Postnatal: STDP refinement via spiking activity
   - Hybrid: CNN pretraining + SNN fine-tuning

## Architecture Components

### 1. CNN Architecture

#### Layer Types

- **CONV2D**: 2D convolution with configurable kernels, strides, padding
- **CONV1D**: 1D temporal convolution for audio processing
- **DEPTHWISE_CONV2D**: Efficient separable convolutions
- **POOLING**: Max/average/global pooling
- **BATCH_NORM**: Batch normalization for stable training
- **DROPOUT**: Regularization via random neuron dropout
- **DENSE**: Fully connected layers
- **FLATTEN**: Reshape for transition to dense layers
- **ACTIVATION**: Standalone activation functions
- **RESIDUAL**: Skip connections (ResNet-style)

#### Activation Functions

- **ReLU**: Models depolarization threshold
- **Leaky ReLU**: Allows small negative gradients (leak channels)
- **SiLU/Swish**: Smooth sigmoid-like activation
- **GELU**: Gaussian-weighted activation
- **Sigmoid**: Firing rate saturation
- **Tanh**: Balanced activation
- **Softmax**: Winner-take-all normalization

#### Pooling Types

- **Max**: Complex cell maximum response
- **Average**: Population averaging
- **Global Max/Average**: Whole-field integration (IT representations)
- **Stochastic**: Probabilistic sampling
- **Lp Norm**: Generalized norm pooling

### 2. Training Components

#### Forward Propagation

```c
cnn_trainer_forward(trainer, input, &result);
```

- Sequential layer-wise computation
- Activation caching for backpropagation
- Batch processing support
- Bio-async messaging for training events

#### Backpropagation

```c
cnn_trainer_backward(trainer, target, &forward_result);
```

- Reverse-order gradient computation
- Uses cached activations from forward pass
- Integrates with gradient manager for clipping, accumulation

#### Weight Update

```c
cnn_trainer_step(trainer);
```

- Optimizer integration (SGD, Adam, RMSprop, etc.)
- Learning rate scheduling
- Weight decay / L2 regularization
- Gradient clipping

### 3. Loss Functions

Integrated with NIMCP's existing loss function module:

- **Cross-Entropy**: Classification tasks
- **MSE**: Regression tasks
- **Contrastive Loss**: Siamese networks, embeddings
- **Triplet Loss**: Metric learning
- **Focal Loss**: Class imbalance handling

### 4. Data Pipeline

#### Data Loader

```c
cnn_dataloader_t* loader = cnn_dataloader_create(data, labels, &config);
```

- Batch sampling and shuffling
- Multi-threaded data loading
- Memory pinning for GPU transfer
- Automatic batching with drop_last option

#### Data Augmentation

Flags-based augmentation configuration:

- **CNN_AUGMENT_FLIP_HORIZONTAL/VERTICAL**: Mirror flips
- **CNN_AUGMENT_ROTATE**: Random rotations
- **CNN_AUGMENT_SCALE**: Random scaling
- **CNN_AUGMENT_TRANSLATE**: Random translations
- **CNN_AUGMENT_BRIGHTNESS/CONTRAST**: Color jitter
- **CNN_AUGMENT_NOISE**: Gaussian noise injection
- **CNN_AUGMENT_CUTOUT**: Random region dropout
- **CNN_AUGMENT_MIXUP**: Sample blending

### 5. CNN-to-SNN Conversion

#### Conversion Methods

1. **Rate Coding** (Default)
   ```
   Firing Rate = normalize(CNN_activation) × max_firing_rate
   ```
   - Biological: 0-200 Hz cortical firing rates
   - Simple, preserves activation magnitude

2. **Population Coding**
   - Multiple SNN neurons encode single CNN unit
   - Gaussian receptive fields across value range
   - Distributed representation

3. **Temporal Coding**
   - High activation → early spike
   - Time-to-first-spike carries information
   - Rapid processing (Thorpe 2001)

4. **Burst Coding**
   - Burst count encodes activation intensity
   - Models burst behavior in biological neurons

5. **Hybrid Coding**
   - Layer-specific encoding schemes
   - Conv layers: Population coding
   - Dense layers: Rate coding

#### Conversion Algorithm

```c
cnn_to_snn_convert(converter, trainer, calibration_data, &result);
```

**Steps:**

1. **Calibration**: Run CNN on calibration dataset, collect activation statistics
   - Mean, max, percentiles per layer
   - Identify activation ranges

2. **Normalization**: Compute firing rate scales
   ```
   scale = max_firing_rate / max_activation
   ```

3. **Threshold Setting**: Set SNN thresholds based on quantiles
   ```
   threshold = percentile(activations, threshold_quantile)
   ```

4. **Weight Conversion**: Map CNN weights to SNN synapses
   - Conv2D → SNN with spatial connectivity
   - Dense → SNN fully connected
   - Normalize weights by firing rate scale

5. **Network Construction**: Build SNN network
   - Preserve layer topology
   - Configure neuron models (LIF/Izhikevich)
   - Set synaptic delays

6. **Verification**: Test accuracy on calibration data
   - Measure conversion accuracy loss
   - Typical: <5% accuracy drop with proper normalization

7. **STDP Fine-tuning** (Optional)
   ```c
   cnn_to_snn_finetune_stdp(result, train_data, epochs);
   ```
   - Refine weights with spike-based plasticity
   - Recover lost accuracy
   - Adapt to spiking dynamics

#### Biological Grounding

The conversion mimics developmental transitions:

- **Prenatal**: CNN structure (genetically determined connectivity)
- **Postnatal**: STDP refinement (experience-dependent plasticity)
- **Outcome**: Efficient spiking network (energy-efficient computation)

## NIMCP Integration

### 1. Visual Cortex Integration

```c
cnn_connect_visual_cortex(trainer, visual_cortex);
```

- CNN layers serve as V1 feature extractors
- Output feeds to hippocampus, prefrontal cortex
- Enables vision-language grounding

### 2. Audio Cortex Integration

```c
cnn_connect_audio_cortex(trainer, audio_cortex);
```

- CNN for spectrotemporal features
- Cochlear processing via 2D convolutions on spectrograms
- Temporal pattern recognition

### 3. Bio-Async Messaging

```c
cnn_connect_bio_async(trainer);
```

**Module ID**: `BIO_MODULE_CNN_TRAINING = 0x0700`

**Messages Sent**:
- Training epoch start/end
- Loss updates
- Gradient statistics
- Convergence events

**Integration Points**:
- **Immune System**: Instability detection (NaN/Inf gradients)
- **Plasticity**: Learning rate modulation
- **Cognitive Training**: Cross-modal coordination

### 4. Optimizer Integration

Uses existing NIMCP optimizers:

- `NIMCP_OPTIMIZER_SGD`: Stochastic Gradient Descent with momentum
- `NIMCP_OPTIMIZER_ADAM`: Adaptive moment estimation
- `NIMCP_OPTIMIZER_ADAMW`: Adam with decoupled weight decay
- `NIMCP_OPTIMIZER_RMSPROP`: Root mean square propagation

### 5. Gradient Manager Integration

- **Gradient Accumulation**: Large batch training
- **Gradient Scaling**: Mixed precision training
- **Gradient Clipping**: Prevent explosion
- **NaN/Inf Detection**: Automatic handling

## Use Cases

### 1. Visual Perception

```
ImageNet CNN → Convert to SNN → Deploy as robot vision system
```

- Pretrain on ImageNet for object recognition
- Convert to SNN for neuromorphic hardware
- Energy-efficient real-time vision

### 2. Audio Processing

```
Audio CNN (spectrograms) → SNN cochlear model → Speech recognition
```

- Train on speech spectrograms
- Convert to SNN for auditory pathway
- Temporal pattern recognition

### 3. Transfer Learning

```
Pretrained CNN → Fine-tune on task → Convert to SNN
```

- Leverage ImageNet features
- Task-specific fine-tuning
- Deploy as SNN in NIMCP brain

### 4. Hybrid Networks

```
CNN feature extraction → SNN decision making → Motor control
```

- CNN for complex visual features
- SNN for real-time decisions
- Combine strengths of both paradigms

## API Examples

### Basic CNN Training

```c
// Configure trainer
cnn_trainer_config_t config;
cnn_trainer_default_config(&config);
config.max_epochs = 50;
config.learning_rate = 0.001f;
config.optimizer_config.type = NIMCP_OPTIMIZER_ADAM;

// Create trainer
cnn_trainer_t* trainer = cnn_trainer_create(&config);

// Build architecture
cnn_conv_config_t conv_cfg = {
    .kernel_h = 3, .kernel_w = 3,
    .stride_h = 1, .stride_w = 1,
    .in_channels = 3, .out_channels = 64,
    .activation = CNN_ACTIVATION_RELU
};
cnn_trainer_add_conv_layer(trainer, &conv_cfg);

cnn_pool_config_t pool_cfg = {
    .type = CNN_POOL_MAX,
    .pool_h = 2, .pool_w = 2,
    .stride_h = 2, .stride_w = 2
};
cnn_trainer_add_pool_layer(trainer, &pool_cfg);

// Add more layers...

cnn_trainer_add_flatten_layer(trainer);

cnn_dense_config_t dense_cfg = {
    .in_features = 1024,
    .out_features = 10,
    .activation = CNN_ACTIVATION_SOFTMAX
};
cnn_trainer_add_dense_layer(trainer, &dense_cfg);

// Create data loaders
cnn_dataloader_config_t loader_cfg;
loader_cfg.batch_size = 32;
loader_cfg.shuffle = true;

cnn_dataloader_t* train_loader = cnn_dataloader_create(train_data, train_labels, &loader_cfg);
cnn_dataloader_t* val_loader = cnn_dataloader_create(val_data, val_labels, &loader_cfg);

// Train
cnn_trainer_fit(trainer, train_loader, val_loader);

// Cleanup
cnn_dataloader_destroy(train_loader);
cnn_dataloader_destroy(val_loader);
cnn_trainer_destroy(trainer);
```

### CNN-to-SNN Conversion

```c
// Configure conversion
cnn_to_snn_config_t conv_config = {
    .method = CNN_TO_SNN_RATE_CODING,
    .max_firing_rate = 200.0f,
    .time_steps = 100,
    .timestep_ms = 1.0f,
    .normalize_weights = true,
    .normalize_thresholds = true,
    .threshold_quantile = 0.99f,
    .enable_stdp_finetuning = true,
    .finetune_epochs = 10,
    .stdp_learning_rate = 0.001f
};

// Create converter
cnn_to_snn_converter_t* converter = cnn_to_snn_converter_create(&conv_config);

// Convert
cnn_to_snn_result_t result;
cnn_to_snn_convert(converter, trained_cnn, calibration_data, &result);

printf("Conversion complete:\n");
printf("  Total neurons: %u\n", result.total_neurons);
printf("  Total synapses: %u\n", result.total_synapses);
printf("  Avg firing rate: %.2f Hz\n", result.avg_firing_rate);
printf("  Accuracy loss: %.2f%%\n", (1.0f - result.conversion_accuracy) * 100.0f);

// Use converted SNN
snn_network_t* snn = result.snn;
// Run SNN inference...

// Cleanup
cnn_to_snn_converter_destroy(converter);
snn_network_destroy(snn);
```

### NIMCP Brain Integration

```c
// Create CNN trainer for visual cortex
cnn_trainer_t* visual_cnn = cnn_trainer_create(&config);
// Build visual architecture...

// Connect to NIMCP modules
cnn_connect_visual_cortex(visual_cnn, brain->visual_cortex);
cnn_connect_bio_async(visual_cnn);

// Train on visual data
cnn_trainer_fit(visual_cnn, visual_train_loader, visual_val_loader);

// Convert to SNN for deployment
cnn_to_snn_result_t snn_result;
cnn_to_snn_convert(converter, visual_cnn, calibration_images, &snn_result);

// Deploy SNN in visual cortex
visual_cortex_set_snn(brain->visual_cortex, snn_result.snn);
```

## Implementation Status

### Current State: STUB IMPLEMENTATION

All functions are stubbed with:
- Proper function signatures
- Guard clauses for null checks
- NIMCP_LOGGING_WARN messages
- Return NIMCP_ERROR_NOT_IMPLEMENTED

### Implementation Roadmap

#### Phase 1: Core Layer Operations (High Priority)

1. **Convolution Forward/Backward**
   - Implement im2col algorithm for efficient convolution
   - GEMM (General Matrix Multiply) for batched convolution
   - Backward pass: gradient w.r.t. input, weights, bias

2. **Pooling Forward/Backward**
   - Max pooling with index tracking
   - Average pooling
   - Backward pass: gradient routing

3. **Activation Functions**
   - ReLU, Leaky ReLU, Sigmoid, Tanh, Softmax
   - Forward and backward (derivatives)

4. **Dense Layers**
   - Matrix multiplication
   - Bias addition
   - Backward pass

#### Phase 2: Training Infrastructure (High Priority)

1. **Forward Pass Pipeline**
   - Sequential layer execution
   - Activation caching
   - Batch processing

2. **Backward Pass Pipeline**
   - Reverse-order gradient computation
   - Gradient accumulation
   - Chain rule application

3. **Optimizer Integration**
   - Connect to existing NIMCP optimizers
   - Parameter registration
   - Weight updates

4. **Loss Functions**
   - Cross-entropy implementation
   - Gradient computation
   - Reduction modes

#### Phase 3: Data Pipeline (Medium Priority)

1. **Data Loader**
   - Batch sampling
   - Shuffling with random seed
   - Multi-threaded loading

2. **Data Augmentation**
   - Image transformations (flip, rotate, crop)
   - Color jitter
   - Cutout and MixUp

#### Phase 4: CNN-to-SNN Conversion (Medium Priority)

1. **Activation Analysis**
   - Statistical collection during calibration
   - Percentile computation
   - Firing rate normalization

2. **Weight Conversion**
   - Conv2D → SNN spatial connectivity
   - Dense → SNN fully connected
   - Threshold calibration

3. **SNN Construction**
   - Layer-wise network building
   - Neuron configuration
   - Synaptic delay setting

4. **STDP Fine-tuning**
   - Integrate with existing SNN training module
   - Run STDP on converted network
   - Accuracy recovery

#### Phase 5: Advanced Features (Low Priority)

1. **Batch Normalization**
   - Running statistics tracking
   - Inference vs training mode

2. **Dropout**
   - Stochastic mask generation
   - Scaling in inference

3. **Residual Connections**
   - Skip connection addition
   - Gradient flow

4. **Advanced Augmentation**
   - Stochastic pooling
   - AutoAugment policies

#### Phase 6: NIMCP Integration (Low Priority)

1. **Visual Cortex Integration**
   - CNN output → visual features
   - Salience map generation

2. **Audio Cortex Integration**
   - Spectrogram processing
   - Temporal feature extraction

3. **Bio-Async Messaging**
   - Training event broadcasting
   - Immune system coordination

## Coding Standards Compliance

All code follows NIMCP standards:

1. **Guard Clauses**: All functions check arguments first, early returns
2. **Function Size**: Target < 50 lines (stubs currently smaller)
3. **WHAT-WHY-HOW Documentation**: Every function documented
4. **Single Responsibility**: Each function has one clear purpose
5. **Biological Grounding**: Biological analogies documented
6. **Memory Safety**: Use nimcp_malloc/nimcp_free, proper cleanup

## Testing Strategy

### Unit Tests

- Layer forward/backward passes
- Activation functions
- Pooling operations
- Shape computations
- Data augmentation

### Integration Tests

- Full forward-backward cycle
- Optimizer integration
- Gradient manager integration
- Loss function integration

### E2E Tests

- MNIST training (simple CNN)
- CIFAR-10 training (deeper CNN)
- CNN-to-SNN conversion accuracy
- Visual cortex integration

### Regression Tests

- Gradient numerical stability
- Convergence behavior
- Memory leak detection
- Thread safety

## Performance Considerations

### Optimization Opportunities

1. **SIMD Vectorization**: Utilize AVX/NEON for convolution
2. **GEMM Libraries**: Link to OpenBLAS/MKL for matrix operations
3. **GPU Acceleration**: CUDA/OpenCL for large CNNs
4. **Memory Pooling**: Reuse activation buffers across layers
5. **Multi-threading**: Parallel batch processing

### Memory Management

- **Layer Buffers**: Preallocate activation/gradient tensors
- **Copy-on-Write**: Share weights across threads
- **Gradient Checkpointing**: Trade compute for memory
- **Mixed Precision**: FP16 activations, FP32 gradients

## References

### Biological Neuroscience

- Hubel & Wiesel (1962): Receptive fields in cat visual cortex
- Thorpe et al. (2001): Spike-based rapid categorization
- Bi & Poo (1998): STDP in hippocampal neurons
- Turrigiano (2012): Homeostatic synaptic scaling

### Deep Learning

- LeCun et al. (1998): Gradient-based learning
- He et al. (2015): Deep residual learning
- Ioffe & Szegedy (2015): Batch normalization

### Neuromorphic Computing

- Diehl & Cook (2015): Unsupervised learning in SNNs
- Rueckauer et al. (2017): CNN-to-SNN conversion methods
- Sengupta et al. (2019): Going deeper in SNNs

## Conclusion

This CNN training integration provides a comprehensive framework for:

1. **Classical CNN Training**: Standard backpropagation-based learning
2. **Biological Grounding**: Connections to cortical processing
3. **SNN Conversion**: Deployment pathway for neuromorphic hardware
4. **NIMCP Integration**: Seamless integration with brain modules

The modular design allows incremental implementation while maintaining a clear API for users. The CNN-to-SNN conversion pathway enables hybrid architectures that combine the training efficiency of CNNs with the energy efficiency of SNNs.

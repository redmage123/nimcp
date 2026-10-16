# NIMCP Platform Tier System (Portia Spider Foundation)

## Overview

The **Platform Tier System** enables NIMCP to run on diverse hardware platforms, from high-end servers to constrained IoT devices. Named after *Portia fimbriata*, a jumping spider with extraordinary cognitive abilities despite having only ~600,000 neurons, this system automatically detects platform capabilities and configures NIMCP components appropriately.

## Files Created

1. **`include/utils/platform/nimcp_platform_tier.h`** - Header with API definitions
2. **`src/utils/platform/nimcp_platform_tier.c`** - Implementation

## Platform Tiers

### Tier 0: FULL (High-End Desktop/Server)
**Hardware Requirements:**
- RAM: ≥8GB
- CPU: ≥8 cores

**Configuration:**
- Max neurons: 1,000,000
- Max synapses/neuron: 10,000
- Memory budget: 4GB
- Visual resolution: 640x480
- Audio sample rate: 48kHz (CD quality)
- All cognitive modules enabled

**Use Cases:**
- Research and development
- Large-scale training
- Full-featured inference
- Multi-modal processing

### Tier 1: MEDIUM (Laptop/Tablet/Dev Board)
**Hardware Requirements:**
- RAM: ≥2GB
- CPU: ≥4 cores

**Configuration:**
- Max neurons: 100,000
- Max synapses/neuron: 1,000
- Memory budget: 1GB
- Visual resolution: 320x240
- Audio sample rate: 22kHz
- Core cognitive modules enabled

**Use Cases:**
- Development on laptops
- Edge AI applications
- Embedded vision systems
- Robotics platforms

### Tier 2: CONSTRAINED (Phone/Drone/Embedded)
**Hardware Requirements:**
- RAM: ≥256MB
- CPU: ≥2 cores

**Configuration:**
- Max neurons: 10,000
- Max synapses/neuron: 100
- Memory budget: 128MB
- Visual resolution: 160x120
- Audio sample rate: 16kHz
- Essential modules only

**Use Cases:**
- Mobile robotics
- Drones
- IoT gateways
- Smart sensors

### Tier 3: MINIMAL (IoT/MCU/Ultra-Constrained)
**Hardware Requirements:**
- RAM: ≥64MB
- CPU: ≥1 core

**Configuration:**
- Max neurons: 1,000 (Portia-scale!)
- Max synapses/neuron: 10
- Memory budget: 32MB
- Visual resolution: 64x48
- Audio: Disabled
- Reactive processing only

**Use Cases:**
- Microcontrollers
- Ultra-low-power sensors
- Minimal inference
- Resource-critical applications

## API Usage

### Basic Usage

```c
#include "utils/platform/nimcp_platform_tier.h"

// Auto-detect platform tier
platform_tier_t tier = platform_tier_detect();
printf("Running on %s platform\n", platform_tier_get_name(tier));

// Get configuration for detected tier
platform_tier_config_t config = platform_tier_get_config(tier);

// Create brain with tier-appropriate settings
brain_t* brain = brain_create(config.max_neurons, config.initial_neurons);
```

### Check Module Availability

```c
// Check if curiosity module is available
if (platform_tier_can_enable_module(tier, COGNITIVE_MODULE_CURIOSITY)) {
    // Safe to initialize curiosity system
    curiosity_init();
} else {
    printf("Curiosity module disabled on this platform\n");
}
```

### Dynamic Neuron Count Recommendation

```c
// Get system resources
system_resources_t resources;
system_resources_query(&resources);

// Get recommended neuron count based on actual available RAM
uint32_t recommended = platform_tier_recommend_neuron_count(tier, &resources);
printf("Recommended neurons: %u\n", recommended);
```

### Validate Custom Configuration

```c
// User wants custom configuration
platform_tier_config_t custom_config = platform_tier_get_config(tier);
custom_config.max_neurons = 50000;  // Custom value

// Validate against tier constraints
char error[256];
if (!platform_tier_validate_config(tier, &custom_config, error, sizeof(error))) {
    printf("Invalid config: %s\n", error);
    // Fall back to defaults
    custom_config = platform_tier_get_config(tier);
}
```

## Cognitive Module Flags

The system uses bitmask flags for fine-grained control over enabled modules:

```c
// Core modules (highest priority)
COGNITIVE_MODULE_ATTENTION
COGNITIVE_MODULE_WORKING_MEMORY
COGNITIVE_MODULE_SALIENCE

// Emotional modules
COGNITIVE_MODULE_EMOTIONS
COGNITIVE_MODULE_EMOTIONAL_TAG

// Memory systems
COGNITIVE_MODULE_SEMANTIC_MEMORY
COGNITIVE_MODULE_EPISODIC_MEMORY
COGNITIVE_MODULE_CONSOLIDATION

// Executive functions
COGNITIVE_MODULE_EXECUTIVE
COGNITIVE_MODULE_REASONING
COGNITIVE_MODULE_CURIOSITY

// Meta-cognitive
COGNITIVE_MODULE_META_LEARNING
COGNITIVE_MODULE_INTROSPECTION
COGNITIVE_MODULE_SELF_AWARENESS

// Social cognition
COGNITIVE_MODULE_THEORY_OF_MIND
COGNITIVE_MODULE_MIRROR_NEURONS
COGNITIVE_MODULE_EMPATHY

// Advanced features
COGNITIVE_MODULE_GLOBAL_WORKSPACE
COGNITIVE_MODULE_PREDICTIVE
COGNITIVE_MODULE_ETHICS

// Perception
COGNITIVE_MODULE_VISUAL_CORTEX
COGNITIVE_MODULE_AUDIO_CORTEX
```

## Integration Examples

### Visual Cortex Integration

```c
platform_tier_config_t config = platform_tier_get_config(tier);

visual_cortex_config_t visual_cfg = {
    .input_width = config.visual.max_input_width,
    .input_height = config.visual.max_input_height,
    .num_filters = config.visual.num_filters_conv1,
    .enable_attention = config.visual.enable_attention
};

visual_cortex_t* v1 = visual_cortex_create(&visual_cfg);
```

### Audio Cortex Integration

```c
platform_tier_config_t config = platform_tier_get_config(tier);

audio_cortex_config_t audio_cfg = {
    .sample_rate = config.audio.max_sample_rate,
    .num_mel_filters = config.audio.num_mel_filters,
    .num_mfcc = config.audio.num_mfcc,
    .enable_attention = config.audio.enable_attention
};

audio_cortex_t* a1 = audio_cortex_create(&audio_cfg);
```

### Brain Creation with Tier Config

```c
platform_tier_t tier = platform_tier_detect();
platform_tier_config_t config = platform_tier_get_config(tier);

// Create brain with tier-appropriate settings
brain_create_params_t params = {
    .num_neurons = config.initial_neurons,
    .max_neurons = config.max_neurons,
    .max_synapses_per_neuron = config.max_synapses_per_neuron,
    .enable_plasticity = config.enable_plasticity,
    .enable_neuromodulation = config.enable_neuromodulation,
    .num_threads = config.max_threads
};

brain_t* brain = brain_create_with_params(&params);
```

## Performance Characteristics

### Memory Estimates

| Tier        | Neurons  | Est. Memory | Use Case           |
|-------------|----------|-------------|--------------------|
| FULL        | 1M       | ~10GB       | Research/Training  |
| MEDIUM      | 100K     | ~1GB        | Development/Edge   |
| CONSTRAINED | 10K      | ~100MB      | Mobile/Drone       |
| MINIMAL     | 1K       | ~10MB       | IoT/MCU            |

### Compute Budgets

| Tier        | FLOPS       | Visual FPS | Audio Rate |
|-------------|-------------|------------|------------|
| FULL        | 10 GFLOPS   | 30 FPS     | 48kHz      |
| MEDIUM      | 1 GFLOPS    | 15 FPS     | 22kHz      |
| CONSTRAINED | 100 MFLOPS  | 5 FPS      | 16kHz      |
| MINIMAL     | 10 MFLOPS   | 1 FPS      | Disabled   |

## Design Philosophy

### Graceful Degradation

The tier system implements **progressive feature reduction**:

1. **FULL → MEDIUM**: Reduce scale, maintain full cognition
2. **MEDIUM → CONSTRAINED**: Disable advanced modules, keep essentials
3. **CONSTRAINED → MINIMAL**: Reactive processing only

### Conservative Defaults

All tier configurations use **conservative resource budgets**:

- Memory budgets leave room for OS and other processes
- Neuron counts use 80% safety margin
- Compute estimates account for overhead

### Portia Inspiration

Like *Portia fimbriata* (the Portia spider):

- **Minimal tier** mimics Portia's ~600K neurons
- Demonstrates cognitive capabilities on minimal hardware
- Adaptive strategies based on available resources
- Efficient resource utilization

## Testing

A test program is provided to verify the implementation:

```bash
# Compile test program
gcc -I/home/bbrelin/nimcp/include \
    -c src/utils/platform/nimcp_platform_tier.c -o platform_tier.o
gcc -I/home/bbrelin/nimcp/include \
    test_platform_tier.c platform_tier.o \
    -o test_platform_tier

# Run test
./test_platform_tier
```

Expected output shows:
- System resource detection
- Tier classification
- Module availability
- Recommended neuron counts
- Configuration validation

## Future Extensions

### Planned Enhancements

1. **Dynamic tier adjustment** - Runtime tier switching based on load
2. **GPU tier variants** - Separate tiers for CUDA/OpenCL platforms
3. **Power-aware tiers** - Battery vs. AC power modes
4. **Custom tier definitions** - User-defined tiers for specific hardware

### Integration Roadmap

1. **Phase 1**: Basic tier detection and config retrieval ✓
2. **Phase 2**: Brain creation integration
3. **Phase 3**: Cortex configuration integration
4. **Phase 4**: Cognitive module auto-configuration
5. **Phase 5**: Runtime monitoring and adaptation

## References

- **Portia spider**: Cross, F. R., & Jackson, R. R. (2016). "The execution of planned detours by spider-eating predators"
- **Resource-aware computing**: Lipasti, M. (2014). "Hardware-Software Co-Design for Cognitive Systems"
- **Graceful degradation**: Parnas, D. L. (1979). "Designing Software for Ease of Extension and Contraction"

## Author

NIMCP Development Team
Date: 2025-12-08
Version: 2.8.0

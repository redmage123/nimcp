# NIMCP Configuration System

## Overview

NIMCP now supports creating brains from YAML or JSON configuration files. This allows you to:

- Define brain architecture declaratively
- Version control your brain configurations
- Share configurations across teams
- Easily experiment with different parameters
- Separate code from configuration

## Quick Start

### 1. Create a Configuration File

```yaml
# my_brain.yaml
brain:
  name: "mnist_classifier"
  size: small
  task: classification

  architecture:
    num_inputs: 784    # 28x28 images
    num_outputs: 10    # 10 digits
    num_hidden: 256
    learning_rate: 0.01

  model_path: "/tmp/mnist.brain"
```

### 2. Load in Your Code

**C:**
```c
#include "nimcp.h"

nimcp_init();
nimcp_brain_t brain = nimcp_brain_create_from_config("my_brain.yaml");
// Use brain...
nimcp_brain_destroy(brain);
nimcp_shutdown();
```

**Python:**
```python
import nimcp

nimcp.init()
brain = nimcp.Brain.from_config("my_brain.yaml")
# Use brain...
brain.destroy()
```

**Ruby:**
```ruby
require 'nimcp'

NIMCP.init
brain = NIMCP::Brain.from_config('my_brain.yaml')
# Use brain...
brain.destroy
```

## Configuration Schema

### Complete Example

```yaml
brain:
  # Identity
  name: "my_brain"
  size: small                    # tiny | small | medium | large
  task: classification           # classification | regression | pattern_matching | sequence | association

  # Architecture
  architecture:
    num_inputs: 784
    num_outputs: 10
    num_hidden: 256
    learning_rate: 0.01

  # Training
  training:
    max_epochs: 100
    batch_size: 32
    validation_split: 0.2
    early_stopping: true
    patience: 10

  # Plasticity
  plasticity:
    enable_bcm: true
    bcm_tau: 1000.0
    enable_stdp: false
    stdp_window: 20.0

  # Ethics
  ethics:
    enabled: false
    golden_rule_threshold: 0.0
    empathy_weight: 0.5

  # Persistence
  model_path: "/tmp/brain.model"
  checkpoint_interval: 10
```

### JSON Format

```json
{
  "brain": {
    "name": "my_brain",
    "size": "small",
    "task": "classification",
    "architecture": {
      "num_inputs": 784,
      "num_outputs": 10,
      "num_hidden": 256,
      "learning_rate": 0.01
    }
  }
}
```

## API Reference

### C API

```c
/**
 * Create brain from configuration file
 * Supports .yaml, .yml, and .json files
 */
nimcp_brain_t nimcp_brain_create_from_config(const char* config_filepath);
```

### Configuration Structure

```c
typedef struct {
    // Basic
    char name[128];
    int size;           // nimcp_brain_size_t (0-3)
    int task;           // nimcp_brain_task_t (0-4)

    // Architecture
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t num_hidden;
    float learning_rate;

    // Training
    uint32_t max_epochs;
    uint32_t batch_size;
    float validation_split;
    bool early_stopping;
    uint32_t patience;

    // Plasticity
    bool enable_bcm;
    float bcm_tau;
    bool enable_stdp;
    float stdp_window;

    // Ethics
    bool ethics_enabled;
    float golden_rule_threshold;
    float empathy_weight;

    // Model
    char model_path[256];
    uint32_t checkpoint_interval;
} nimcp_brain_config_t;
```

## Files Created

```
nimcp/
├── configs/                                 # Configuration files
│   ├── README.md                           # Config documentation
│   ├── brain_simple.json                   # Minimal example
│   ├── brain_classifier_small.yaml         # Classification example
│   └── brain_regression_medium.yaml        # Regression example
│
├── src/
│   ├── include/
│   │   └── nimcp.h                         # Added: nimcp_brain_create_from_config()
│   │
│   ├── api/
│   │   └── nimcp.c                         # Added: Implementation
│   │
│   └── utils/config/
│       ├── nimcp_config.h                  # Config parser header
│       └── nimcp_config.c                  # Config parser implementation
│
├── examples/
│   └── config_based_brain.c                # Example usage
│
└── docs/
    └── CONFIG_SYSTEM.md                    # This file
```

## Example Configurations

### Tiny Brain (Testing/Prototyping)

```yaml
brain:
  name: "test_brain"
  size: tiny
  task: classification
  architecture:
    num_inputs: 10
    num_outputs: 3
    num_hidden: 20
    learning_rate: 0.01
```

### Small Brain (Mobile/Edge)

```yaml
brain:
  name: "mobile_classifier"
  size: small
  task: classification
  architecture:
    num_inputs: 128
    num_outputs: 5
    num_hidden: 100
    learning_rate: 0.01
  training:
    max_epochs: 50
    batch_size: 16
```

### Medium Brain (General Purpose)

```yaml
brain:
  name: "image_classifier"
  size: medium
  task: classification
  architecture:
    num_inputs: 784
    num_outputs: 10
    num_hidden: 512
    learning_rate: 0.001
  training:
    max_epochs: 200
    batch_size: 64
  plasticity:
    enable_bcm: true
    bcm_tau: 2000.0
```

### Large Brain (Complex Tasks)

```yaml
brain:
  name: "complex_predictor"
  size: large
  task: regression
  architecture:
    num_inputs: 2048
    num_outputs: 100
    num_hidden: 1024
    learning_rate: 0.0001
  training:
    max_epochs: 500
    batch_size: 128
  plasticity:
    enable_bcm: true
    bcm_tau: 5000.0
  ethics:
    enabled: true
    golden_rule_threshold: 0.0
    empathy_weight: 0.7
```

## Usage Tips

1. **Start with defaults**: Use `brain_simple.json` as a template
2. **Tune incrementally**: Change one parameter at a time
3. **Version control**: Keep configs in git
4. **Document choices**: Add YAML comments explaining why
5. **Environment-specific**: Use different configs for dev/test/prod

## Parser Implementation

The configuration parser:

- **Simple**: No external dependencies (no libyaml needed)
- **Robust**: Validates all parameters
- **Flexible**: Supports both YAML and JSON
- **Fast**: Efficient parsing for quick loading
- **Error-friendly**: Clear error messages

### Example Error Messages

```
Failed to open config file: configs/missing.yaml
Invalid size value: 'huge' (must be tiny, small, medium, or large)
Missing required field: num_inputs
```

## Future Enhancements

Planned additions:

- [ ] Multi-layer architecture definitions
- [ ] Custom activation function selection
- [ ] Regularization options (dropout, L1/L2)
- [ ] Advanced ethics policy configuration
- [ ] Distributed training settings
- [ ] Hyperparameter search grid specification
- [ ] Network topology visualization export

## Migration Guide

### Before (Hardcoded):

```c
nimcp_brain_t brain = nimcp_brain_create(
    "classifier",
    NIMCP_BRAIN_SMALL,
    NIMCP_TASK_CLASSIFICATION,
    784,
    10
);
```

### After (Config-based):

```yaml
# classifier.yaml
brain:
  name: "classifier"
  size: small
  task: classification
  architecture:
    num_inputs: 784
    num_outputs: 10
```

```c
nimcp_brain_t brain = nimcp_brain_create_from_config("classifier.yaml");
```

## Benefits

1. **Flexibility**: Change architecture without recompiling
2. **Reproducibility**: Share exact configurations
3. **Documentation**: Config files document the architecture
4. **Experimentation**: Easy A/B testing of parameters
5. **Deployment**: Different configs for different environments
6. **Collaboration**: Team members can share configurations

## See Also

- `configs/README.md` - Detailed configuration reference
- `examples/config_based_brain.c` - Complete C example
- `src/utils/config/nimcp_config.h` - Parser API reference

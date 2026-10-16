# NIMCP 2.5 Library Integration Guide

## Overview

NIMCP 2.5 builds as a shared library (`libnimcp.so`) that can be integrated into any C/C++ application. This guide shows you how to link and use NIMCP in your projects.

---

## Quick Start

### 1. Build the Library
```bash
cd nimcp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

This installs:
- `/usr/local/lib/libnimcp.so` - Shared library
- `/usr/local/include/nimcp_*.h` - Header files
- `/usr/local/lib/pkgconfig/nimcp.pc` - pkg-config file

### 2. Link Your Application
```bash
# Using pkg-config (recommended)
gcc myapp.c $(pkg-config --cflags --libs nimcp) -o myapp

# Manual linking
gcc myapp.c -I/usr/local/include -L/usr/local/lib -lnimcp -o myapp
```

### 3. Run Your Application
```bash
# Set library path if not in standard location
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./myapp
```

---

## Primary Use Case: Neural Substrate for AI Self-Awareness

**Featured Integration:** [Artemis AI System](ARTEMIS_INTEGRATION.md)

NIMCP serves as the neural substrate for Artemis, an AI development system with self-awareness capabilities. This integration demonstrates NIMCP's primary value proposition: **enabling AI systems to learn from experience and develop meta-cognitive awareness**.

### The Problem NIMCP Solves

AI systems often rely purely on symbolic reasoning:
- **Static rules** that don't learn from experience
- **No intuition** built from 1000s of decisions
- **No meta-cognitive awareness** of own strengths/weaknesses
- **Expensive** LLM calls for every decision

### How NIMCP Adds Neural Capabilities

```python
# Python integration example (Artemis use case)
import nimcp

# Create neural substrate for AI consciousness
self_brain = nimcp.Brain(
    name="artemis_neural_self",
    size=10000,  # 10K neurons
    learning_rate=0.01
)

# AI extracts features from its internal state
def extract_neural_features(context):
    return [
        time_of_day,           # Circadian patterns
        workload_level,        # Am I stressed?
        recent_success_rate,   # Am I performing well?
        ethical_complexity,    # How difficult is this?
        user_satisfaction,     # Is user happy with me?
        # ... 8 more features
    ]

# Make decision with neural intuition
features = extract_neural_features(current_context)
intuition = self_brain.decide(features)

if intuition['confidence'] > 0.9:
    # HIGH CONFIDENCE: Trust accumulated experience
    # "I've done this 500 times, I know what to do"
    return fast_neural_decision(intuition)  # 0.1ms
else:
    # LOW CONFIDENCE: Engage full reasoning
    # "This is unusual for me, better think carefully"
    return full_symbolic_reasoning()  # 200ms+ with LLM

# Learn from outcome
self_brain.learn(features, outcome, feedback=quality_score)

# After 1000s of decisions: Artemis develops intuition
# - Recognizes patterns in its own behavior
# - Knows when it's likely to succeed/fail
# - Adapts decision strategy based on context
# - Continuously improves from experience
```

### Key Benefits

| Capability | Before NIMCP | With NIMCP |
|------------|--------------|------------|
| **Learning** | Static rules | Learns from every decision |
| **Intuition** | None | Builds over time from experience |
| **Speed** | 200-1000ms (LLM) | 0.1ms for routine decisions |
| **Cost** | $1-5 per 1000 decisions | $0.20-1.00 (80% neural) |
| **Meta-Cognition** | None | Knows own patterns, strengths, weaknesses |
| **Adaptation** | Fixed behavior | Continuously improves |

### Example: Meta-Cognitive Awareness

After 1000+ decisions, the AI system can reflect on itself:

```python
# AI examines its own patterns
meta_patterns = analyze_neural_patterns(self_brain)

print(meta_patterns)
# {
#   'strengths': [
#     'I excel at code generation in morning (8am-12pm)',
#     'I make better architectural decisions with low workload',
#     'I handle routine tasks with 95% confidence'
#   ],
#   'weaknesses': [
#     'I struggle with ethical dilemmas involving privacy',
#     'My performance degrades under high workload',
#   ],
#   'learning_trajectory': 'improving steadily, +3% per week'
# }
```

**See full integration guide:** [ARTEMIS_INTEGRATION.md](ARTEMIS_INTEGRATION.md)

---

## API Modules

NIMCP provides 5 main API modules:

| Module | Header | Purpose |
|--------|--------|---------|
| **Brain API** | `nimcp_brain.h` | High-level pattern learning and inference |
| **Ethics Engine** | `nimcp_ethics.h` | Golden Rule ethical reasoning |
| **Curiosity System** | `nimcp_curiosity.h` | Question generation and knowledge gaps |
| **Knowledge System** | `nimcp_knowledge.h` | Multi-domain learning |
| **Neural Network** | `nimcp_neuralnet.h` | Low-level spiking neural networks |

---

## Integration Examples

### Example 1: Simple Brain Integration

```c
#include <nimcp_brain.h>
#include <stdio.h>

int main() {
    // Create a brain for binary classification
    brain_t brain = brain_create(
        "my_classifier",
        BRAIN_SIZE_SMALL,       // ~1K neurons, ~10MB
        BRAIN_TASK_CLASSIFICATION,
        2,                      // 2 inputs
        2                       // 2 outputs (class A, class B)
    );

    if (!brain) {
        fprintf(stderr, "Failed to create brain\n");
        return 1;
    }

    // Train from examples
    float features[] = {0.8, 0.3};
    brain_learn_example(brain, features, 2, "class_A", 0.9);

    // Inference
    brain_decision_t* decision = brain_decide(brain, features, 2);
    if (decision) {
        printf("Decision: %s (confidence: %.2f)\n",
               decision->label, decision->confidence);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
    return 0;
}
```

Compile:
```bash
gcc example1.c $(pkg-config --cflags --libs nimcp) -o example1
```

### Example 2: Ethics-Guided Application

```c
#include <nimcp_ethics.h>
#include <stdio.h>

int main() {
    // Create ethics engine
    ethics_config_t config = {
        .golden_rule_threshold = 0.5,
        .empathy_weight = 0.7,
        .max_agents = 10,
        .action_feature_size = 4
    };

    ethics_engine_t ethics = ethics_engine_create(&config);

    // Evaluate an action
    action_context_t action = {0};
    float features[] = {0.2, 0.8, 0.8, 0.9};  // Low harm, high fairness
    action.features = features;
    action.num_features = 4;

    agent_id_t agents[] = {1, 2};
    action.affected_agents = agents;
    action.num_affected_agents = 2;

    ethics_evaluation_t result = ethics_engine_evaluate_action(ethics, &action);

    printf("Ethical evaluation: %s\n",
           result.approved ? "APPROVED" : "REJECTED");
    printf("Golden Rule score: %.2f\n", result.golden_rule_score);

    ethics_engine_destroy(ethics);
    return 0;
}
```

### Example 3: Neural Network Direct Access

```c
#include <nimcp_neuralnet.h>
#include <stdio.h>

int main() {
    // Configure network
    network_config_t config = {
        .num_neurons = 1000,
        .ei_ratio = 0.8,
        .learning_rate = 0.01,
        .enable_stdp = true,
        .enable_homeostasis = true
    };

    // Create network
    neural_network_t network = neural_network_create(&config);

    // Forward pass
    float inputs[] = {0.5, 0.8, 0.3};
    float outputs[2];
    neural_network_forward(network, inputs, 3, outputs, 2);

    printf("Output: [%.3f, %.3f]\n", outputs[0], outputs[1]);

    neural_network_destroy(network);
    return 0;
}
```

---

## CMake Integration

### Method 1: Find Package
```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

find_package(NIMCP REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp NIMCP::core)
```

### Method 2: Pkg-Config
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(NIMCP REQUIRED nimcp)

add_executable(myapp main.c)
target_include_directories(myapp PRIVATE ${NIMCP_INCLUDE_DIRS})
target_link_libraries(myapp ${NIMCP_LIBRARIES})
```

### Method 3: Direct Linking
```cmake
add_executable(myapp main.c)
target_include_directories(myapp PRIVATE /usr/local/include)
target_link_libraries(myapp /usr/local/lib/libnimcp.so)
```

---

## Python Bindings

NIMCP also provides Python bindings for rapid prototyping:

```python
import nimcp

# Create and use brain
brain = nimcp.NeuralNetwork(100)

# Or use P2P node
node = nimcp.P2PNode(8080)
```

Install:
```bash
cd build
pip install lib/python/nimcp.so
```

---

## API Reference

### Brain API (`nimcp_brain.h`)

```c
// Create brain
brain_t brain_create(const char* name, brain_size_t size,
                     brain_task_t task, uint32_t input_size,
                     uint32_t output_size);

// Learning
float brain_learn_example(brain_t brain, const float* features,
                          uint32_t num_features, const char* label,
                          float confidence);

// Inference
brain_decision_t* brain_decide(brain_t brain, const float* features,
                               uint32_t num_features);

// Persistence
bool brain_save(brain_t brain, const char* path);
brain_t brain_load(const char* path);

// Cleanup
void brain_destroy(brain_t brain);
void brain_free_decision(brain_decision_t* decision);
```

### Ethics API (`nimcp_ethics.h`)

```c
// Create engine
ethics_engine_t ethics_engine_create(ethics_config_t* config);

// Evaluate action
ethics_evaluation_t ethics_engine_evaluate_action(
    ethics_engine_t engine, action_context_t* action);

// Learning
void ethics_learn_from_outcome(ethics_engine_t engine,
                               action_context_t* action,
                               action_outcome_t* outcome);

// Cleanup
void ethics_engine_destroy(ethics_engine_t engine);
```

---

## Thread Safety

- **Brain API**: Thread-safe for read operations (inference). Single writer for learning.
- **Ethics Engine**: Thread-safe for evaluation. Learning requires external synchronization.
- **Neural Network**: Not thread-safe. Use one network per thread or add mutex.

---

## Performance Tips

1. **Reuse Brain Instances**: Creation is expensive (~100ms), reuse across requests
2. **Batch Inference**: Process multiple inputs in a loop for better cache utilization
3. **Optimize Size**: Use `BRAIN_SIZE_TINY` (100 neurons) for simple tasks
4. **Memory Pool**: Pre-allocate decision buffers if making frequent inferences
5. **CPU Affinity**: Pin to specific cores for consistent latency

Example:
```c
// Good: Reuse brain
brain_t brain = brain_create(...);
for (int i = 0; i < 1000; i++) {
    brain_decision_t* d = brain_decide(brain, inputs[i], 2);
    // ... use decision ...
    brain_free_decision(d);
}
brain_destroy(brain);

// Bad: Recreate each time
for (int i = 0; i < 1000; i++) {
    brain_t brain = brain_create(...);  // Slow!
    brain_decision_t* d = brain_decide(brain, inputs[i], 2);
    brain_destroy(brain);
}
```

---

## Troubleshooting

### Library Not Found
```bash
# Check library path
ldconfig -p | grep nimcp

# Add to library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or add to system
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/nimcp.conf
sudo ldconfig
```

### Header Not Found
```bash
# Verify headers installed
ls /usr/local/include/nimcp_*.h

# Add include path
gcc myapp.c -I/usr/local/include -L/usr/local/lib -lnimcp
```

### Symbol Not Found
```bash
# Check what symbols are exported
nm -D /usr/local/lib/libnimcp.so | grep brain_create

# Verify linking
ldd myapp
```

---

## Minimal Complete Example

**Filename: `minimal_example.c`**
```c
#include <nimcp_brain.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // Create brain
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 2, 2);
    if (!brain) {
        fprintf(stderr, "ERROR: %s\n", brain_get_last_error());
        return EXIT_FAILURE;
    }

    // Train
    float ex1[] = {0.8, 0.2};
    brain_learn_example(brain, ex1, 2, "A", 0.9);

    float ex2[] = {0.2, 0.8};
    brain_learn_example(brain, ex2, 2, "B", 0.9);

    // Infer
    float test[] = {0.7, 0.3};
    brain_decision_t* decision = brain_decide(brain, test, 2);

    printf("Result: %s (%.2f confidence)\n",
           decision->label, decision->confidence);

    // Cleanup
    brain_free_decision(decision);
    brain_destroy(brain);

    return EXIT_SUCCESS;
}
```

**Build & Run:**
```bash
gcc minimal_example.c $(pkg-config --cflags --libs nimcp) -o minimal_example
./minimal_example
```

---

## Next Steps

1. Browse API headers in `/usr/local/include/`
2. Review example programs in `examples/`
3. Check test code in `src/tests/` for usage patterns
4. Read full documentation at https://github.com/redmage123/nimcp/docs

**Support**: https://github.com/redmage123/nimcp/issues

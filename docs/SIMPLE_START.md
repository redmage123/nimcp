# NIMCP Simple Start Guide - Phase 9

**Start Here!** This guide shows you the easiest ways to use NIMCP with clean, simple interfaces.

All examples include Phase 9 features:
- ✓ Epistemic filtering (bias prevention)
- ✓ Multi-modal processing
- ✓ Ethical reasoning
- ✓ Quality metrics

---

## Quick Start - Choose Your Interface

### 1. C Program (Simplest)

**File**: `examples/simple_demo.c` (170 lines)

```bash
# Build
cd build
make simple_demo

# Run
./examples/simple_demo
```

**What you'll see**:
- Brain initialization (1 line of code!)
- Training examples
- Multi-modal input processing
- Epistemic quality assessment
- Bias detection results

**Perfect for**: Learning NIMCP fundamentals, embedding in C projects

---

### 2. Python Script (Most Flexible)

**File**: `examples/simple_demo.py` (180 lines)

```bash
# Requirements
pip install numpy

# Run
cd examples
python3 simple_demo.py
```

**API is super simple**:
```python
import nimcp

# Create brain (1 line!)
brain = nimcp.Brain("demo", size=nimcp.BrainSize.SMALL,
                    task=nimcp.BrainTask.CLASSIFICATION,
                    num_inputs=8, num_outputs=3)

# Train (1 line!)
brain.learn(features=[1,0,1,0,1,0,1,0], label="pattern_A", confidence=0.9)

# Predict (1 line!)
result = brain.predict(features=[1,0,1,0,1,0,1,0])

print(f"Decision: {result['label']}")
print(f"Confidence: {result['confidence'] * 100:.1f}%")
print(f"Epistemic Quality: {result['epistemic_quality'] * 100:.1f}%")
print(f"Bias Detected: {result['bias_detected']}")
```

**Perfect for**: Scripting, integration with Python ML pipelines, Jupyter notebooks

---

### 3. Web UI (Most Visual)

**Files**: `examples/simple_web_demo.py` + `examples/simple_ui.html`

```bash
# Requirements
pip install flask flask-cors numpy

# Start server
cd examples
python3 simple_web_demo.py

# Open browser
http://localhost:5500
```

**Features**:
- ✓ Single-file HTML UI (no build process!)
- ✓ Clean REST API (no WebSockets complexity)
- ✓ Visual feedback with color-coded metrics
- ✓ Real-time updates

**UI is super clean**:
- Step 1: Initialize Brain
- Step 2: Train with examples
- Step 3: Make predictions
- Step 4: View metrics

**Perfect for**: Demos, teaching, visual exploration

---

### 4. Terminal UI (Most Interactive)

**File**: `examples/simple_terminal_ui.py` (450 lines)

```bash
# Requirements
pip install colorama numpy

# Run
cd examples
python3 simple_terminal_ui.py
```

**Features**:
- ✓ Interactive menu-driven interface
- ✓ Color-coded output (success=green, warning=yellow, error=red)
- ✓ Step-by-step guidance
- ✓ Training history viewer
- ✓ Real-time metrics

**Perfect for**: Command-line users, SSH access, no GUI needed

---

## Comparison Table

| Interface | Lines of Code | Build Required? | Best For |
|-----------|---------------|-----------------|----------|
| **C Demo** | 170 | Yes (CMake) | Learning, embedding |
| **Python Script** | 180 | No | Scripting, automation |
| **Web UI** | 300 total | No | Visual demos, teaching |
| **Terminal UI** | 450 | No | Interactive CLI use |

---

## What's Different from the Complex Examples?

### Old Way (Complex):
- `integrated_learning_demo.c` - 438 lines, shows everything at once
- `brain_demo.c` - 423 lines, advanced features
- Web demo - 3,685 lines across 12 files, React build process
- Steep learning curve

### New Way (Simple):
- **One concept at a time**
- **Clear comments explaining every step**
- **Clean, readable output**
- **Easy to modify and experiment**
- **No framework complexity**

---

## Key API Functions - All You Need

### C API (Brain-Level)

```c
// Create
brain_t brain = brain_create(name, size, task, num_inputs, num_outputs);

// Train
bool brain_learn_example(brain_t brain, float* features, uint32_t dim,
                         const char* label, float confidence);

// Predict
bool brain_process_multimodal(brain_t brain,
                              const brain_multimodal_input_t* input,
                              brain_multimodal_output_t* output);

// Cleanup
void brain_destroy(brain_t brain);
```

### Python API

```python
# Create
brain = nimcp.Brain(name, size, task, num_inputs, num_outputs)

# Train
brain.learn(features, label, confidence)

# Predict
result = brain.predict(features)

# Metrics
metrics = brain.get_metrics()
```

---

## Understanding the Output

### Confidence Score (0-1)
- **>0.7**: High confidence - trust the prediction
- **0.4-0.7**: Moderate confidence - consider verification
- **<0.4**: Low confidence - needs more training

### Epistemic Quality (0-1)
- **>0.7**: High quality evidence-based decision
- **0.4-0.7**: Moderate quality - some uncertainty
- **<0.4**: Low quality - insufficient evidence

### Bias Detection
- **False**: ✓ No cognitive biases detected
- **True**: ⚠ Potential bias (Dunning-Kruger, confirmation bias, etc.)

### Requires Verification
- **False**: ✓ Prediction is reliable
- **True**: ⚠ Should verify before trusting

### Ethical Approval
- **True**: ✓ Passes Golden Rule evaluation
- **False**: ✗ Potential ethical concern

---

## Example Workflows

### Workflow 1: Pattern Classification

```bash
# 1. Start the terminal UI
python3 simple_terminal_ui.py

# 2. Initialize: 8 inputs, 3 classes

# 3. Train patterns:
#    1,0,1,0,1,0,1,0 → pattern_A
#    0,1,0,1,0,1,0,1 → pattern_B
#    1,1,0,0,1,1,0,0 → pattern_C

# 4. Predict:
#    1,0,1,0,1,0,1,0 → Should predict pattern_A with high confidence
```

### Workflow 2: Testing Epistemic Filter

```bash
# Train with high-quality data
brain.learn([1,0,1,0,1,0,1,0], "pattern_A", confidence=0.95)

# Test with ambiguous input (should flag for verification)
result = brain.predict([0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5])

# Expected:
# - Low epistemic_quality (<0.5)
# - requires_verification = True
# - Lower confidence
```

### Workflow 3: Multi-Modal Input

```c
// Prepare inputs
brain_multimodal_input_t input = {0};
input.visual_data = image_pixels;  // 8x8 grayscale
input.visual_width = 8;
input.visual_height = 8;
input.audio_data = audio_samples;  // 16 samples
input.audio_samples = 16;
input.direct_data = features;      // 8 features
input.direct_dim = 8;

// Process
brain_multimodal_output_t output = {0};
brain_process_multimodal(brain, &input, &output);

// Check results
printf("Visual attention: %.1f%%\n", output.visual_attention * 100);
printf("Audio attention: %.1f%%\n", output.audio_attention * 100);
printf("Direct attention: %.1f%%\n", output.direct_attention * 100);
```

---

## Next Steps

After trying the simple examples:

1. **Modify them** - Change input dimensions, add more classes
2. **Combine them** - Use Python script to generate training data
3. **Build your own** - Use the simple code as a template
4. **Read the docs** - Check `docs/SYSTEMS_ANALYSIS_2025.md` for architecture details
5. **Try advanced examples** - `integrated_learning_demo.c`, `brain_demo.c`

---

## Troubleshooting

### "NIMCP Python bindings not found"
```bash
cd build
make
# Bindings are in: build/lib/python/
```

### "Brain initialization failed"
- Check input/output dimensions match your data
- Ensure brain size is appropriate (SMALL=1K neurons usually fine)

### "Low epistemic quality"
- Normal for ambiguous inputs
- Means: "I'm not confident, needs verification"
- Train with more examples to improve

### Web UI won't start
```bash
# Install dependencies
pip install flask flask-cors

# Check port 5500 is free
lsof -i :5500

# Run from examples/ directory
cd examples
python3 simple_web_demo.py
```

---

## File Locations

```
examples/
├── simple_demo.c              # C reference implementation
├── simple_demo.py             # Python script example
├── simple_web_demo.py         # Web backend (Flask)
├── simple_ui.html             # Web frontend (single file!)
├── simple_terminal_ui.py      # Interactive terminal UI
└── ... (24 other advanced examples)

docs/
├── SIMPLE_START.md            # This file
├── SYSTEMS_ANALYSIS_2025.md   # Technical architecture
└── README.md                  # Full documentation
```

---

## Summary

**3 Easy Ways to Use NIMCP**:

1. **C Program**: `./simple_demo` - 170 lines, shows everything
2. **Python Script**: `python3 simple_demo.py` - 180 lines, flexible API
3. **Web UI**: `python3 simple_web_demo.py` - Visual, interactive
4. **Terminal UI**: `python3 simple_terminal_ui.py` - Menu-driven CLI

**All include**:
- ✓ Phase 9 epistemic filtering
- ✓ Bias detection
- ✓ Quality metrics
- ✓ Clean, simple code
- ✓ Easy to understand

**Start with** simple_demo.c or simple_demo.py depending on your preference!

---

**Questions?** Check `README.md` or `docs/SYSTEMS_ANALYSIS_2025.md`

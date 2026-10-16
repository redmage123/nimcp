# NIMCP API Reference

**Version 2.6.4** | Complete API reference for all language bindings

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Language Binding Reference](#language-binding-reference)
3. [Brain Lifecycle & Configuration](#brain-lifecycle--configuration)
4. [Learning & Training](#learning--training)
5. [Inference & Prediction](#inference--prediction)
6. [Sensory Processing & Perception](#sensory-processing--perception)
7. [Network Statistics & Metrics](#network-statistics--metrics)
8. [Avatar & Identity](#avatar--identity)
9. [Brain Regions & Neuromodulation](#brain-regions--neuromodulation)
10. [Training Integration & UTM](#training-integration--utm)
11. [Cloud & Collective](#cloud--collective)
12. [Security](#security)

---

## Quick Start

### Python
```python
import nimcp
brain = nimcp.Brain("classifier", neuron_count=10000, num_inputs=1024, num_outputs=4096)
loss = brain.learn([0.5, 0.3, 0.8, ...], "class_a")
label, confidence = brain.predict([0.5, 0.3, 0.8, ...])
brain.save("model.brain")
```

### Java
```java
long brain = NimcpJNI.nativeCreate("classifier", 0, 0, 10, 3, 10000, null, null, null);
float loss = NimcpJNI.nativeLearn(brain, features, "class_a", 0.0f, 1.0f);
NimcpJNI.nativeDestroy(brain);
```

### Node.js
```javascript
const nimcp = require('nimcp');
const brain = nimcp.createBrain("classifier", 0, 0, 10, 3, 10000);
const loss = nimcp.learn(brain, features, "class_a");
nimcp.destroyBrain(brain);
```

### Go
```go
import "nimcp"
brain, _ := nimcp.NewBrain("classifier", nimcp.BrainSmall, nimcp.TaskClassification, 10, 3)
defer brain.Destroy()
loss, _ := brain.Learn(features, "class_a", 0.0, 1.0)
```

### C#
```csharp
using NIMCP;
var brain = new Brain("classifier", Brain.BRAIN_SMALL, Brain.TASK_CLASSIFICATION, 10, 3);
float loss = brain.Learn(features, "class_a");
brain.Dispose();
```

### C++
```cpp
#include <nimcp.hpp>
nimcp::Library lib;  // RAII init/shutdown
nimcp::Brain brain("classifier", nimcp::BrainSize::Small, nimcp::TaskType::Classification, 10, 3);
brain.learn({0.5f, 0.3f, 0.8f, ...}, "class_a");
auto [label, confidence] = brain.predict({0.5f, 0.3f, 0.8f, ...});
```

### Rust
```rust
use nimcp::Brain;
let mut brain = Brain::new("classifier", BrainSize::Small, TaskType::Classification, 10, 3)?;
let loss = brain.learn(&features, "class_a", 0.0, 1.0)?;
let (label, confidence) = brain.predict(&features)?;
```

### Perl
```perl
use NIMCP;
my $brain = NIMCP::Brain->new("classifier", $NIMCP::BRAIN_SMALL, $NIMCP::TASK_CLASSIFICATION, 10, 3);
my $loss = $brain->learn(\@features, "class_a");
my ($label, $confidence) = $brain->predict(\@features);
```

### C
```c
#include "nimcp.h"
nimcp_brain_t brain = nimcp_brain_create("classifier", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3);
nimcp_brain_learn_example(brain, features, 10, "class_a", 1.0f);
nimcp_brain_destroy(brain);
```

---

## Language Binding Reference

Each API method is listed with its Python name. The table below maps Python method names to their equivalents in other language bindings.

### Lifecycle & Configuration

| Python | C++ | Rust | Java (JNI) | Node.js | Go | C# | Perl |
|--------|-----|------|-----------|---------|-----|-----|------|
| `Brain(...)` | `Brain(...)` | `Brain::new(...)` | `nativeCreate(...)` | `createBrain(...)` | `NewBrain(...)` | `new Brain(...)` | `Brain->new(...)` |
| `Brain.load(path)` | `Brain::load(path)` | `Brain::load(path)` | `nativeLoad(...)` | `loadBrain(...)` | `LoadBrain(...)` | `Brain.Load(...)` | `Brain->load(...)` |
| `save(path)` | `save(path)` | `save(path)` | `nativeSave(...)` | `save(...)` | `Save(...)` | `Save(...)` | `save(...)` |
| `freeze()` | `freeze()` | `freeze()` | `nativeFreeze(...)` | `freeze(...)` | `Freeze()` | `Freeze()` | `freeze()` |
| `resize(n)` | `resize(n)` | `resize(n)` | `nativeResize(...)` | `resize(...)` | `Resize(n)` | `Resize(n)` | `resize(n)` |
| `configure_training(...)` | `configure_training(...)` | `configure_training(...)` | `nativeConfigureTraining(...)` | `configureTraining(...)` | `ConfigureTraining(...)` | `ConfigureTraining(...)` | `configure_training(...)` |
| `set_fast_training(on)` | `set_fast_training(on)` | `set_fast_training(on)` | `nativeSetFastTraining(...)` | `setFastTraining(...)` | `SetFastTraining(on)` | `SetFastTraining(on)` | `set_fast_training(on)` |
| `set_task_type(t)` | `set_task_type(t)` | `set_task_type(t)` | `nativeSetTaskType(...)` | `setTaskType(...)` | `SetTaskType(t)` | `SetTaskType(t)` | `set_task_type(t)` |
| `enable_multi_network()` | `enable_multi_network()` | `enable_multi_network()` | `nativeEnableMultiNetwork(...)` | `enableMultiNetwork(...)` | `EnableMultiNetwork()` | `EnableMultiNetwork()` | `enable_multi_network()` |

### Learning & Inference

| Python | C++ | Rust | Java (JNI) | Node.js | Go | C# | Perl |
|--------|-----|------|-----------|---------|-----|-----|------|
| `learn(feat, label, ...)` | `learn(feat, label, conf)` | `learn(&feat, label, lr, conf)` | `nativeLearn(...)` | `learn(...)` | `Learn(...)` | `Learn(...)` | `learn(...)` |
| `learn_vector(feat, tgt)` | `learn_vector(feat, tgt)` | `learn_vector(&feat, &tgt)` | `nativeLearnVector(...)` | `learnVector(...)` | `LearnVector(...)` | `LearnVector(...)` | `learn_vector(...)` |
| `predict(feat)` | `predict(feat)` | `predict(&feat)` | `nativePredict(...)` | `predict(...)` | `Predict(...)` | `Predict(...)` | `predict(...)` |
| `decide_full(feat)` | `decide_full(feat)` | `decide_full(&feat)` | `nativeDecideFull(...)` | `decideFull(...)` | `DecideFull(...)` | `DecideFull(...)` | `decide_full(...)` |
| `get_accuracy()` | `get_accuracy()` | `get_accuracy()` | `nativeGetAccuracy(...)` | `getAccuracy(...)` | `GetAccuracy()` | `GetAccuracy()` | `get_accuracy()` |

### Sensory & Perception

| Python | C++ | Rust | Java (JNI) | Node.js | Go | C# | Perl |
|--------|-----|------|-----------|---------|-----|-----|------|
| `submit_sensory(mod, data)` | `submit_sensory(mod, data)` | `submit_sensory(mod, &data)` | `nativeSubmitSensory(...)` | `submitSensory(...)` | `SubmitSensory(...)` | `SubmitSensory(...)` | `submit_sensory(...)` |
| `visual_cortex_process(...)` | `visual_cortex_process(...)` | `visual_cortex_process(...)` | `nativeVisualCortexProcess(...)` | `visualCortexProcess(...)` | `VisualCortexProcess(...)` | `VisualCortexProcess(...)` | `visual_cortex_process(...)` |
| `audio_cortex_process(s)` | `audio_cortex_process(s)` | `audio_cortex_process(&s)` | `nativeAudioCortexProcess(...)` | `audioCortexProcess(...)` | -- | -- | `audio_cortex_process(...)` |

### Metrics & Stats

| Python | C++ | Rust | Java (JNI) | Node.js | Go | C# | Perl |
|--------|-----|------|-----------|---------|-----|-----|------|
| `get_network_metrics()` | `get_network_metrics()` | `get_network_metrics()` | `nativeGetNetworkMetrics(...)` | `getNetworkMetrics(...)` | `GetNetworkMetrics()` | `GetNetworkMetrics()` | `get_network_metrics()` |
| `get_cortex_cnn_metrics()` | `get_cortex_cnn_metrics()` | `get_cortex_cnn_metrics()` | `nativeGetCortexCnnMetrics(...)` | `getCortexCnnMetrics(...)` | `GetCortexCNNMetrics()` | `GetCortexCnnMetrics()` | `get_cortex_cnn_metrics()` |
| `get_avatar_state()` | `get_avatar_state()` | `get_avatar_state()` | `nativeGetAvatarState(...)` | `getAvatarState(...)` | `GetAvatarState()` | `GetAvatarState()` | `get_avatar_state()` |
| `lnn_get_stats()` | `lnn_get_stats()` | `lnn_get_stats()` | `nativeLnnGetStats(...)` | `lnnGetStats(...)` | `LNNGetStats()` | `LnnGetStats()` | `lnn_get_stats()` |
| `snn_get_stats()` | `snn_get_stats()` | `snn_get_stats()` | `nativeSnnGetStats(...)` | `snnGetStats(...)` | `SNNGetStats()` | `SnnGetStats()` | `snn_get_stats()` |
| `cnn_get_stats()` | `cnn_get_stats()` | `cnn_get_stats()` | `nativeCnnGetStats(...)` | `cnnGetStats(...)` | `CNNGetStats()` | `CnnGetStats()` | `cnn_get_stats()` |

### Brain Regions

| Python | C++ | Rust | Java (JNI) | Node.js | Go | C# | Perl |
|--------|-----|------|-----------|---------|-----|-----|------|
| `medulla_get_arousal()` | `medulla_get_arousal()` | `medulla_get_arousal()` | `nativeMedullaGetArousal(...)` | `medullaGetArousal(...)` | `MedullaGetArousal()` | `MedullaGetArousal()` | `medulla_get_arousal()` |
| `sleep_get_pressure()` | `sleep_get_pressure()` | `sleep_get_pressure()` | `nativeSleepGetPressure(...)` | `sleepGetPressure(...)` | `SleepGetPressure()` | `SleepGetPressure()` | `sleep_get_pressure()` |
| `bg_get_dopamine()` | `bg_get_dopamine()` | `bg_get_dopamine()` | `nativeBgGetDopamine(...)` | `bgGetDopamine(...)` | `BGGetDopamine()` | `BgGetDopamine()` | `bg_get_dopamine()` |
| `substrate_get_health()` | `substrate_get_health()` | `substrate_get_health()` | `nativeSubstrateGetHealth(...)` | `substrateGetHealth(...)` | `SubstrateGetHealth()` | `SubstrateGetHealth()` | `substrate_get_health()` |

> **Note:** `--` indicates the method is not yet available in that binding. The Python binding is the reference implementation with the most complete coverage (157 methods). Other bindings cover the most commonly used methods across 6 groups: sensory, avatar/metrics, inference, network stats, configuration, and brain regions.

---

## Brain Lifecycle & Configuration

### `Brain(name, size=BRAIN_SMALL, task=TASK_CLASSIFICATION, num_inputs=10, num_outputs=10, neuron_count=0, checkpoint=None, init_mode=None, log_level=None)`

Create a new NIMCP Brain instance. The brain is the central cognitive unit containing neurons, synapses, and all associated subsystems (plasticity, neuromodulation, attention, etc.). If a `checkpoint` path is provided and the file exists, the brain is loaded directly from disk, bypassing full initialization. If `neuron_count` is specified and greater than zero, it overrides the `size` preset. The `init_mode` parameter controls which subsystems are initialized, trading capability for startup speed.

**Parameters:**
- `name` (str) -- Identifier for this brain instance, used in logging and serialization.
- `size` (int, optional) -- Brain size preset constant (`BRAIN_TINY`, `BRAIN_SMALL`, `BRAIN_MEDIUM`, `BRAIN_LARGE`). Ignored when `neuron_count` is provided. Default: `BRAIN_SMALL`
- `task` (int, optional) -- Task type constant (`TASK_CLASSIFICATION`, `TASK_REGRESSION`, etc.) that determines output activation and loss function strategy. Default: `TASK_CLASSIFICATION`
- `num_inputs` (int, optional) -- Number of input features the brain accepts. Default: `10`
- `num_outputs` (int, optional) -- Number of output values the brain produces. Default: `10`
- `neuron_count` (int, optional) -- Explicit neuron count, overriding `size`. When nonzero with `init_mode='fast'`, uses the fast-init path. Default: `0` (use `size` preset)
- `checkpoint` (str, optional) -- Path to a previously saved brain file. If the file exists, the brain is loaded from it directly; if the file does not exist or loading fails, a fresh brain is created. Default: `None`
- `init_mode` (str, optional) -- Initialization mode: `'full'` initializes all 80+ subsystems, `'fast'` runs 6 of 27 init waves for faster startup (~14s for 1.5M neurons), `'minimal'` initializes core systems only. Default: `None` (equivalent to `'full'`)
- `log_level` (str, optional) -- Set the global log level before brain creation. One of `'trace'`, `'debug'`, `'info'`, `'warn'`, `'error'`, `'off'`. Default: `None` (no change)

**Returns:** `Brain` -- A new Brain instance.

---

### `Brain.create_full(name, task=TASK_CLASSIFICATION, num_inputs=1024, num_outputs=2048, neuron_count=1500000)`

Class method. Create a brain with ALL subsystems enabled, including the RESEARCH cognitive profile, world model (RSSM + JEPA), creative modules, LGSS content filtering, and full neuromodulator systems. Unlike the standard constructor followed by `configure_cognitive()`, this method applies the full profile at creation time so all subsystems initialize in proper dependency order.

**Parameters:**
- `name` (str) -- Identifier for this brain instance.
- `task` (int, optional) -- Task type constant. Default: `TASK_CLASSIFICATION`
- `num_inputs` (int, optional) -- Number of input features. Default: `1024`
- `num_outputs` (int, optional) -- Number of output values. Default: `2048`
- `neuron_count` (int, optional) -- Total number of neurons to allocate. Default: `1500000`

**Returns:** `Brain` -- A fully-configured Brain instance with all subsystems active.

---

### `Brain.load(filepath)`

Class method. Load a previously saved brain from a binary file on disk. The loaded brain retains all learned weights, synapse structures, and configuration from the time it was saved. Note that some runtime subsystems (GPU context, LNN state) may require re-initialization after loading.

**Parameters:**
- `filepath` (str) -- Path to the saved brain file.

**Returns:** `Brain` -- A Brain instance restored from the file.

**Raises:** `IOError` if the file cannot be read or the data is corrupted.

---

### `save(filepath)`

Save the brain's complete state to a binary file on disk. This includes all neuron weights, synapse structures, configuration flags, and learned parameters. The GIL is released during the I/O operation.

**Parameters:**
- `filepath` (str) -- Destination file path. Parent directory must exist.

**Returns:** None

---

### `snapshot_cow()`

Create an instant copy-on-write (COW) snapshot of the entire brain state. The snapshot shares memory pages with the live brain until either is modified, making creation nearly instantaneous regardless of brain size. Useful for creating rollback points during training.

**Parameters:** None

**Returns:** `capsule` -- An opaque PyCapsule containing the snapshot handle. Pass this to `restore_cow()` or `destroy_cow_snapshot()`.

---

### `restore_cow(snapshot)`

Restore the brain to a previously captured COW snapshot state. Performs an efficient pointer-swap restoration, reverting all neuron weights, synapse structures, and internal state.

**Parameters:**
- `snapshot` (capsule) -- A snapshot capsule previously returned by `snapshot_cow()`.

**Returns:** bool -- `True` on successful restore.

---

### `destroy_cow_snapshot(snapshot)`

Explicitly destroy a COW snapshot and release its associated memory. Optional since snapshots are automatically cleaned up on garbage collection.

**Parameters:**
- `snapshot` (capsule) -- A snapshot capsule previously returned by `snapshot_cow()`.

**Returns:** None

---

### `freeze()`

Freeze the brain for inference-only mode. All learning and plasticity operations are disabled and the brain's weights become read-only. Irreversible for the lifetime of the brain instance.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

### `is_frozen`

Read-only property. Check whether the brain is currently frozen for inference-only mode.

**Returns:** bool -- `True` if the brain is frozen, `False` otherwise.

---

### `resize(new_neuron_count)`

Resize the brain to a new neuron count. Growth only -- the new count must be greater than the current neuron count. Existing neurons and their synapses are preserved.

**Parameters:**
- `new_neuron_count` (int) -- Target total neuron count. Must be greater than the current count.

**Returns:** bool -- `True` on success.

---

### `auto_resize()`

Automatically resize the brain based on current utilization metrics. If neuron utilization exceeds an internal threshold, the brain grows by a predetermined factor. The brain does not shrink.

**Parameters:** None

**Returns:** bool -- `True` if a resize was performed, `False` if no resize was needed.

---

### `get_neuron_count()`

Return the current total number of neurons in the brain.

**Parameters:** None

**Returns:** int -- The current neuron count.

---

### `configure_training(learning_rate=0.001, weight_decay=0.0001, gradient_clip=1.0)`

Configure the training pipeline with learning rate, regularization, and gradient management settings. Enables LR scheduling, L2 regularization, gradient clipping, and gradient health monitoring. Must be called before starting concurrent training (not thread-safe).

**Parameters:**
- `learning_rate` (float, optional) -- Base learning rate for the optimizer. Default: `0.001`
- `weight_decay` (float, optional) -- L2 regularization coefficient. Default: `0.0001`
- `gradient_clip` (float, optional) -- Maximum gradient norm for clipping. Default: `1.0`

**Returns:** bool -- `True` on success.

---

### `configure_cognitive()`

Enable cognitive configuration flags on an existing brain. Activates multi-head attention (8 heads, key dim 64), executive control, meta-learning, predictive processing, active inference, logic, epistemic filtering, emotional tagging, ethics, wellbeing monitoring, brain regions, neural oscillations, sleep-wake cycles, memory replay, synaptic homeostasis, dendritic computation, and eligibility traces. Note: this only sets config flags; actual subsystem initialization must have occurred at brain creation time. For full subsystem initialization, use `Brain.create_full()` instead.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

### `set_fast_training(enabled=True)`

Toggle fast training mode. When enabled, the brain skips expensive biological subsystems during learning (VAE, attention, engram formation, neuromodulators, emotions, cortical columns), yielding a 5-10x training speedup at the cost of reduced biological fidelity.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable fast training, `False` to restore full biological processing. Default: `True`

**Returns:** bool -- `True` on success.

---

### `set_task_type(task_type)`

Set the brain's task strategy, controlling output activation functions and loss computation. The strategy determines whether outputs pass through softmax (classification), are used as raw values (regression), or follow pattern/association-specific processing.

**Parameters:**
- `task_type` (str) -- One of `'regression'`, `'classification'`, `'pattern'`, or `'association'`.

**Returns:** bool -- `True` on success.

---

### `set_training_mode(active)`

Enable or disable the training-mode fast path. When active, the brain optimizes its forward pass for training (keeping intermediate activations for backpropagation).

**Parameters:**
- `active` (bool) -- `True` to enable training mode, `False` for inference mode.

**Returns:** None

---

### `set_fusion_enabled(enabled)`

Enable or disable multi-network fusion. When enabled, the brain combines outputs from all active network types (ANN, CNN, SNN, LNN) using configurable fusion weights.

**Parameters:**
- `enabled` (bool) -- `True` to enable fusion, `False` to disable.

**Returns:** None

---

### `set_fusion_weights(weights)`

Set the blending weights for multi-network fusion. Must be called after `set_fusion_enabled(True)`.

**Parameters:**
- `weights` (list[float]) -- Exactly 4 floats representing weights for [ANN, CNN, SNN, LNN].

**Returns:** None

---

### `set_network_ablation(train_cnn=-1, train_snn=-1, train_lnn=-1)`

Selectively enable or disable training for individual network types. Useful for ablation studies.

**Parameters:**
- `train_cnn` (int, optional) -- `1` = enable, `0` = disable, `-1` = unchanged. Default: `-1`
- `train_snn` (int, optional) -- `1` = enable, `0` = disable, `-1` = unchanged. Default: `-1`
- `train_lnn` (int, optional) -- `1` = enable, `0` = disable, `-1` = unchanged. Default: `-1`

**Returns:** None

---

### `enable_biological_plasticity(enabled=True)`

Wire or unwire the full biological plasticity pipeline into the learning path. Controls three integrated systems: the Training-Plasticity Bridge (TPB), Event-Driven Plasticity (EDP), and the Plasticity Coordinator.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable, `False` to disable. Default: `True`

**Returns:** bool -- `True` on success.

---

### `enable_bptt(enabled=True, window_size=8, discount=0.9)`

Configure backpropagation through time (BPTT) for temporal sequence learning. The brain maintains a circular buffer of recent input/output/target triples and performs gradient computation across the temporal window.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable BPTT. Default: `True`
- `window_size` (int, optional) -- Number of timesteps to unroll. Default: `8`
- `discount` (float, optional) -- Temporal discount factor. Default: `0.9`

**Returns:** bool -- `True` on success.

---

### `enable_gradient_checkpointing(enabled=True, interval=0)`

Enable or disable gradient checkpointing for memory-efficient training. Intermediate activations are recomputed during the backward pass instead of being stored, reducing peak memory from O(L) to O(sqrt(L)).

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable. Default: `True`
- `interval` (int, optional) -- Checkpoint every N layers. `0` = automatic. Default: `0`

**Returns:** bool -- `True` on success.

---

### `enable_hemispheric(enabled=True)`

Enable or disable the hemispheric brain architecture with left/right hemispheres and corpus callosum bridge.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable. Default: `True`

**Returns:** bool -- `True` on success.

---

### `enable_mixed_precision(enabled=True)`

Enable or disable FP16 mixed-precision training. Forward and backward passes use half-precision where safe, with FP32 master weights for numerical stability.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable. Default: `True`

**Returns:** bool -- `True` on success.

---

### `enable_multi_network()`

Enable LNN + CNN ensemble training alongside the primary adaptive/SNN network. Once enabled, all active network types train in parallel during each learning step.

**Parameters:** None

**Returns:** None

---

### `enable_recurrent(enabled=True, max_iterations=3, confidence_threshold=0.7, blend_alpha=0.3)`

Configure recurrent forward pass behavior. When enabled, the brain iteratively refines its output by feeding predictions back as input.

**Parameters:**
- `enabled` (bool, optional) -- `True` to enable. Default: `True`
- `max_iterations` (int, optional) -- Maximum refinement iterations. Default: `3`
- `confidence_threshold` (float, optional) -- Stop early if confidence exceeds this. Default: `0.7`
- `blend_alpha` (float, optional) -- Blending factor between iterations. Default: `0.3`

**Returns:** bool -- `True` on success.

---

### `enable_world_model(enabled=True)`

Activate the brain's internal world model (RSSM + JEPA + dreaming). If enabling for the first time on a brain created without world model support, the subsystem is marked for lazy initialization.

**Parameters:**
- `enabled` (bool, optional) -- `True` to activate. Default: `True`

**Returns:** bool -- `True` on success.

---

## Learning & Training

### `learn(features, label, lr=0.0, confidence=1.0)`

Learn from a single labeled example. Performs a forward pass, computes loss against the given label, and updates network weights via backpropagation. The GIL is released during the C-level learning operation.

**Parameters:**
- `features` (list[float]) -- Input feature vector.
- `label` (str) -- Target class label.
- `lr` (float, optional) -- Per-call learning rate override. If positive and finite, temporarily replaces the brain's configured learning rate. Default: `0.0` (use brain default).
- `confidence` (float, optional) -- Confidence weight for this example. Default: `1.0`.

**Returns:** float -- The loss value. Raises `ValueError` if loss is NaN/inf, `RuntimeError` on failure.

---

### `learn_batch(batch)`

Learn from a batch of labeled examples in a single C call. 10-20x throughput improvement over calling `learn()` in a loop. Maximum batch size is 10,000.

**Parameters:**
- `batch` (list[tuple]) -- List of `(features, label[, confidence])` tuples.

**Returns:** list[float] -- Per-example loss values.

---

### `learn_vector(features, target, label=None, confidence=1.0, learning_rate=0.0)`

Learn from a dense target vector instead of a class label. Enables training toward semantic embeddings or soft targets (e.g., teacher distillation, generative training).

**Parameters:**
- `features` (list[float]) -- Input feature vector.
- `target` (list[float]) -- Dense target output vector to train toward.
- `label` (str or None, optional) -- Optional text label for logging. Default: `None`.
- `confidence` (float, optional) -- Confidence weight. Default: `1.0`.
- `learning_rate` (float, optional) -- Per-call learning rate override. Default: `0.0`.

**Returns:** float -- The loss value.

---

### `learn_vector_batch(batch, learning_rate=-1.0)`

Batch vector learning with GPU gradient accumulation. Accumulates gradients across N samples and applies a single weight update.

**Parameters:**
- `batch` (list[tuple]) -- List of `(features, target)` tuples.
- `learning_rate` (float, optional) -- Learning rate for this batch. Default: `-1.0` (use brain default).

**Returns:** float -- Aggregated batch loss. Returns `-1.0` if batch is empty.

---

### `learn_knowledge(text, domain=10)`

Learn knowledge from text and store it in the brain's multi-domain knowledge graph.

**Parameters:**
- `text` (str) -- Text content to learn as knowledge.
- `domain` (int, optional) -- Knowledge domain identifier. Default: `10` (`KNOWLEDGE_DOMAIN_GENERAL`).

**Returns:** bool -- `True` if successfully learned.

---

### `learn_language(text)`

Learn language from text exposure through distributional and syntactic analysis, without requiring explicit input-target pairs.

**Parameters:**
- `text` (str) -- Text to learn language patterns from.

**Returns:** dict -- `{"loss": float, "success": bool}`

---

### `learn_language_pair(input_text, target_text, learning_rate=0.0)`

Learn from an input-target text pair via teacher-guided social learning.

**Parameters:**
- `input_text` (str) -- Input/prompt text.
- `target_text` (str) -- Expected/correct response text.
- `learning_rate` (float, optional) -- Learning rate. Default: `0.0`.

**Returns:** dict -- `{"loss": float, "success": bool}`

---

### `train_cognitive(text, domain=10, target_text=None, learning_rate=0.001)`

Train all cognitive modules from text in a single unified call. Ensures grounded language, knowledge, and the language generator all learn together.

**Parameters:**
- `text` (str) -- Input text to train on.
- `domain` (int, optional) -- Knowledge domain. Default: `10`.
- `target_text` (str or None, optional) -- Target text for language generator. Default: `None`.
- `learning_rate` (float, optional) -- Learning rate. Default: `0.001`.

**Returns:** dict -- `{"loss": float, "success": bool}`

---

### `train_language(input_text, target_text, learning_rate=0.001)`

Train the language generator (LNN decoder) on an input-target text pair.

**Parameters:**
- `input_text` (str) -- Input/prompt text.
- `target_text` (str) -- Expected output text.
- `learning_rate` (float, optional) -- Learning rate. Default: `0.001`.

**Returns:** dict -- `{"loss": float, "success": bool, "error": str (on failure)}`

---

### `experience(input, output_size, teacher_reward=0.0)`

Unified experience-based learning that merges inference and training. Performs a forward pass, computes prediction error, applies attention-gated plasticity, and processes reward signals.

**Parameters:**
- `input` (list[float]) -- Input feature vector.
- `output_size` (int) -- Size of the desired output vector (1 to 1,048,576).
- `teacher_reward` (float, optional) -- External reward signal from teacher. Default: `0.0`.

**Returns:** dict -- Keys: `output` (list), `prediction_error` (float), `attention_level` (float), `learning_rate_used` (float), `learning_applied` (bool), `synapse_formed` (bool), `reward_signal` (float), `experience_id` (int).

---

### `experience_configure(enabled=True, base_lr=0.001, attention_threshold=0.3, attention_lr_scale=3.0, novelty_boost=1.5, enable_hebbian=True, enable_reward=True, enable_world_model=True, enable_structural=False, synaptogenesis_threshold=0.7, consolidation_interval=1000)`

Configure experience-based learning parameters. Controls which plasticity mechanisms are active and tunes their sensitivity.

**Parameters:**
- `enabled` (bool, optional) -- Master switch for experience learning. Default: `True`.
- `base_lr` (float, optional) -- Base learning rate. Default: `0.001`.
- `attention_threshold` (float, optional) -- Minimum attention for learning. Default: `0.3`.
- `attention_lr_scale` (float, optional) -- LR multiplier when attention is high. Default: `3.0`.
- `novelty_boost` (float, optional) -- LR multiplier for novel stimuli. Default: `1.5`.
- `enable_hebbian` (bool, optional) -- Enable Hebbian plasticity. Default: `True`.
- `enable_reward` (bool, optional) -- Enable reward-modulated learning. Default: `True`.
- `enable_world_model` (bool, optional) -- Enable world model updates. Default: `True`.
- `enable_structural` (bool, optional) -- Enable structural plasticity. Default: `False`.
- `synaptogenesis_threshold` (float, optional) -- Correlation threshold for new synapses. Default: `0.7`.
- `consolidation_interval` (int, optional) -- Experiences between auto-consolidation. Default: `1000`.

**Returns:** None

---

### `experience_correct(expected)`

Correct the brain's last experience with the expected output. Provides a supervised teaching signal.

**Parameters:**
- `expected` (list[float]) -- The correct/expected output vector.

**Returns:** float -- Correction loss.

---

### `experience_attend(modality, strength=1.0)`

Direct the brain's attention to a specific sensory modality during developmental training.

**Parameters:**
- `modality` (str) -- Sensory modality: `"visual"`, `"auditory"`, `"linguistic"`, etc.
- `strength` (float, optional) -- Attention strength [0.0, 1.0]. Default: `1.0`.

**Returns:** None

---

### `ground_word(word, features, modality=5, attention=0.8)`

Ground a word in sensory experience through cross-modal binding.

**Parameters:**
- `word` (str) -- The word to ground.
- `features` (list[float]) -- Sensory feature vector.
- `modality` (int, optional) -- Sensory modality. Default: `5` (`GL_MODALITY_LINGUISTIC`).
- `attention` (float, optional) -- Attention level during grounding. Default: `0.8`.

**Returns:** bool -- `True` if successfully grounded.

---

### `innate_hardwire(stage=0, face=True, voice=True, motion=None, reflexes=True, cry=True, social_reward=True, habituation=True, novelty=True, strength=-1.0)`

Hardwire innate circuits into the brain, pre-configuring infant-like biases for face detection, voice preference, reflexes, and social reward.

**Parameters:**
- `stage` (int, optional) -- Developmental stage index. Default: `0`.
- `face` (bool, optional) -- Enable face detection bias. Default: `True`.
- `voice` (bool, optional) -- Enable voice preference bias. Default: `True`.
- `motion` (bool or None, optional) -- Enable motion detection bias. Default: `None` (use stage default).
- `reflexes` (bool, optional) -- Enable reflex circuits. Default: `True`.
- `cry` (bool, optional) -- Enable cry/distress detection. Default: `True`.
- `social_reward` (bool, optional) -- Enable social reward circuits. Default: `True`.
- `habituation` (bool, optional) -- Enable habituation. Default: `True`.
- `novelty` (bool, optional) -- Enable novelty detection. Default: `True`.
- `strength` (float, optional) -- Override bias strength. Default: `-1.0` (use stage defaults).

**Returns:** None

---

### `consolidate(mode='auto', cycles=0, prune_passes=0)`

Trigger memory consolidation, strengthening important memories and pruning weak synapses.

**Parameters:**
- `mode` (str, optional) -- `"auto"`, `"light"`, or `"full"`. Default: `"auto"`.
- `cycles` (int, optional) -- Number of consolidation cycles. Default: `0` (use mode default).
- `prune_passes` (int, optional) -- Number of pruning passes. Default: `0` (use mode default).

**Returns:** bool -- `True` on success.

---

### `prune_synapses(threshold=0.01)`

Prune weak synapses whose absolute weight falls below the given threshold.

**Parameters:**
- `threshold` (float, optional) -- Minimum absolute weight to retain. Default: `0.01`.

**Returns:** int -- Number of synapses pruned.

---

### `cache_communities()`

Pre-compute community structure for community-aware memory consolidation. Detects clusters of strongly interconnected neurons.

**Parameters:** None

**Returns:** dict -- `{"num_communities": int, "num_hubs": int, "modularity": float, "num_neurons": int}`

---

### `invalidate_community_cache()`

Invalidate the cached community structure, forcing re-computation on the next `cache_communities()` call.

**Parameters:** None

**Returns:** None

---

### `set_rubric_validation(features)`

Set validation features for the training rubric for periodic rubric assessments.

**Parameters:**
- `features` (list[float]) -- Validation input feature vector.

**Returns:** bool -- `True` on success.

---

### `set_plasticity_state(state)`

Set the brain's plasticity coordinator state, controlling the global learning regime.

**Parameters:**
- `state` (str) -- One of `"ACQUISITION"`, `"CONSOLIDATION"`, `"MAINTENANCE"`, `"STABILIZING"`.

**Returns:** bool -- `True` on success.

---

## Inference & Prediction

### `predict(features)`

Make a prediction using the brain's full cognitive pipeline (all 28 cognitive stages). Use `predict_fast()` for performance-sensitive code paths.

**Parameters:**
- `features` (list[float]) -- Input feature vector.

**Returns:** tuple(str, float) -- `(label, confidence)`.

---

### `predict_batch(features_list)`

Batch prediction over multiple samples. Internally uses `predict_fast()` per sample. Maximum batch size is 100,000.

**Parameters:**
- `features_list` (list[list[float]]) -- List of feature vectors.

**Returns:** tuple(list[str], list[float]) -- `(labels, confidences)`.

---

### `predict_fast(features)`

Fast prediction -- forward pass only, no cognitive stages. 10-100x faster than `predict()`. Preferred for training loops and batch evaluation.

**Parameters:**
- `features` (list[float]) -- Input feature vector.

**Returns:** tuple(str, float) -- `(label, confidence)`.

---

### `predict_in_domain(features, domain)`

Domain-scoped prediction. Restricts label search to labels matching a domain prefix, preventing first-mover dominance in multi-domain training.

**Parameters:**
- `features` (list[float]) -- Input feature vector.
- `domain` (str) -- Domain prefix (e.g., `"biology:"`, `"physics:"`).

**Returns:** tuple(str, float) -- `(label, confidence)`.

---

### `decide_full(features)`

Run the full cognitive decision pipeline and return detailed results including the output vector, explanation, active neuron count, sparsity, and timing.

**Parameters:**
- `features` (list[float]) -- Input feature vector.

**Returns:** dict -- Keys: `label` (str), `confidence` (float), `explanation` (str), `output_vector` (list[float]), `num_active_neurons` (int), `sparsity` (float), `inference_time_us` (int).

---

### `get_transcript()`

Get the cognitive transcript from the last `decide_full()` call. Returns the chain of cognitive module activations.

**Parameters:** None

**Returns:** list[dict] -- Each dict has: `module` (str), `summary` (str), `salience` (float), `confidence` (float).

---

### `rubric()`

Evaluate the quality of the last brain decision using a two-tier rubric system with letter grading (A+ through F).

**Parameters:** None

**Returns:** dict -- 15 quality scores (`internal_consistency`, `confidence_calibration`, `completeness`, `reasoning_chain_quality`, `epistemic_quality`, `ethical_alignment`, `tier1_score`, `originality`, `integration_depth`, `communication_clarity`, `engagement_quality`, `empathetic_accuracy`, `information_density`, `tier2_score`, `overall_score`), plus `grade` (str), `subsystems_available` (int), `evaluation_time_us` (int).

---

### `training_rubric()`

Get aggregated rubric statistics from training.

**Parameters:** None

**Returns:** dict or None -- `{"eval_count": int, "min_score": float, "max_score": float, "avg_score": float, "last_score": float, "last_grade": str}`. `None` if no evaluations.

---

### `get_accuracy()`

Get running label-match accuracy as an exponential moving average (EMA).

**Parameters:** None

**Returns:** float -- EMA accuracy [0.0, 1.0].

---

### `get_last_gradient_norm()`

Get the L2 norm of the gradient from the most recent `learn()` call.

**Parameters:** None

**Returns:** float -- L2 gradient norm.

---

### `get_uncertainty(features=None)`

Get uncertainty estimates decomposed into epistemic (model) and aleatoric (data) components.

**Parameters:**
- `features` (list[float], optional) -- Input features to assess. If omitted, returns general brain uncertainty.

**Returns:** dict -- `{"epistemic": float, "aleatoric": float, "total": float, "confidence": float, "ensemble_size": int}`

---

### `reset_inference_state()`

Reset neuron activations and LNN hidden states for clean inference between unrelated tasks.

**Parameters:** None

**Returns:** int -- 0 on success.

---

### `probe()`

Get comprehensive brain metrics as a dictionary.

**Parameters:** None

**Returns:** dict -- Extensive metrics including: `num_neurons`, `num_synapses`, `total_inferences`, `total_learning_steps`, `avg_sparsity`, `current_learning_rate`, `accuracy`, `last_loss`, `last_gradient_norm`, `weight_l2_norm`, `ema_loss`, `gpu_available`, `memory_rss_bytes`, `gpu_vram_bytes`, `immune_inflammation`, and many more.

---

### `broadcast_probe()`

Probe brain metrics and broadcast via bio-async for monitoring.

**Parameters:** None

**Returns:** bool -- `True` if successfully broadcast.

---

### `generate(prompt=None, semantic_input=None)`

Generate text using the LNN decoder with a text prompt, semantic vector, or both.

**Parameters:**
- `prompt` (str, optional) -- Text prompt to seed generation.
- `semantic_input` (list[float], optional) -- Semantic vector to condition generation.

**Returns:** dict -- `{"text": str, "confidence": float, "perplexity": float, "success": bool}`

---

### `generate_text(semantic_vector)`

Generate text from a semantic vector using `nimcp_brain_speak()`.

**Parameters:**
- `semantic_vector` (list[float]) -- Semantic embedding vector.

**Returns:** dict -- `{"text": str, "confidence": float, "fluency": float}`

---

### `comprehend(text)`

Comprehend text into a 128-dimensional semantic vector. Inverse of `produce_text()`.

**Parameters:**
- `text` (str) -- Input text.

**Returns:** dict -- `{"semantic_vector": list[float], "confidence": float, "success": bool}`

---

### `produce_text(intent)`

Produce text from a semantic intent vector. Inverse of `comprehend()`.

**Parameters:**
- `intent` (list[float]) -- Semantic intent vector.

**Returns:** dict -- `{"text": str, "confidence": float, "success": bool}`

---

### `tokenize(text)`

Tokenize text to token IDs using the brain's persistent C-level tokenizer. Vocabulary grows across calls.

**Parameters:**
- `text` (str) -- Input text.

**Returns:** list[int] -- Token IDs (up to 512 tokens).

---

### `deliberate(topic)`

Run cognitive deliberation: (1) reasoning engine chain-of-thought, (2) inner dialogue multi-perspective debate.

**Parameters:**
- `topic` (str) -- Topic to deliberate on.

**Returns:** dict -- `{"reasoning_confidence": float, "dialogue_agreement": float, "has_conclusion": bool, "total_turns": int}`

---

### `creative_blend(vector_a, vector_b, blend_ratio=0.5)`

Blend two concept vectors creatively to generate novel text.

**Parameters:**
- `vector_a` (list[float]) -- First concept vector.
- `vector_b` (list[float]) -- Second concept vector.
- `blend_ratio` (float, optional) -- Interpolation ratio (0.0=A, 1.0=B). Default: `0.5`.

**Returns:** dict -- `{"text": str, "success": bool}`

---

### `grounded_respond(text)`

Respond to text using the grounded language system. Comprehension + response in one call.

**Parameters:**
- `text` (str) -- Input text.

**Returns:** dict -- `{"response": str, "confidence": float, "success": bool}`

---

### `self_assess(domain)`

Self-model assessment for a specific domain. Returns probe-based metrics as a proxy for perceived capability.

**Parameters:**
- `domain` (str) -- Domain name (e.g., `"biology"`, `"physics"`).

**Returns:** dict -- `{"domain": str, "accuracy": float, "learning_steps": int, "sparsity": float, "learning_rate": float, "num_neurons": int, "assessment": str}`

---

### `curiosity_detect_gaps(topic)`

Detect knowledge gaps for a topic using the curiosity engine.

**Parameters:**
- `topic` (str) -- Topic to analyze.

**Returns:** dict -- `{"topic": str, "gap_size": float, "curiosity_intensity": float, "learning_potential": float, "related_concepts": int, "questions": list[str]}`

---

### `utm_forward_only(features, output_dim)`

Forward-only inference through the Unified Training Manager.

**Parameters:**
- `features` (list[float]) -- Input feature vector.
- `output_dim` (int) -- Desired output dimension.

**Returns:** list[float] -- Output activation vector.

---

## Sensory Processing & Perception

### `submit_sensory(modality, data, width=0, height=0, channels=0, n_segments=0)`

Stage sensory data into the brain's internal buffers for the next training or inference step. Data is buffered per-modality and replaces any previously staged data for that modality.

**Parameters:**
- `modality` (str) -- One of `"visual"`, `"audio"`, `"speech"`, `"somatosensory"` (or `"somato"`).
- `data` (list[float]) -- Raw sensory data. Visual values in [0.0, 1.0] are scaled to [0, 255].
- `width` (int, optional) -- Image width (visual only). Default: 0 (falls back to 32).
- `height` (int, optional) -- Image height (visual only). Default: 0 (falls back to 32).
- `channels` (int, optional) -- Image channels (visual only). Default: 0 (falls back to 3).
- `n_segments` (int, optional) -- Body segments (somatosensory only). Default: 0 (uses data length).

**Returns:** None

---

### `visual_cortex_process(pixels, width, height, channels)`

Process a raw image through the brain's visual cortex, extracting V1-style Gabor filter features.

**Parameters:**
- `pixels` (list[float]) -- Flat pixel values. Values in [0.0, 1.0] are scaled to [0, 255].
- `width` (int) -- Image width.
- `height` (int) -- Image height.
- `channels` (int) -- Color channels (1=grayscale, 3=RGB).

**Returns:** list[float] -- Feature vector (typically 128 dimensions). Empty list if not initialized.

---

### `audio_cortex_process(samples)`

Process raw audio through the brain's audio cortex, extracting MFCC and mel-spectrogram features.

**Parameters:**
- `samples` (list[float]) -- Raw audio sample amplitudes.

**Returns:** list[float] -- Feature vector (typically 128 dimensions). Empty list if not initialized.

---

### `speech_cortex_process(samples)`

Process raw audio through the brain's speech cortex, extracting phoneme and prosody features.

**Parameters:**
- `samples` (list[float]) -- Raw audio sample amplitudes.

**Returns:** list[float] -- Feature vector (typically 128 dimensions). Empty list if not initialized.

---

## Network Statistics & Metrics

### `get_network_metrics()`

Get per-network training metrics for all network types (ANN, CNN, SNN, LNN), plus HNN and FNO when active.

**Parameters:** None

**Returns:** dict -- Core keys: `ann_loss`, `cnn_loss`, `snn_loss`, `lnn_loss` (float), `ann_steps`, `cnn_steps`, `snn_steps`, `lnn_steps` (int). HNN keys (when active): `hnn_energy`, `hnn_energy_deviation`, `hnn_initial_energy` (float), `hnn_active` (bool). FNO Audio keys (when active): `fno_audio_loss`, `fno_audio_ema_loss` (float), `fno_audio_steps`, `fno_audio_params` (int). FNO Population keys (when active): `fno_pop_train_mse`, `fno_pop_val_mse` (float), `fno_pop_ready` (bool), `fno_pop_train_steps`, `fno_pop_inference_steps` (int).

---

### `get_cortex_cnn_metrics()`

Get per-cortex CNN processor metrics for each sensory modality.

**Parameters:** None

**Returns:** dict -- Keyed by `"visual"`, `"audio"`, `"speech"`, `"somato"`. Each value is a dict with: `last_loss`, `ema_loss`, `embedding_norm`, `confidence` (float), `forward_steps`, `backward_steps` (int), `embedding_dim`, `num_params` (int). Only initialized cortices appear.

---

### `get_cognitive_stats()`

Get per-module cognitive training statistics.

**Parameters:** None

**Returns:** dict -- Keyed by module name (e.g., `"grounded_language"`, `"knowledge"`, `"vae"`, etc.). Each value: `{"steps": int, "last_loss": float}`.

---

### `get_plasticity_stats()`

Get biological plasticity statistics: RPE, neuromodulators, mechanism states, event counts.

**Parameters:** None

**Returns:** dict -- TPB keys: `dopamine`, `acetylcholine`, `serotonin`, `norepinephrine`, `rpe`, `rpe_ema`, `baseline_loss` (float), `tpb_rpe_computations`, `tpb_plasticity_updates`, `tpb_stdp_updates`, `tpb_bcm_updates` (int). EDP keys: `edp_active` (bool), `edp_plasticity_updates`, `edp_ltp_events`, `edp_ltd_events` (int), `edp_avg_prediction_error`, `edp_avg_reward` (float). Coordinator keys: `plasticity_state` (str), `energy_rate` (float), `low_energy` (bool).

---

### `get_utilization_metrics()`

Get brain utilization and saturation metrics.

**Parameters:** None

**Returns:** tuple(float, float) -- `(utilization, saturation)` both in [0.0, 1.0].

---

### `lnn_create(n_sensory=128, n_inter=64, n_command=32, n_output=64)`

Create an NCP-architecture Liquid Neural Network temporal processor. Idempotent.

**Parameters:**
- `n_sensory` (int, optional) -- Sensory input neurons. Default: 128.
- `n_inter` (int, optional) -- Interneurons. Default: 64.
- `n_command` (int, optional) -- Command neurons. Default: 32.
- `n_output` (int, optional) -- Output neurons. Default: 64.

**Returns:** bool -- `True` on success.

---

### `lnn_forward_step(features)`

Run one ODE integration timestep through the LNN (dt=0.01s).

**Parameters:**
- `features` (list[float]) -- Input features matching sensory neuron count.

**Returns:** list[float] -- Output vector from motor neurons.

---

### `lnn_get_state()`

Get the LNN's full internal hidden state vector.

**Parameters:** None

**Returns:** list[float] or None -- Internal state vector, or `None` if LNN not created.

---

### `lnn_get_stats()`

Get LNN training and health statistics.

**Parameters:** None

**Returns:** dict or None -- `{"forward_steps": int, "backward_steps": int, "total_ode_evals": int, "avg_tau": float, "state_norm": float, "gradient_norm": float, "nan_count": int, "inf_count": int}`.

---

### `snn_get_stats()`

Get SNN firing rates, spike counts, population health, and memory usage.

**Parameters:** None

**Returns:** dict or None -- `{"total_steps": int, "total_spikes": int, "mean_firing_rate": float, "max_firing_rate": float, "sparsity": float, "synchrony": float, "spikes_per_sample": float, "silent_neurons": int, "hyperactive_neurons": int, "health": int, "memory_usage_bytes": int}`.

---

### `cnn_get_stats()`

Get CNN architecture and label statistics.

**Parameters:** None

**Returns:** dict or None -- `{"num_layers": int, "num_parameters": int, "num_labels": int, "active": bool}`.

---

## Avatar & Identity

### `get_avatar_state()`

Get the brain's full avatar visual state for driving real-time face animation and lip sync. Returns FACS action units, visemes, gaze, head pose, emotion, and voice parameters.

**Parameters:** None

**Returns:** dict -- Mouth/Lip Sync: `mouth_open`, `lip_round`, `lip_upper`, `lip_lower`, `tongue_position` (float), `current_viseme` (int). FACS AUs: `au1_inner_brow_raise` through `au28_lip_suck` (16 float values). Emotion: `valence` [-1,1], `arousal` [0,1], `dominance` [0,1] (float), `emotion_id` (int), `emotion_intensity` (float). Gaze/Head: `gaze_x`, `gaze_y`, `head_pitch`, `head_yaw`, `head_roll`, `blink` (float). Voice: `pitch_hz`, `speaking_rate`, `volume` (float). Meta: `timestamp_us` (int), `is_speaking` (bool).

---

### `speak(semantic_vector=None)`

Generate spoken text from the brain's neural state, optionally guided by a semantic vector.

**Parameters:**
- `semantic_vector` (list[float], optional) -- Semantic vector to guide speech. Default: `None` (use current brain state).

**Returns:** dict -- `{"text": str, "confidence": float, "fluency": float}`

---

## Brain Regions & Neuromodulation

### `medulla_get_arousal()`

Get the current arousal level from the medulla.

**Parameters:** None

**Returns:** float -- Arousal level [0.0, 1.0].

---

### `medulla_boost_arousal(delta)`

Increase medulla arousal level.

**Parameters:**
- `delta` (float) -- Amount to increase arousal.

**Returns:** None

---

### `medulla_reduce_arousal(delta)`

Decrease medulla arousal level.

**Parameters:**
- `delta` (float) -- Amount to decrease arousal.

**Returns:** None

---

### `medulla_get_circadian_phase()`

Get the current circadian phase from the medulla.

**Parameters:** None

**Returns:** int -- Circadian phase enum value.

---

### `medulla_get_circadian_efficiency()`

Get circadian efficiency factor, useful for scaling learning rate by time-of-day efficiency.

**Parameters:** None

**Returns:** float -- Circadian efficiency [0.0, 1.0].

---

### `update_medulla(delta_time_s)`

Advance the medulla subsystem by a time step, updating circadian clock and arousal state.

**Parameters:**
- `delta_time_s` (float) -- Elapsed time in seconds.

**Returns:** None

---

### `bg_get_dopamine()`

Get current dopamine level from the basal ganglia.

**Parameters:** None

**Returns:** float -- Dopamine level.

---

### `bg_get_rpe()`

Get reward prediction error (RPE) from the basal ganglia. Positive RPE strengthens associations, negative weakens them.

**Parameters:** None

**Returns:** float -- Current RPE.

---

### `bg_get_conflict()`

Get conflict level from the basal ganglia, indicating competing action selections.

**Parameters:** None

**Returns:** float -- Conflict level.

---

### `bg_get_mode()`

Get the current operating mode of the basal ganglia (exploration, exploitation, or habit).

**Parameters:** None

**Returns:** int -- Operating mode enum.

---

### `bg_update_reward(reward, expected)`

Update basal ganglia with actual and expected reward, computing RPE.

**Parameters:**
- `reward` (float) -- Actual reward received.
- `expected` (float) -- Expected reward.

**Returns:** bool -- `True` if update succeeded.

---

### `bg_register_habit(domain, action_id)`

Register a new habit in the basal ganglia for a cognitive domain.

**Parameters:**
- `domain` (str) -- Cognitive domain name.
- `action_id` (int) -- Action identifier.

**Returns:** int -- Habit ID, or negative on failure.

---

### `bg_strengthen_habit(habit_id, success)`

Strengthen an existing habit after successful use.

**Parameters:**
- `habit_id` (int) -- Habit identifier from `bg_register_habit()`.
- `success` (bool) -- Whether execution was successful.

**Returns:** int -- 0 on success, negative on failure.

---

### `bg_check_habit(domain)`

Check whether a habit exists for a cognitive domain.

**Parameters:**
- `domain` (str) -- Cognitive domain name.

**Returns:** int -- Habit ID if exists, -1 if not.

---

### `sleep_get_pressure()`

Get current sleep pressure (adenosine accumulation).

**Parameters:** None

**Returns:** float -- Sleep pressure [0.0, 1.0].

---

### `sleep_get_state()`

Get current sleep state.

**Parameters:** None

**Returns:** int -- 0=awake, 1=drowsy, 2=light, 3=deep (NREM), 4=REM.

---

### `sleep_get_statistics()`

Get comprehensive sleep history and consolidation quality statistics.

**Parameters:** None

**Returns:** dict or None -- `{"total_awake_time_ms": int, "total_sleep_time_ms": int, "sleep_cycles_completed": int, "total_memories_replayed": int, "total_synapses_pruned": int, "avg_consolidation_efficiency": float, "energy_savings_percent": float, "current_sleep_pressure": float}`.

---

### `sleep_is_needed()`

Check whether accumulated sleep pressure warrants a sleep cycle.

**Parameters:** None

**Returns:** bool -- `True` if sleep is needed.

---

### `sleep_run_cycle(num_cycles=1)`

Run one or more full sleep cycles (drowsy, light, deep, REM, awake) with memory consolidation.

**Parameters:**
- `num_cycles` (int, optional) -- Number of cycles. Default: 1.

**Returns:** bool -- `True` on success.

---

### `thalamus_get_mode(nucleus)`

Get the firing mode for a thalamic nucleus.

**Parameters:**
- `nucleus` (str) -- Nucleus name (case-insensitive): `"LGN"`, `"MGN"`, `"VPL"`, `"VPM"`, `"VA"`, `"VL"`, `"PULVINAR"`, `"MD"`, `"ANTERIOR"`, `"TRN"`, or aliases `"visual"`, `"auditory"`, `"somatosensory"`, `"motor"`, `"attention"`, `"prefrontal"`, `"limbic"`.

**Returns:** str -- `"TONIC"`, `"BURST"`, or `"INHIBITED"`.

---

### `thalamus_set_attention(nucleus, attention)`

Set attention level for a specific thalamic nucleus. In practice, attention gating is managed automatically by thalamic bridges during `decide_full()`.

**Parameters:**
- `nucleus` (str) -- Nucleus name (same as `thalamus_get_mode()`).
- `attention` (float) -- Desired attention level.

**Returns:** bool -- `True` (acknowledged).

---

### `substrate_get_health()`

Get overall neural substrate health status.

**Parameters:** None

**Returns:** str -- `"OPTIMAL"`, `"STRESSED"`, `"COMPROMISED"`, `"CRITICAL"`, or `"UNKNOWN"`.

---

### `substrate_get_metabolic()`

Get current metabolic state.

**Parameters:** None

**Returns:** dict -- `{"atp": float, "oxygen": float, "glucose": float, "metabolic_rate": float, "capacity": float}`.

---

### `cerebellum_predict_outcome(state)`

Run cerebellar forward model prediction for error-based motor learning.

**Parameters:**
- `state` (list[float]) -- Input state/motor command vector.

**Returns:** tuple(list[float], float) or None -- `(predicted_outcome, confidence)`, or `None` if cerebellum not enabled.

---

### `cerebellum_process_error(error)`

Send climbing fiber error signal to cerebellum, triggering LTD at Purkinje cells.

**Parameters:**
- `error` (float) -- Error magnitude.

**Returns:** bool -- `True` if broadcast succeeded.

---

### `edp_process_novelty(novelty)`

Process novelty signal through event-driven plasticity.

**Parameters:**
- `novelty` (float) -- Novelty magnitude.

**Returns:** bool -- `True` on success.

---

### `edp_process_reward(reward)`

Consolidate eligibility traces with a reward signal.

**Parameters:**
- `reward` (float) -- Reward value. Positive strengthens, negative weakens eligible synapses.

**Returns:** bool -- `True` on success.

---

### `get_hemispheric_balance()`

Get overall hemispheric balance.

**Parameters:** None

**Returns:** float -- [-1.0, +1.0] where -1=left dominant, +1=right dominant.

---

### `get_callosum_transfers()`

Get total inter-hemispheric corpus callosum transfers.

**Parameters:** None

**Returns:** int -- Transfer count.

---

### `get_lateralization(domain)`

Get lateralization dominance for a cognitive domain.

**Parameters:**
- `domain` (int) -- Cognitive domain index (0-11).

**Returns:** float -- 0.0=right dominant, 1.0=left dominant.

---

### `shift_lateralization(domain, shift)`

Shift lateralization of a cognitive domain. Positive = toward left, negative = toward right.

**Parameters:**
- `domain` (int) -- Cognitive domain index (0-11).
- `shift` (float) -- Shift amount.

**Returns:** bool -- `True` on success.

---

### `world_model_dream(horizon=5)`

Run dreaming via RSSM for offline consolidation.

**Parameters:**
- `horizon` (int, optional) -- Forward prediction steps per dream. Default: 5.

**Returns:** bool -- `True` on success.

---

### `jepa_predict(context)`

Forward prediction in latent space via RSSM world model.

**Parameters:**
- `context` (list[float]) -- Context action vector.

**Returns:** tuple(list[float], float) -- `(predicted_state, confidence)`.

---

## Training Integration & UTM

### `ti_add_fact(fact, salience)`

Add a fact to the reasoning knowledge base.

**Parameters:**
- `fact` (str) -- Fact string.
- `salience` (float) -- Importance weight.

**Returns:** bool -- `True` on success.

---

### `ti_add_rule(rule, priority)`

Add a rule to the reasoning engine for forward/backward chaining.

**Parameters:**
- `rule` (str) -- Rule string.
- `priority` (float) -- Priority weight.

**Returns:** bool -- `True` on success.

---

### `ti_forward_chain(max_iterations)`

Run forward chaining on the knowledge base, deriving new facts from rules.

**Parameters:**
- `max_iterations` (int) -- Maximum chaining iterations.

**Returns:** int -- Number of new facts derived.

---

### `ti_backward_chain(goal)`

Run backward chaining to prove a goal.

**Parameters:**
- `goal` (str) -- Goal to prove.

**Returns:** float -- Confidence score [0.0, 1.0].

---

### `ti_reason(query)`

Run a reasoning query.

**Parameters:**
- `query` (str) -- Reasoning query.

**Returns:** float -- Confidence score.

---

### `ti_query_knowledge(query)`

Query the knowledge base for matching facts.

**Parameters:**
- `query` (str) -- Query string.

**Returns:** int -- Number of matching facts.

---

### `ti_init_reasoning()`

Initialize the reasoning subsystem. Must be called before `ti_add_fact`, `ti_add_rule`, `ti_forward_chain`, `ti_backward_chain`, or `ti_reason`.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

### `ti_compute_adaptive_lr(base_lr)`

Compute adaptive learning rate based on brain state (dopamine, arousal).

**Parameters:**
- `base_lr` (float) -- Base learning rate.

**Returns:** float -- Adjusted learning rate.

---

### `ti_compute_unified_lr(base_lr)`

Compute unified adaptive LR with all brain modulations (arousal, circadian, inflammation, instability, Portia, stress).

**Parameters:**
- `base_lr` (float) -- Base learning rate.

**Returns:** float -- Fully modulated learning rate.

---

### `ti_compute_decision_cycle(loss_cur, loss_prev, grad_norm, grad_norm_prev, loss_vol, grad_var, lr, batch)`

Run full training decision cycle combining convergent evidence, causal reasoning, and abductive diagnosis.

**Parameters:**
- `loss_cur` (float) -- Current training loss.
- `loss_prev` (float) -- Previous training loss.
- `grad_norm` (float) -- Current gradient norm.
- `grad_norm_prev` (float) -- Previous gradient norm.
- `loss_vol` (float) -- Loss volatility.
- `grad_var` (float) -- Gradient variance.
- `lr` (float) -- Current learning rate.
- `batch` (float) -- Current batch size.

**Returns:** dict -- Keys: `consensus_action` (int), `lr_factor`, `batch_factor`, `grad_clip_factor`, `urgency` (float), `converged` (bool), `num_contributors` (int), `primary_diagnosis` (str), `diagnosis_plausibility` (float), `recommend_pause`, `recommend_rollback` (bool), `causal_explanation` (str), `causal_confidence` (float), `lr_change_beneficial` (bool).

---

### `ti_compute_modulation_state()`

Get full modulation state from all brain subsystems.

**Parameters:** None

**Returns:** dict -- Keys: `arousal_level`, `arousal_cognitive_gain`, `arousal_memory_consolidation`, `circadian_efficiency`, `rpe_bonus`, `inflammation_learning_factor`, `inflammation_precision`, `instability_lr_scale`, `instability_batch_scale`, `instability_clip_factor`, `portia_learning_gate`, `portia_compute_budget`, `stress_level`, `cognitive_capacity`, `conflict_level`, `final_lr_factor`, `final_batch_factor`, `final_clip_factor` (float), `should_pause` (bool).

---

### `ti_post_batch_update(accuracy, expected, domain)`

Post-batch update feeding accuracy and domain info back to training integration.

**Parameters:**
- `accuracy` (float) -- Achieved accuracy.
- `expected` (float) -- Expected accuracy.
- `domain` (str) -- Training domain name.

**Returns:** bool -- `True` on success.

---

### `ti_get_cognitive_capacity()`

Get cognitive capacity from hypothalamus [0.0, 1.0].

**Parameters:** None

**Returns:** float

---

### `ti_get_stress_level()`

Get stress level from hypothalamus [0.0, 1.0].

**Parameters:** None

**Returns:** float

---

### `ti_get_urgency_mode()`

Get urgency mode from hypothalamus (0=relaxed, 3=fight-or-flight).

**Parameters:** None

**Returns:** int

---

### `ti_get_reasoning_degradation()`

Get Portia degradation level affecting reasoning (0-4).

**Parameters:** None

**Returns:** int

---

### `ti_get_reasoning_phases_disabled()`

Get number of reasoning phases currently disabled by Portia.

**Parameters:** None

**Returns:** int

---

### `ti_should_skip_reasoning()`

Check if Portia recommends skipping reasoning due to resource pressure.

**Parameters:** None

**Returns:** bool

---

### `ti_mesh_is_available()`

Check if mesh reasoning network is available.

**Parameters:** None

**Returns:** bool

---

### `ti_mesh_get_coherence()`

Get mesh reasoning channel coherence [0.0, 1.0].

**Parameters:** None

**Returns:** float

---

### `ti_mesh_get_participant_count()`

Get mesh reasoning participant count.

**Parameters:** None

**Returns:** int

---

### `utm_get_training_health()`

Get DFA-based training health diagnostic.

**Parameters:** None

**Returns:** dict -- `{"health": int, "health_name": str, "dfa_exponent": float, "gradients_healthy": int, "early_stopped": int}`

---

### `utm_set_fractal_lr(net_idx, scale)`

Set fractal LR scaling for a network.

**Parameters:**
- `net_idx` (int) -- Network index (0-based).
- `scale` (float) -- Fractal scaling factor.

**Returns:** None

---

### `utm_set_natural_gradient(net_idx, enabled)`

Enable/disable natural gradient for a network.

**Parameters:**
- `net_idx` (int) -- Network index.
- `enabled` (bool) -- Enable or disable.

**Returns:** None

---

### `utm_set_per_network_lr(net_idx, lr)`

Set per-network learning rate override.

**Parameters:**
- `net_idx` (int) -- Network index.
- `lr` (float) -- Learning rate.

**Returns:** None

---

### `utm_swap_to_ema()`

Swap to EMA parameters for smoother inference.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

### `utm_swap_from_ema()`

Swap back to live parameters after EMA inference.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

## Cloud & Collective

### `connect_cloud(cloud_brain, confidence_threshold=0.5, enable_distillation=True)`

Connect a cloud backend brain for hybrid edge-cloud inference. When local confidence falls below threshold, queries escalate to cloud.

**Parameters:**
- `cloud_brain` (Brain) -- Brain instance serving as cloud backend.
- `confidence_threshold` (float, optional) -- Escalation threshold. Default: 0.5.
- `enable_distillation` (bool, optional) -- Buffer cloud responses for distillation. Default: True.

**Returns:** bool -- `True` on success.

---

### `disconnect_cloud()`

Disconnect cloud backend, returning to standalone inference.

**Parameters:** None

**Returns:** bool -- `True` on success.

---

### `distill_cloud_batch(max_examples=0)`

Process buffered distillation examples from cloud inference.

**Parameters:**
- `max_examples` (int, optional) -- Max examples to process. 0 = all. Default: 0.

**Returns:** int -- Number of examples processed.

---

### `get_cloud_stats()`

Get cloud inference usage statistics.

**Parameters:** None

**Returns:** dict -- `{"total_queries": int, "local_handled": int, "cloud_escalated": int, "distillation_steps": int, "local_handled_pct": float}`

---

### `connect_collective(other_brain, instance_id)`

Connect to another brain's collective cognition system for distributed reasoning.

**Parameters:**
- `other_brain` (Brain) -- Brain to connect with.
- `instance_id` (int) -- Unique identifier for this brain in the collective.

**Returns:** None

---

## Security

### `lgss_check_content(text)`

Run the LGSS (Layered Generative Safety System) content filter on text.

**Parameters:**
- `text` (str) -- Text to check for safety.

**Returns:** dict -- `{"is_safe": bool, "status": int, "confidence": float, "explanation": str, "reason": str}`. If matched, also includes `"matched_pattern"` (str). If no LGSS filter available, returns `{"is_safe": true, "reason": "no lgss filter available"}`.

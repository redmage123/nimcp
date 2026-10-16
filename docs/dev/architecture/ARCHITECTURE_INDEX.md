# NIMCP Core Architecture - Documentation Index

## Quick Navigation

### For Different Audiences

**I want a quick overview** → Read: `QUICK_REFERENCE.md`
- Concise component descriptions
- File locations & line counts
- Key data structures
- Common usage patterns
- Performance characteristics

**I want comprehensive details** → Read: `ARCHITECTURE_SUMMARY.md`
- Detailed module descriptions (1000+ lines)
- Full API documentation
- Design patterns explained
- Interdependency analysis
- Biological motivation & references

**I want a visual diagram** → Read: `ARCHITECTURE_DIAGRAM.txt`
- ASCII layer diagram
- Component relationships
- Interdependency graph
- Feature checklist
- Design principles

---

## Core Modules (24,346 LOC)

### Tier 1: Public API
- **File**: `src/include/nimcp.h` (739 lines)
- **Role**: Single entry point for all bindings
- **Purpose**: Version-stable C API with opaque handles
- **Key Types**: `nimcp_brain_t`, `nimcp_network_t`, `nimcp_ethics_t`, `nimcp_knowledge_t`

### Tier 2: Brain (Facade)
- **Files**: `src/core/brain/*.{h,c}` (7,770 lines)
- **Role**: High-level learning interface
- **Components**:
  - `nimcp_brain.h/c` - Main brain API (1,382 + 6,388 lines)
  - `nimcp_distributed_cow.h/c` - Distributed cloning (447 + 818 lines)
  - `nimcp_pretrained.c` - Pre-trained models (525 lines)
  - `processing/` - Cognitive modules (281 lines)
- **Key Types**: `brain_config_t`, `brain_decision_t`, `brain_stats_t`
- **Supports**: 4 size presets × 5 tasks + 50+ advanced features

### Tier 3: Neural Network Core
- **Files**: `src/core/neuralnet/*.{h,c}` (3,196 lines)
- **Role**: Low-level computation engine
- **Components**:
  - `nimcp_neuralnet.h/c` - Core network (524 + 2,672 lines)
- **Key Types**: `neuron_t` (100-100K neurons), `synapse_t` (256 max/neuron)
- **Features**: STDP, Hebbian, Oja, homeostasis, plasticity

### Tier 4: Synapse & Neuron Specialization

#### 4A: Synapse Computation (NIMCP 2.7)
- **Files**: `src/core/synapse_compute/*.{h,c}` (1,091 lines)
- **Role**: Programmable synapses (synapses as processors, not just weights)
- **Key Functions**:
  - 6 built-in compute: default, attention, semantic, gating, neuromodulated, dendritic
  - 3 built-in learn: three-factor, attention-modulated, metaplastic
- **Performance**: 10-500 cycles/synapse depending on type

#### 4B: Synapse Types (Phase 8.7)
- **Files**: `src/core/synapse_types/*.{h,c}` (1,362 lines)
- **Role**: Biologically realistic receptor diversity
- **Types**: 9 synapses (AMPA, NMDA, GABA-A, GABA-B, dopamine, serotonin, acetylcholine, electrical, generic)
- **Kinetics**: Type-specific time constants & dynamics

#### 4C: Neuron Types (Phase 8.7)
- **Files**: `src/core/neuron_types/*.{h,c}` (3,091 lines total)
- **Role**: Specialized neuron types for different functions
- **Types**: 20+ including V1 simple/complex, place cells, grid cells, PFC, amygdala, etc
- **Logic Gates** (Phase 9.4): AND, OR, XOR, threshold operations

#### 4D: Neuron Models
- **Files**: `src/core/neuron_models/*.{h,c}` (1,201 lines)
- **Role**: Pluggable neuron dynamics
- **Models**: LIF (simple), Izhikevich (realistic), custom (user-defined)

### Tier 5: Topology Generation
- **Files**: `src/core/topology/*.{h,c}` (1,463 lines)
- **Role**: Scale-free & fractal network construction
- **Algorithms**:
  - Barabási-Albert (preferential attachment)
  - Fractal hierarchical
- **Benefit**: 70-80% fewer synapses vs random connectivity
- **Analysis**: Hub detection, betweenness, power-law fitting

### Tier 6: Advanced Subsystems

#### 6A: Brain Oscillations
- **Files**: `src/core/brain_oscillations/*.{h,c}` (936 lines)
- **Purpose**: Neural oscillation analysis & modulation
- **Types**: Delta, theta, alpha, beta, gamma (biologically accurate)

#### 6B: Brain Regions
- **Files**: `src/core/brain_regions/*.{h,c}` (1,193 lines)
- **Purpose**: Functional brain area modeling
- **Areas**: 8 regions (visual, temporal, parietal, prefrontal, amygdala, hippocampus, striatum, cerebellum)

#### 6C: Multimodal Integration (Phase 8)
- **Files**: `src/core/integration/*.{h,c}` (646 lines)
- **Purpose**: Unified vision + audio + language processing
- **Pipeline**: 7 stages from sensory extraction to consolidation

---

## Feature Matrix

### By NIMCP Version

| Version | Major Feature | Location | LOC |
|---------|---------------|----------|-----|
| 2.0-2.5 | Basic neural network | neuralnet | 3,196 |
| 2.6 | Short-term plasticity, Izhikevich | neuralnet, neuron_models | +1,201 |
| **2.7** | **Programmable synapses** | synapse_compute | +1,091 |
| **Phase 8.7** | **Synapse & neuron specialization** | synapse_types, neuron_types | +4,453 |
| **Phase 8** | **Multi-modal processing** | integration | +646 |
| **Phase 2.8** | **Distributed COW cloning** | brain/distributed_cow | +1,265 |
| **Phase 5.2** | **Brain oscillations** | brain_oscillations | +936 |
| **Phase 5.3** | **Brain regions** | brain_regions | +1,193 |
| **Phase 9.0** | **Pre-trained models** | brain/pretrained | +525 |
| **Phase 9.4** | **Language & logic** | neuron_types/neural_logic | +938 |
| **Phase 10** | **Advanced cognition** | brain (config) | (integrated) |

### By Capability

| Capability | Module | LOC | Performance |
|------------|--------|-----|-------------|
| Learning | brain, neuralnet | 10,966 | Various |
| Inference | neuralnet | 2,672 | 0.1-50ms |
| Specialization | synapse_types, neuron_types | 4,453 | Type-dependent |
| Plasticity | synapse_compute | 1,091 | 10-500 cycles |
| Topology | topology | 1,463 | O(N log N) |
| Oscillations | brain_oscillations | 936 | Monitoring |
| Integration | integration | 646 | 10-50ms total |
| Distributed | brain/distributed_cow | 1,265 | <10ms clone |

---

## Key Data Structures Reference

### Brain Layer
- `brain_config_t` - 50+ configuration flags
- `brain_decision_t` - Prediction with confidence & explanation
- `brain_stats_t` - Network statistics
- `brain_snapshot_info_t` - Snapshot metadata
- `brain_multimodal_input_t` - Vision + audio + language input
- `brain_multimodal_output_t` - Comprehensive cognitive output

### Network Layer
- `neuron_t` - Neuron with synapses, state, model, type parameters
- `synapse_t` - Synapse with weight, plasticity, compute functions, type
- `network_config_t` - Network configuration
- `network_stats_t` - Network statistics

### Synapse Computation (NIMCP 2.7)
- `synapse_compute_context_t` - Shared context for synapses
- `synapse_compute_state_t` - Per-synapse state (local + extended memory)

### Synapse Types (Phase 8.7)
- `ampa_state_t`, `nmda_state_t`, `gaba_a_state_t`, `gaba_b_state_t` - Kinetics
- `dopamine_state_t`, `serotonin_state_t`, `ach_state_t` - Neuromodulation
- `electrical_state_t` - Gap junction

### Neuron Models
- `neuron_model_state_t` - Model-agnostic state
- `izhikevich_params_t` - Izhikevich parameters (a, b, c, d)

### Topology
- `topology_config_t` - Unified topology configuration
- `scale_free_config_t` - Barabási-Albert parameters
- `fractal_config_t` - Hierarchical parameters
- `topology_stats_t` - Graph metrics

---

## API Quick Reference

### Brain API
```c
nimcp_brain_t nimcp_brain_create(name, size, task, inputs, outputs);
nimcp_brain_t nimcp_brain_create_from_config(filepath);
nimcp_brain_t nimcp_brain_create_pretrained(model_id, task);
void nimcp_brain_destroy(brain);

nimcp_status_t nimcp_brain_learn_example(brain, features, count, label, confidence);
nimcp_status_t nimcp_brain_predict(brain, features, count, out_label, out_confidence);

nimcp_status_t nimcp_brain_snapshot_save(brain, name, description);
nimcp_brain_t nimcp_brain_snapshot_restore(brain, name);

nimcp_brain_t nimcp_brain_clone_cow(original);  // 86% memory savings
nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(brain);  // 99% savings

bool brain_process_multimodal(brain, input, output);  // Phase 8
nimcp_status_t nimcp_brain_probe(brain, probe);
```

### Network API
```c
neural_network_t neural_network_create(config);
void neural_network_destroy(network);

bool neural_network_forward(network, inputs, in_size, outputs, out_size);
uint32_t neural_network_compute_step(network, timestamp);

uint32_t neural_network_add_neuron(network, activation);
bool neural_network_add_connection(network, from, to, weight);
bool neural_network_add_connection_typed(network, from, to, weight, type);  // Phase 8.7

uint32_t neural_network_apply_stdp(network, neuron_id, timestamp);
uint32_t neural_network_apply_oja(network, neuron_id, timestamp);
```

### Topology API
```c
bool topology_generate_scale_free(network, config, stats);
bool topology_generate_fractal(network, config, stats);
bool topology_compute_stats(network, stats);
bool topology_identify_hubs(network, percentile, hubs, count);
```

---

## Performance Targets

### Memory Usage
- TINY: <1 MB (100 neurons)
- SMALL: ~10 MB (1K neurons)
- MEDIUM: ~50 MB (10K neurons)
- LARGE: ~500 MB (100K neurons)

### Inference Speed
- TINY: ~0.1 ms
- SMALL: ~0.5 ms
- MEDIUM: ~5 ms
- LARGE: ~50 ms

### Copy-on-Write
- Clone time: <10 ms
- Memory overhead: ~1 MB
- Savings: 86% for replicas, 99% for snapshots

### Synapse Computation
- Default: ~10 cycles
- Attention: ~100-500 cycles
- Semantic: ~300 cycles
- Function call: ~5 cycles overhead

---

## Biological Realism Features

- ✓ Spike-timing dependent plasticity (STDP)
- ✓ Homeostatic plasticity (stability)
- ✓ 9 different synapse types (AMPA, NMDA, GABA, dopamine, etc)
- ✓ 20+ specialized neuron types
- ✓ Short-term plasticity (depression/facilitation)
- ✓ Neural oscillations (delta, theta, alpha, beta, gamma)
- ✓ Hierarchical organization (brain regions, cortical columns)
- ✓ Dendritic computation (nonlinear integration)
- ✓ Gap junctions (electrical synapses)
- ✓ Voltage-dependent dynamics (Izhikevich, NMDA Mg²⁺ block)

---

## Design Philosophy

### Biological Realism
- STDP learning matches experimental neuroscience
- Diverse synapse/neuron types reflect real brain
- Oscillations for neural coordination
- Hierarchical organization of brain regions

### Computational Efficiency
- Scale-free topology: 70-80% fewer synapses
- Sparse connectivity: O(S) not O(N²)
- Copy-on-Write: 86% memory savings for clones
- Bidirectional synapse tracking: O(S) input summation

### Extensibility
- Programmable synapses (NIMCP 2.7)
- Pluggable neuron models
- Custom compute functions
- Strategy pattern throughout

### Usability
- Simple high-level API (brain.h)
- Low-level control (neuralnet.h)
- Pre-trained models
- Multi-modal processing
- Interpretability (explanations)

---

## Important Files

### Headers (Public Interface)
```
src/include/nimcp.h                      - Public API (start here)
src/core/brain/nimcp_brain.h             - Brain API
src/core/neuralnet/nimcp_neuralnet.h     - Network API
src/core/synapse_compute/...             - Synapse computation
src/core/synapse_types/...               - Receptor types
src/core/neuron_types/...                - Neuron specialization
src/core/neuron_models/...               - Neuron dynamics
src/core/topology/...                    - Network topology
```

### Implementation
```
src/core/brain/nimcp_brain.c             - 6,388 lines (largest)
src/core/neuralnet/nimcp_neuralnet.c     - 2,672 lines
src/core/neuron_types/nimcp_neuron_types.c - 793 lines
...and 11 other .c files
```

### Documentation (This Package)
```
QUICK_REFERENCE.md           - Start here for quick lookup
ARCHITECTURE_SUMMARY.md      - Comprehensive 50+ page details
ARCHITECTURE_DIAGRAM.txt     - Visual layer diagram
ARCHITECTURE_INDEX.md        - This file
```

---

## Learning Path

### For Users
1. Read `QUICK_REFERENCE.md` - Get overview
2. Look at `src/include/nimcp.h` - Public API
3. Study `ARCHITECTURE_DIAGRAM.txt` - System structure
4. Explore `ARCHITECTURE_SUMMARY.md` - Deep dive

### For Contributors
1. Read `ARCHITECTURE_SUMMARY.md` - Full details
2. Study interdependency section
3. Look at module implementations
4. Understand design patterns used
5. Review biological references

### For Researchers
1. `QUICK_REFERENCE.md` - Feature inventory
2. Biological realism section in `ARCHITECTURE_SUMMARY.md`
3. Individual module headers (design motivation)
4. Reference papers cited in code

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total Core LOC | 24,346 |
| Total with Public API | 25,085 |
| Number of Modules | 12 |
| Number of Header Files | 25+ |
| Number of Implementation Files | 20+ |
| Max Neurons | 100,000 |
| Max Synapses/Neuron | 256 |
| Supported Synapse Types | 9 |
| Supported Neuron Types | 20+ |
| Brain Size Presets | 4 |
| Task Templates | 5 |
| Configuration Options | 50+ |
| Design Patterns Used | 9+ |
| Phases Implemented | 10 (2.0-10) |
| Versions | 2.7.0 |

---

## Version History

- **2.0-2.5**: Core neural network, STDP, plasticity
- **2.6**: Short-term plasticity, Izhikevich model
- **2.7**: Programmable synapses (MAJOR)
- **2.8.7 / Phase 8.7**: Synapse & neuron specialization (MAJOR)
- **Phase 8**: Multi-modal integration
- **Phase 9.0**: Pre-trained models
- **Phase 9.4**: Language & logic reasoning
- **Phase 10**: Advanced cognition (working memory, emotions, mirror neurons)
- **Phase 2.8**: Distributed COW cloning
- **Phase 5.2-5.3**: Oscillations, brain regions
- **Current**: 2.7.0 / Phase 8.7 (November 2025)

---

## Resources

- **Main Documentation**: `ARCHITECTURE_SUMMARY.md` (comprehensive)
- **Visual Diagram**: `ARCHITECTURE_DIAGRAM.txt` (ASCII art)
- **Quick Reference**: `QUICK_REFERENCE.md` (lookup)
- **This Index**: `ARCHITECTURE_INDEX.md` (navigation)

---

**Document Version**: 1.0  
**NIMCP Version**: 2.7.0 / Phase 8.7  
**Last Updated**: November 11, 2025  
**Total Lines Analyzed**: 24,346  
**Files Reviewed**: 25+

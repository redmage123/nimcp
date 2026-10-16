# NIMCP Project Overview

**Version**: 2.6.3 | **Updated**: March 2026

## Project Vision & Motivation

### The Problem with Current AI

Current AI trends favor large language models (LLMs) with ever-increasing parameter counts. This approach presents fundamental challenges:

| Issue | Description |
|-------|-------------|
| **Resource Consumption** | Power and water requirements are growing exponentially, making AI environmentally unsustainable |
| **Hallucinations** | LLMs continue to produce confident but incorrect outputs due to their stochastic nature |
| **AGI Uncertainty** | It remains unclear whether transformer-based architectures can achieve true artificial general intelligence |

### Our Solution: Biologically-Inspired Neural Computing

NIMCP (Neuro Inspired Modular Control Protocol) is a working biologically-grounded neural computing system implemented in C with Python bindings. Rather than scaling transformer neural networks, NIMCP faithfully models the architecture and mechanisms of biological brains. The core hypothesis — that evolution has already solved the problems of efficient, robust, and general intelligence — drives every design decision.

The system currently runs a 2M neuron brain with a multi-layer diamond architecture, 80+ integrated subsystems, and full biological learning across 33+ brain regions. It is written in approximately 2,552 source files and 2,456 headers, with 150+ Python API methods.

---

## Architecture at a Glance

### Neural Substrate

| Component | Implementation |
|-----------|---------------|
| **Multi-layer diamond architecture** | Small (<5K): 3 layers. Medium (5K-100K): 5 layers. Large (100K+): 7 layers. Layer sizes follow diamond/pyramid distributions |
| **Neuron model** | Hot/cold struct split for cache optimization. Hot path: membrane potential, spike state, activity EMA. Cold path: Oja params, creation metadata, type params |
| **Sparse synapse storage** | Embedded capacity 320 per neuron (inline, no heap). Overflow pool of 200K entries. Chunked metadata block allocator (64K entries/block) |
| **Synaptic connectivity** | MIN_FAN_IN 128, MAX_FAN_IN 384. Feedback connections (10% of forward), lateral Mexican-hat (5%), small-world shortcuts (1%). 50M connection budget |
| **CPU-staged embeddings** | 2048D pinned CPU pool via `cudaHostAlloc`. GPU batch relevance pipeline computes cosine similarity with shared-memory context vector |

### Three Neural Network Paradigms

1. **SNN (Spiking Neural Networks)** — Biological spike timing, 42 cross-modal bridges, population coding
2. **LNN (Liquid Neural Networks)** — NCP architecture for temporal context, ODE-based dynamics, adjoint gradient method
3. **CNN** — Visual and audio feature extraction pipelines

### Brain Regions (33+)

Prefrontal cortex, occipital lobe, parietal lobe, temporal lobe, hippocampus, cerebellum, basal ganglia, thalamus, amygdala, Broca's area, Wernicke's area, motor cortex, somatosensory cortex, auditory cortex, visual cortex, insular cortex, cingulate cortex, and others — each with region-specific processing logic.

### Cognitive Modules (60+)

Introspection, ethics evaluation, theory of mind, imagination engine, reasoning (deductive, inductive, abductive, analogical), emotional processing, curiosity/novelty, epistemic evaluation, aesthetic evaluation, creative orchestration, inner dialogue, metacognition, autobiographical memory, predictive hierarchy, knowledge graph, explanation generation, and many more.

---

## Biological Systems Modeled

| System | Biological Basis | NIMCP Implementation |
|--------|------------------|---------------------|
| **Plasticity** | LTP/LTD, STDP, BCM, metaplasticity | Full biological learning: STDP, BCM, eligibility traces, dendritic computation, homeostatic synaptic scaling |
| **Neuromodulation** | DA, 5-HT, ACh, NE, GABA, Glu | Spatial neuromodulator diffusion with 6 transmitter types, reward modulation, curiosity gating |
| **Glial Cells** | Astrocytes, oligodendrocytes, microglia | Astrocyte calcium dynamics (Euler), oligodendrocyte skip-frames, glial amortization |
| **Immune System** | B cells, T cells, cytokines, inflammation | Adaptive immunity with B cell state progression (Naive -> Activated -> Plasma), antibody production, BBB |
| **Sleep/Wake** | Circadian rhythms, sleep stages, consolidation | Sleep consolidation/pruning cycles (every 500 steps), memory replay, hippocampal neurogenesis |
| **Hemispheric** | Left/right brain, corpus callosum | Bilateral processing with lateralization, 500 msg/s corpus callosum bandwidth |
| **Developmental** | Critical periods, synaptogenesis, pruning | Immersive curriculum: Newborn, Infant, Crawler, Toddler, Child stages with critical period plasticity (2x in first 1000 steps) |

---

## Key Design Principles

1. **Universal Scalability** — Portia tier system (FULL/MEDIUM/CONSTRAINED/MINIMAL) adapts fidelity to available compute. FAST init mode brings 1.5M neurons online in 14 seconds. Edge-cloud hybrid inference with confidence-gated routing.

2. **Biological Learning** — Continuous real-time learning, not just training-then-inference. Multiple plasticity mechanisms operate simultaneously. Memory consolidation during simulated sleep. Self-improvement through introspection and metacognition.

3. **Swarm Intelligence** — Distributed cognition modeled after biological hive species. Multi-brain collective with consensus mechanisms. Bio-async messaging with 8-thread pool.

4. **Ethical Foundation** — Primary directive: furthering and bettering the human condition. LGSS (Layered Governance Safety System), Asimov-inspired directives with Golden Rule reciprocity, Blood-Brain Barrier for safety isolation.

5. **Self-Awareness** — IIT 3.0 Phi computation for consciousness metrics. Theory of Mind. Autobiographical memory. Introspection and cognitive transcript generation.

---

## GPU Acceleration

| Feature | Details |
|---------|---------|
| **Forward/backward pass** | CUDA kernels for multi-layer diamond architecture, iterates `num_layers` transitions |
| **Plasticity** | GPU-accelerated STDP, BCM, eligibility trace updates |
| **Embedding relevance** | Batch cosine similarity kernel with shared-memory context vector |
| **Stream pool** | 8 `cudaStreamNonBlocking` streams, round-robin dispatch via atomics |
| **Memory** | ~15-16.5 GB VRAM for 1.5M neurons on 20 GB RTX 4000 |

---

## Language & Communication

- **Grounded language system**: Word grounding, comprehension, text production
- **SNN-Language bridge**: Sparse binding hash map, STDP word-concept binding, population coding (8 neurons/pop), Broca/Wernicke cascades
- **Cognitive transcript**: Captures outputs from 28 cognitive module types per decision cycle, computes coherence, cognitive load, dominant module, 10 response hint flags
- **Python talk interface**: `talk_to_athena.py` for interactive conversation with transcript introspection

---

## Training

- **Immersive developmental curriculum**: Biologically-staged learning (Newborn through Child)
- **Hyperledger-inspired EOV training**: Consensus gate and audit ledger
- **Adaptive curriculum LR**: Mastery tracking with per-domain rate adjustment
- **Athena training target**: 95% accuracy across 24 domains (81 datasets)
- **Checkpoint-resume**: Full training state persistence (loss history, LR, counters)

---

## Supported Compute Hardware

| Hardware | Status |
|----------|--------|
| **CPU** | Production. SIMD vectorization, hot/cold struct split, prefetch hints |
| **GPU (CUDA)** | Production. Forward/backward/plasticity kernels, stream pool, batch embeddings |
| **Neuromorphic** | Planned. Native SNN mapping to Intel Loihi, IBM TrueNorth, SpiNNaker |
| **FPGA / TPU / Quantum** | Planned. Architecture supports future backends |

---

## Python API

150+ methods exposed through C Python bindings (`nimcp.cpython-312-x86_64-linux-gnu.so`). Key capabilities: brain creation with init modes (FULL/FAST/MINIMAL), predict, learn_vector, generate_text, tokenize, prune_synapses, get_transcript, checkpoint save/load, multi-brain collective, and domain-specific training interfaces.

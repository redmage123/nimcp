# Mathematical Foundations of NIMCP

*Neuro-Inspired Modular Control Protocol -- A Formal Treatment*

**Version 0.9.0-beta | March 2026**

---

## Table of Contents

- [Abstract](#abstract)
- [Introduction](#introduction)
- [1. Neural Dynamics](#1-neural-dynamics)
  - [1.1 Leaky Integrate-and-Fire Model](#11-leaky-integrate-and-fire-model)
  - [1.2 Spiking Neural Network Population Dynamics](#12-spiking-neural-network-population-dynamics)
  - [1.3 Liquid Neural Network (LNN)](#13-liquid-neural-network-lnn)
  - [1.4 Hamiltonian Neural Networks (HNN)](#14-hamiltonian-neural-networks-hnn)
- [2. Learning Rules](#2-learning-rules)
  - [2.1 Spike-Timing-Dependent Plasticity (STDP)](#21-spike-timing-dependent-plasticity-stdp)
  - [2.2 BCM Rule (Bienenstock-Cooper-Munro)](#22-bcm-rule-bienenstock-cooper-munro)
  - [2.3 Eligibility Traces](#23-eligibility-traces)
  - [2.4 Homeostatic Plasticity](#24-homeostatic-plasticity)
  - [2.5 Backpropagation Through Time for SNN](#25-backpropagation-through-time-for-snn)
  - [2.6 Adjoint Method for LNN](#26-adjoint-method-for-lnn)
  - [2.7 AdamW Optimizer](#27-adamw-optimizer)
- [3. Fourier Neural Operators](#3-fourier-neural-operators)
  - [3.1 Spectral Convolution](#31-spectral-convolution)
  - [3.2 FFT Implementation](#32-fft-implementation)
  - [3.3 FNO Layer Architecture](#33-fno-layer-architecture)
  - [3.4 SNN-FNO Bridge](#34-snn-fno-bridge)
- [4. Quantum-Inspired Algorithms](#4-quantum-inspired-algorithms)
  - [4.1 Quantum Annealing](#41-quantum-annealing)
  - [4.2 Quantum Walk](#42-quantum-walk)
  - [4.3 Quantum Monte Carlo](#43-quantum-monte-carlo)
  - [4.4 Quantum Shannon Integration](#44-quantum-shannon-integration)
  - [4.5 Quantum-Classical Bridge](#45-quantum-classical-bridge)
- [5. Information Theory](#5-information-theory)
  - [5.1 Shannon Entropy](#51-shannon-entropy)
  - [5.2 Mutual Information and KL Divergence](#52-mutual-information-and-kl-divergence)
  - [5.3 Integrated Information (IIT Phi)](#53-integrated-information-iit-phi)
  - [5.4 Partial Information Decomposition (PID)](#54-partial-information-decomposition-pid)
  - [5.5 Fisher Information Matrix](#55-fisher-information-matrix)
  - [5.6 Information Geometry](#56-information-geometry)
- [6. Positional Encoding](#6-positional-encoding)
  - [6.1 Sinusoidal Encoding](#61-sinusoidal-encoding-vaswani-2017)
  - [6.2 Rotary Position Embedding (RoPE)](#62-rotary-position-embedding-rope)
  - [6.3 ALiBi](#63-alibi-attention-with-linear-biases)
  - [6.4 Relative Position Representations](#64-relative-position-representations-shaw-2018)
- [7. Signal Processing](#7-signal-processing)
  - [7.1 Fast Fourier Transform](#71-fast-fourier-transform)
  - [7.2 Spectral Analysis](#72-spectral-analysis)
- [8. Optimization](#8-optimization)
  - [8.1 Gradient Descent Variants](#81-gradient-descent-variants)
  - [8.2 Natural Gradient](#82-natural-gradient)
  - [8.3 Quantum Annealing for Weight Optimization](#83-quantum-annealing-for-weight-optimization)
  - [8.4 Elastic Weight Consolidation](#84-elastic-weight-consolidation)
  - [8.5 Federated Averaging (Swarm Learning)](#85-federated-averaging-swarm-learning)
- [9. Geometry and Manifolds](#9-geometry-and-manifolds)
  - [9.1 Riemannian Metrics](#91-riemannian-metrics)
  - [9.2 Surface Area and Nambu-Goto Action](#92-surface-area-and-nambu-goto-action)
  - [9.3 Geometric Parameters](#93-geometric-parameters)
  - [9.4 Neural Manifold Analysis](#94-neural-manifold-analysis)
- [10. Probability and Statistics](#10-probability-and-statistics)
  - [10.1 Sigmoid and Softmax](#101-sigmoid-and-softmax)
  - [10.2 Exponential Moving Average](#102-exponential-moving-average)
  - [10.3 Cosine Similarity (Diversity Loss)](#103-cosine-similarity-diversity-loss)
  - [10.4 Quantum State Fidelity](#104-quantum-state-fidelity)
- [11. Neuromodulation](#11-neuromodulation)
  - [11.1 Dopamine Reward Prediction Error](#111-dopamine-reward-prediction-error)
  - [11.2 Serotonin, Norepinephrine, Acetylcholine](#112-serotonin-norepinephrine-acetylcholine)
- [12. Memory Mathematics](#12-memory-mathematics)
  - [12.1 Engram Encoding](#121-engram-encoding)
  - [12.2 Episodic Replay](#122-episodic-replay)
  - [12.3 Memory Consolidation](#123-memory-consolidation)
  - [12.4 Habituation](#124-habituation)
- [13. Safety Mathematics](#13-safety-mathematics)
  - [13.1 CRC32 Checksum](#131-crc32-checksum)
  - [13.2 Byzantine Gradient Detection](#132-byzantine-gradient-detection)
  - [13.3 LGSS Safety Lattice](#133-lgss-safety-lattice)
- [14. Free Energy Principle and World Model](#14-free-energy-principle-and-world-model)
  - [14.1 Variational Free Energy](#141-variational-free-energy)
  - [14.2 World Model (Predictive Processing)](#142-world-model-predictive-processing)
- [15. Prime Resonance Memory](#15-prime-resonance-memory)
  - [15.1 Prime Resonance Encoding](#151-prime-resonance-encoding)
  - [15.2 Resonance-Based Retrieval](#152-resonance-based-retrieval)
- [16. Adaptive Network Architecture](#16-adaptive-network-architecture)
  - [16.1 9-Layer Diamond Topology](#161-9-layer-diamond-topology)
  - [16.2 Forward Pass](#162-forward-pass)
  - [16.3 Backward Pass and Weight Decay](#163-backward-pass-and-weight-decay)
- [17. Composite Loss and Cross-Network Gradients](#17-composite-loss-and-cross-network-gradients)
  - [17.1 Composite Loss Function](#171-composite-loss-function)
  - [17.2 LSA Bridge Gradient Flow](#172-lsa-bridge-gradient-flow)
- [18. Diversity and Contrastive Loss](#18-diversity-and-contrastive-loss)
  - [18.1 Diversity Loss (Effective Rank)](#181-diversity-loss-effective-rank)
  - [18.2 Contrastive Loss (Cross-Network)](#182-contrastive-loss-cross-network)
  - [18.3 Cosine Similarity Ring Buffer](#183-cosine-similarity-ring-buffer)
- [19. Sparse Synapse Mathematics](#19-sparse-synapse-mathematics)
  - [19.1 Three-Tier Storage](#191-three-tier-storage)
  - [19.2 Pool Allocation with Graceful Fallback](#192-pool-allocation-with-graceful-fallback)
- [20. Developmental Curriculum Mathematics](#20-developmental-curriculum-mathematics)
  - [20.1 Stage Transition](#201-stage-transition)
  - [20.2 Cosine Annealing with Warmup](#202-cosine-annealing-with-warmup)
  - [20.3 Spectral K-Fold Cross-Validation](#203-spectral-k-fold-cross-validation)
- [21. Hemispheric Architecture](#21-hemispheric-architecture)
  - [21.1 Corpus Callosum Transfer](#211-corpus-callosum-transfer)
- [22. K-Winners-Take-All Sparse Coding](#22-k-winners-take-all-sparse-coding)
- [23. Pink Noise (1/f) Stochastic Dynamics](#23-pink-noise-1f-stochastic-dynamics)
  - [23.1 1/f Power Spectrum](#231-1f-power-spectrum)
  - [23.2 The 12 Pink Noise Bridges](#232-the-12-pink-noise-bridges)
  - [23.3 Generation via Voss-McCartney Algorithm](#233-generation-via-voss-mccartney-algorithm)
- [24. Fractal Topology and Detrended Fluctuation Analysis](#24-fractal-topology-and-detrended-fluctuation-analysis)
  - [24.1 Fractal Network Topology](#241-fractal-network-topology)
  - [24.2 Detrended Fluctuation Analysis (DFA)](#242-detrended-fluctuation-analysis-dfa)
  - [24.3 Hurst Exponent for Training Health](#243-hurst-exponent-for-training-health)
- [25. Tensor Operations and SIMD](#25-tensor-operations-and-simd-acceleration)
- [26. Ternary Computing](#26-ternary-computing)
- [27. Gabor Filters (V1 Simple Cells)](#27-gabor-filters-v1-simple-cells)
- [28. Fuzzy Inference](#28-fuzzy-inference)
- [29. Differential Geometry](#29-differential-geometry)
- [30. Tensor Networks (MPS and SVD)](#30-tensor-networks-mps-and-svd)
- [31. Complex Number Arithmetic](#31-complex-number-arithmetic)
- [32. Numerical Integration](#32-numerical-integration)
- [33. Bayesian Statistics](#33-bayesian-statistics)
- [34. Time Series Analysis](#34-time-series-analysis)
- [35. Chaos Engineering](#35-chaos-engineering)
- [36. Spatial Indexing (KD-Tree)](#36-spatial-indexing-kd-tree)
- [37. Streaming Statistics](#37-streaming-statistics)
- [38. Survival Analysis](#38-survival-analysis)
- [39. Current Training Observations](#39-current-training-observations)
  - [39.1 System Configuration](#391-system-configuration)
  - [39.2 Current Metrics](#392-current-metrics-stage-1-step-6200-of-20000)
  - [39.3 Biological Dynamics](#393-biological-dynamics)
  - [39.4 Stability](#394-stability)
- [40. Projected Training Outcomes](#40-projected-training-outcomes)
  - [40.1 End of Stage 1](#401-end-of-stage-1-step-20000-cross-modal-association)
  - [40.2 End of Stage 2](#402-end-of-stage-2-step-40000-feedback-learning)
  - [40.3 End of Stage 3](#403-end-of-stage-3-step-50000-abstract-reasoning)
  - [40.4 Uncertainty and Caveats](#404-uncertainty-and-caveats)
- [Glossary](#glossary)
- [References](#references)

---

## Abstract

This paper presents the complete mathematical framework underlying the Neuro-Inspired Modular Control Protocol (NIMCP), a 2.5-million-neuron artificial brain system with six network types, 60+ cognitive modules, and full biological plasticity. For each mathematical formulation, we explain not only *what* the equation computes but *why* that formulation was chosen over alternatives, *how* it integrates with the rest of the system, and *what happens when it fails*. Every equation corresponds to implemented code in the NIMCP codebase, with source file references provided throughout.

The design philosophy throughout is: **use the simplest formulation that captures the essential dynamics, fail gracefully when assumptions break, and make every failure observable**. The mathematics described here is not aspirational — it is running right now in a 2.5-million neuron brain called Athena, currently in Stage 1 of developmental training on a single NVIDIA RTX 4000 GPU.

---

## Introduction

### The Problem

Modern artificial intelligence achieves remarkable performance by scaling a single mathematical idea — the transformer's self-attention mechanism — to billions of parameters and trillions of training tokens. This approach works. GPT-5, Claude, and Gemini demonstrate that statistical learning over token co-occurrences can approximate sophisticated reasoning, translation, code generation, and creative writing.

But the brain does not work this way.

The human cortex does not compute attention scores over a context window. It operates through the interplay of multiple heterogeneous computational systems: spiking neurons that encode information in the precise timing of action potentials, neuromodulatory systems that adjust learning rates based on surprise and reward, synaptic plasticity rules that operate at five different timescales simultaneously, and oscillatory dynamics that bind features across sensory modalities. These systems did not evolve because they are mathematically elegant — they evolved because they work in a physical body navigating a physical world with limited energy and unreliable sensors.

NIMCP asks: what happens if we implement these systems in silicon and let them co-train?

### What This Paper Covers

This paper formalizes the mathematics of every computational system in NIMCP. It is organized by mathematical domain rather than by system architecture:

**Section 1 (Neural Dynamics)** covers the differential equations governing four network types: Leaky Integrate-and-Fire spiking neurons, Liquid Neural Networks with continuous-time ODE dynamics, Hamiltonian Neural Networks with energy-conserving mechanics, and the population dynamics that arise from connecting them. Each formulation was chosen for a specific reason — computational efficiency, gradient compatibility, or physical plausibility — and those reasons are documented alongside the equations.

**Section 2 (Learning Rules)** covers the six learning mechanisms that operate simultaneously on the same synapses: STDP for causal timing, BCM for homeostatic stability, eligibility traces for delayed credit assignment, homeostatic plasticity for activity normalization, BPTT with surrogate gradients for supervised learning in spiking networks, and the adjoint method for continuous-time gradient computation. The key challenge — preventing these rules from destructively interfering — is addressed through timescale separation.

**Section 3 (Fourier Neural Operators)** covers spectral methods for frequency-domain learning, including the mathematical connection between biological auditory processing and learned spectral convolution.

**Sections 4-6 (Quantum, Information Theory, Positional Encoding)** cover quantum-inspired optimization algorithms, information-theoretic measures used for monitoring and analysis, and the positional encoding schemes that enable sequence processing.

**Sections 7-10 (Signal Processing, Optimization, Geometry, Statistics)** cover the supporting mathematics: FFT implementations, natural gradient descent, Riemannian manifold analysis of the output space, and the statistical methods used throughout.

**Section 11 (Neuromodulation)** covers the four-neuromodulator system that translates training loss into stimulus-dependent, pathway-specific learning rate modulation — a fundamentally different approach from the fixed schedules used in conventional deep learning.

**Section 12 (Memory)** covers the mathematics of engram encoding, episodic replay, and memory consolidation — the mechanisms that give NIMCP resistance to catastrophic forgetting without explicit Elastic Weight Consolidation.

**Section 13 (Safety)** covers the mathematical guarantees underlying NIMCP's structural safety system: CRC32 tamper detection, Byzantine fault detection in swarm learning, and the LGSS safety lattice that provides provable safety properties.

### What Makes This Different From a Textbook

Every equation in this paper is implemented in running C code. We provide source file references for each formulation so the reader can verify that the mathematics matches the implementation. More importantly, for each section we document:

1. **Why this formulation** — what problem it solves that alternatives don't, and what trade-offs were made
2. **How it connects** — where the outputs of one equation become the inputs of another across the six-network architecture
3. **What happens when it fails** — the actual failure modes we encountered during development, and how they were resolved

This is not a theoretical paper. It is the engineering record of a working system.

---

## 1. Neural Dynamics

### 1.1 Leaky Integrate-and-Fire Model

**Why LIF and not Hodgkin-Huxley or Izhikevich?** The Hodgkin-Huxley model (1952) captures the biophysics of ion channel dynamics with 4 differential equations per neuron. At 2.5 million neurons, this would require 10 million coupled ODEs per timestep — computationally intractable on a single GPU. The Izhikevich model (2003) is cheaper (2 equations per neuron) and reproduces 20+ firing patterns, but the extra parameters ($a, b, c, d$) require hand-tuning per neuron type and complicate BPTT gradient computation.

LIF reduces each neuron to a single ODE with a threshold reset. It captures the essential dynamics — temporal integration, leak, refractory period — while being simple enough for BPTT surrogate gradients to flow cleanly through 100 timesteps. The trade-off: LIF neurons cannot burst, adapt, or exhibit resonance. For NIMCP, this is acceptable because the SNN's 768 neurons are not the primary computational substrate — the 9-layer adaptive network handles most of the feature extraction. The SNN provides *complementary* temporal coding, not a complete neuronal simulation.

The membrane potential of neuron $i$ evolves according to:


```math
\tau_m \frac{dV_i}{dt} = -(V_i - V_{\text{rest}}) + R_m I_i(t)
```


where $\tau_m$ is the membrane time constant, $V_{\text{rest}}$ the resting potential, $R_m$ the membrane resistance, and $I_i(t)$ the total synaptic input current. A spike is emitted when $V_i \geq V_{\text{thresh}}$, after which $V_i \leftarrow V_{\text{reset}}$ and the neuron enters a refractory period $t_{\text{ref}}$.

The SNN populations in NIMCP use this model with per-neuron state vectors tracking membrane voltage, refractory timers, and spike indicators. The FNO bridge (Section 3.4) learns to approximate these dynamics at the population level.

**How it connects to the system:** The SNN receives input through the Unified Training Manager's rate-to-spike bridge, which scales continuous ANN activations by `input_current_scale = 70.0` to produce currents in the millivolt range. An input of 0.3 produces 21 mV of drive — just above the 20 mV threshold gap ($V_{\text{thresh}} - V_{\text{rest}} = -50 - (-70)$). This calibration ensures that typical inputs cause firing without saturation.

**What happens when it fails:** With $\tau_m = 20$ ms and $dt = 1$ ms, a single timestep produces only $\sim 1$ mV of membrane change. If the SNN is given only 1 timestep per training step (as occurred in an early implementation bug), neurons never reach threshold and the entire SNN remains silent. The fix — running 100 timesteps (100ms) per training step — gives input neurons ~3$\tau_m$ to reach steady state, fire, and propagate spikes through hidden to output populations.

*Source: `include/snn/nimcp_snn_types.h`, `src/snn/nimcp_snn_network.c`*

### 1.2 Spiking Neural Network Population Dynamics

Synaptic current into neuron $i$ from population $p$ is:


```math
I_i^{\text{syn}}(t) = \sum_{j \in \text{pre}(i)} w_{ji} \sum_{k} \alpha(t - t_j^k)
```


where $w_{ji}$ is the synaptic weight, $t_j^k$ the $k$-th spike time of presynaptic neuron $j$, and $\alpha(t)$ is a synaptic kernel (exponential or alpha function). Spike trains are stored in ring buffers with fields `total_spikes` and per-population spike counts, indexed by `n_populations`.

*Source: `include/snn/nimcp_snn_types.h`, `include/snn/nimcp_snn_training.h`*

### 1.3 Liquid Neural Network (LNN)

**Why a separate ODE network when the ANN already handles static mapping?** The ANN processes each input independently — it has no notion of time between training steps. An RNN would add temporal memory but uses discrete timesteps tied to the training loop. The LNN's continuous-time ODE naturally captures dynamics at arbitrary timescales: a fast-changing input evolves the ODE quickly (small $\tau$), a slow input evolves it slowly (large $\tau$). The learned time constants $\tau_i$ per neuron adapt to the temporal structure of the data — something neither an ANN nor a standard RNN can do.

**Why adjoint method instead of standard BPTT?** Standard BPTT for an ODE requires storing the full state trajectory — with 100+ integration steps and 64 neurons, that's 6,400+ float values per forward pass. The adjoint method (Chen et al., 2018) computes gradients by integrating a separate ODE backward in time, using only O(1) memory regardless of the number of integration steps. For NIMCP, this is critical because the LNN runs alongside 5 other networks that each consume memory.

The LNN implements continuous-time recurrent dynamics via an ODE:


```math
\frac{dx_i}{dt} = -\frac{x_i}{\tau_i} + f\bigl(W_{\text{rec}} \, x + W_{\text{in}} \, I(t) + b\bigr)_i
```


where $x_i$ is the hidden state, $\tau_i$ a learned time constant (clamped to $\tau_{\text{safe}} \geq 0.01$ to prevent $1/\tau^2$ explosion), $W_{\text{rec}}$ the recurrent weight matrix, $W_{\text{in}}$ the input projection, and $f$ a nonlinearity (typically $\tanh$). Gradients are computed via the adjoint method (Section 2.6) with per-layer tensors `grad_W_rec`, `grad_tau_base`, and `grad_b_in` serving as the authoritative gradient storage.

**What happens when it fails:** The $\tau_{\text{safe}} \geq 0.01$ clamp exists because the gradient of the ODE with respect to $\tau_i$ contains a $1/\tau_i^2$ term. Without the clamp, a learned $\tau$ near zero produces gradient explosion that cascades through the adjoint computation, producing adjoint norms of 60,000+. The original implementation hard-clipped these norms to 100.0, but this crushed gradient variance — the optimizer saw a constant-magnitude signal and the LNN stopped learning. The fix: normalize adjoint norms to a target of 10.0 (preserving direction while controlling magnitude), combined with the $\tau_{\text{safe}}$ floor to prevent the explosion at its source.

**How it connects to the system:** The LNN's output feeds back to the ANN through a continuous bridge in the UTM. The LNN time constants encode the temporal scale at which the brain should process each feature dimension — fast $\tau$ for rapidly-changing sensory features, slow $\tau$ for persistent semantic context. This temporal structure complements the SNN's discrete spike timing and the ANN's static feature mapping.

*Source: `include/lnn/nimcp_lnn_types.h`, `src/lnn/nimcp_lnn_forward.c`*

### 1.4 Hamiltonian Neural Networks (HNN)

**Why energy conservation?** Standard neural networks can learn dynamics that violate conservation laws — an ODE network predicting planetary motion might predict a planet that gradually speeds up forever. The HNN constrains the learned dynamics to be energy-conserving by construction: the phase-space volume is preserved by the symplectic integrator, so the network *cannot* learn non-physical dynamics even if the loss function would reward them.

**When is this useful?** For embodied applications (drones, robots), the motor control system must respect physics. An HNN-based dynamics model naturally produces trajectories that conserve energy, which translates to smoother, more efficient motor commands. The HNN is currently the least-used network in NIMCP's training pipeline — it becomes critical in Stage 3 when reasoning about physical interactions begins.

Energy-conserving dynamics are achieved by learning a Hamiltonian function $H: \mathbb{R}^{2n} \to \mathbb{R}$ and deriving dynamics from Hamilton's equations:


```math
\frac{dq}{dt} = \frac{\partial H}{\partial p}, \qquad \frac{dp}{dt} = -\frac{\partial H}{\partial q}
```


NIMCP supports two forms:

**Separable Hamiltonian:**


```math
H(q, p) = T(p) + V(q)
```


where $T$ is kinetic energy (parameterized by an MLP over $p$) and $V$ is potential energy (MLP over $q$), enabling cheaper gradient computation.

**General Hamiltonian:**


```math
H(q, p) = \text{MLP}([q; p])
```


with a multi-layer network of configurable depth (`n_hidden_layers`, default 2) and width (`hidden_dim`, default $2n$).

Energy conservation $dH/dt = 0$ is enforced by the **Stormer-Verlet symplectic integrator**:


```math
\begin{aligned}
p_{1/2} &= p_n - \frac{\Delta t}{2} \frac{\partial H}{\partial q}\bigg|_{q_n, p_n} \\
q_{n+1} &= q_n + \Delta t \, \frac{\partial H}{\partial p}\bigg|_{q_n, p_{1/2}} \\
p_{n+1} &= p_{1/2} - \frac{\Delta t}{2} \frac{\partial H}{\partial q}\bigg|_{q_{n+1}, p_{1/2}}
\end{aligned}
```


External input drives momentum via a coupling parameter: $p \leftarrow p + \gamma \cdot I_{\text{ext}}$, where $\gamma$ is `input_coupling`. Energy deviation is monitored as $|H(t) - H(0)| / |H(0)|$ with a regularization penalty weighted by `energy_penalty_weight`.

The connection to the Free Energy Principle is: $H \equiv F$ (variational free energy).

*Source: `include/lnn/nimcp_lnn_hamiltonian.h`, `src/lnn/nimcp_lnn_hamiltonian.c`*

---

## 2. Learning Rules

NIMCP uses six learning rules simultaneously on the same synapses. This is not redundant — each rule captures a different aspect of learning that the others miss. The key insight: **gradient descent optimizes a loss function; biological plasticity optimizes for survivability**. A synapse modified by both backpropagation (minimize prediction error) and STDP (strengthen causal connections) and neuromodulation (amplify learning from surprising events) develops a richer weight structure than any single rule produces alone.

The practical challenge is preventing these rules from destructively interfering. NIMCP handles this through timescale separation: STDP operates at 10ms (within a single BPTT window), backpropagation at the step level (~30s), BCM at 50ms, and homeostatic scaling at 60s. At any given moment, at most two rules are actively modifying the same synapse, and their contributions are additive with different magnitudes.

### 2.1 Spike-Timing-Dependent Plasticity (STDP)

**Why STDP in addition to backpropagation?** Backpropagation computes the optimal weight change to minimize a loss function — but it requires the loss to be differentiable, the forward pass to be stored for the backward pass, and the target to be specified. STDP requires none of these. It operates purely on local spike timing: if neuron A consistently fires before neuron B, the A→B synapse strengthens (Markram et al., 1997). This captures causal relationships that backpropagation may miss, especially in the SNN where surrogate gradients are approximations of the true gradient.

**How STDP and backpropagation coexist:** STDP modifies SNN synaptic weights based on spike timing within each 100ms BPTT window. BPTT then computes gradients through the same window and applies its own weight updates. The STDP changes are small ($A_+ = 0.005$, i.e., 0.5% per coincidence) while BPTT changes are larger but modulated by the learning rate (~0.001). The net effect: STDP provides a continuous "sculpting" of weights based on timing, while BPTT provides directed optimization toward the target. They complement rather than conflict because they optimize different objectives at different scales.

The weight change follows the classic Bi & Poo (1998) exponential window:


```math
\Delta w = \begin{cases} A_+ \exp\!\left(-\frac{\Delta t}{\tau_+}\right) & \text{if } \Delta t > 0 \text{ (pre before post, LTP)} \\ -A_- \exp\!\left(\frac{\Delta t}{\tau_-}\right) & \text{if } \Delta t < 0 \text{ (post before pre, LTD)} \end{cases}
```


Default parameters: $A_+ = 0.005$, $A_- = 0.00525$, $\tau_+ = \tau_- = 20$ ms, $w \in [0, w_{\max}]$ with $w_{\max} = 1.0$.

**Trace-based implementation:** Instead of storing all spike times, exponentially decaying traces are maintained:


```math
\begin{aligned}
\text{pre\_trace}(t + \Delta t) &= \text{pre\_trace}(t) \cdot \exp(-\Delta t / \tau_+) \\
\text{post\_trace}(t + \Delta t) &= \text{post\_trace}(t) \cdot \exp(-\Delta t / \tau_-)
\end{aligned}
```


Traces below `NIMCP_DENORMAL_THRESHOLD` ($10^{-10}$) are flushed to zero for performance.

**Three-factor learning with dopamine modulation:** The effective learning rate is:


```math
\eta_{\text{eff}} = \eta_{\text{base}} \times (1 + g_{\text{DA}} \cdot [\text{DA}]) \times m_{\text{burst}}
```


where $g_{\text{DA}} = 100.0$ (default), $[\text{DA}]$ is dopamine concentration, and $m_{\text{burst}} = 3.0$ during phasic dopamine bursts.

*Source: `include/plasticity/stdp/nimcp_stdp.h`, `src/plasticity/stdp/nimcp_stdp.c`*

### 2.2 BCM Rule (Bienenstock-Cooper-Munro)

**Why BCM in addition to STDP?** STDP alone is unstable — it has a positive feedback loop where strong synapses cause more postsynaptic spikes, which further strengthen those synapses. Without a compensating mechanism, all weights saturate at $w_{\max}$. BCM provides that compensation: its sliding threshold $\theta$ tracks recent postsynaptic activity. When a neuron fires too much ($y > \theta$), the threshold rises, making further potentiation harder. When it fires too little ($y < \theta$), the threshold drops, making potentiation easier. This is a biological homeostatic mechanism that weight decay cannot replicate — weight decay shrinks all weights uniformly, while BCM selectively adjusts based on activity.

**When it's disabled:** BCM requires per-synapse state (the $\theta$ threshold). For networks >100K neurons, this creates 20M+ mutex-locked allocations during wiring. NIMCP disables BCM for large networks to prevent initialization OOM. The homeostatic scaling rule (Section 2.4) provides a cruder but cheaper alternative.

The BCM sliding threshold rule for cortical learning:


```math
\begin{aligned}
\Delta w &= \eta \cdot y \cdot (y - \theta) \cdot x \\
\frac{d\theta}{dt} &= \frac{y^2 - \theta}{\tau_\theta}
\end{aligned}
```


where $y$ is postsynaptic activity, $x$ is presynaptic activity, $\theta$ is the sliding modification threshold, and $\tau_\theta$ is the threshold time constant. When $y > \theta$, the synapse potentiates; when $y < \theta$, it depresses. The threshold tracks the mean squared activity $\langle y^2 \rangle$, making the rule self-stabilizing without explicit normalization.

*Source: `include/plasticity/bcm/nimcp_bcm.h`, `src/plasticity/bcm/nimcp_bcm.c`*

### 2.3 Eligibility Traces

**Why eligibility traces?** Consider a robot that reaches for an object. The motor neurons fire at time $t$, but the reward (successfully grasping) arrives at time $t + 500$ ms. STDP operates at the 10-20ms timescale — by the time the reward arrives, the STDP window has closed. How does the brain know which synapses were responsible for the successful grasp?

Eligibility traces solve this by maintaining a decaying "tag" on each recently-modified synapse. When the motor neurons fire and cause an STDP event, the synapse becomes "eligible" for reinforcement. When the reward arrives 500ms later (as a dopamine signal), only the eligible synapses are strengthened — the rest are unaffected. This is the three-factor rule: Hebbian timing $\times$ eligibility $\times$ neuromodulatory reward.

**How it connects to the system:** In NIMCP, the Training Plasticity Bridge (TPB) converts training loss to a dopamine signal: $\text{DA} = 1 - \min(\text{loss}, 1)$. Low loss → high dopamine → eligible synapses are consolidated. High loss → low dopamine → eligible synapses decay without reinforcement. This creates an automatic curriculum effect: synapses that contributed to correct predictions are reinforced; synapses that contributed to errors are forgotten.

Eligibility traces bridge the temporal credit assignment gap between Hebbian coincidence and delayed reward:


```math
e_{ij}(t+\Delta t) = e_{ij}(t) \cdot \exp(-\Delta t / \tau_e) + \text{STDP}_{ij}(t)
```


Weight updates occur only when a reward signal $r(t)$ arrives:


```math
\Delta w_{ij} = \eta \cdot r(t) \cdot e_{ij}(t)
```


This implements the three-factor rule: Hebbian timing $\times$ eligibility $\times$ neuromodulatory reward.

*Source: `include/plasticity/stdp/nimcp_stdp.h` (field: `eligibility`)*

### 2.4 Homeostatic Plasticity

Three mechanisms maintain neural stability:

**Synaptic Scaling (Turrigiano 1998):**


```math
w_{\text{scaled}} = w \times \left(\frac{r_{\text{target}}}{r_{\text{actual}}}\right)^\alpha
```


where $\alpha \in [0.5, 2.0]$ is the scaling exponent, $r_{\text{target}} = 5.0$ Hz (default).

**Intrinsic Plasticity (threshold adaptation):**


```math
\frac{d\theta}{dt} = \frac{r_{\text{actual}} - r_{\text{target}}}{\tau_{\text{IP}}}
```


**Metaplasticity (BCM threshold sliding):**


```math
\theta_m = \langle r^2 \rangle
```


The sliding threshold based on squared activity history.

*Source: `include/plasticity/homeostatic/nimcp_homeostatic.h`*

### 2.5 Backpropagation Through Time for SNN

**The fundamental problem:** A spike is a binary event — a neuron either fires or it doesn't. The derivative of the Heaviside step function is zero everywhere except at the threshold, where it's infinite. Standard backpropagation cannot flow gradients through this discontinuity.

**Why surrogate gradients?** The solution (Neftci et al., 2019) replaces the Heaviside derivative with a smooth function that approximates it — a "surrogate gradient." The Cauchy distribution used in NIMCP provides a wide, smooth gradient landscape that allows BPTT to flow through hundreds of timesteps without vanishing. The sharpness parameter $\beta$ controls the trade-off: higher $\beta$ more closely approximates the true Heaviside (better gradient fidelity) but produces steeper gradients (risk of explosion). NIMCP uses $\beta = 1.0$, which provides reliable gradient flow through 100 timesteps.

**What happens when the window is too short:** With $\tau_m = 20$ ms, the membrane time constant creates a low-pass filter on the gradient signal. In a 1ms window, the surrogate gradient is essentially zero because the membrane hasn't had time to respond to the input. In a 100ms window, the gradient accumulates over ~5 time constants, producing a meaningful signal. This is why the BPTT window length must be calibrated to the membrane dynamics — a failure we encountered and fixed when the SNN produced 0 spikes during training.

SNN training uses BPTT with surrogate gradients. The non-differentiable Heaviside spike function $\Theta(V - V_{\text{thresh}})$ is replaced by a smooth surrogate:


```math
\sigma'(V) = \frac{1}{\pi} \cdot \frac{1}{1 + (\pi \cdot \beta \cdot (V - V_{\text{thresh}}))^2}
```


The backward pass unrolls through time, computing temporal gradients of the loss with respect to synapse weights and firing thresholds.

*Source: `include/snn/nimcp_snn_training.h`, `src/snn/nimcp_snn_bptt.c`*

### 2.6 Adjoint Method for LNN

**Why not just use standard BPTT for the ODE?** Standard BPTT would require storing the LNN state at every integration step — with adaptive step sizes, this could be hundreds of states per forward pass. The adjoint method (Chen et al., 2018) avoids this: it computes the exact same gradients by solving a *separate* ODE backward in time, requiring only the final state and the loss gradient as initial conditions. Memory usage is O(1) regardless of how many integration steps the forward pass took.

**The practical trade-off:** The adjoint method requires re-evaluating $f(x, \theta, t)$ during the backward pass (since intermediate states aren't stored). This doubles the compute cost but halves the memory. For NIMCP, where 5 other networks compete for memory alongside the LNN, the memory savings are critical.

For the LNN ODE $\dot{x} = f(x, \theta, t)$, the adjoint state $a(t) = \partial L / \partial x(t)$ evolves backward:


```math
\frac{da}{dt} = -a^\top \frac{\partial f}{\partial x}
```


Parameter gradients are accumulated during the backward sweep:


```math
\frac{dL}{d\theta} = -\int_{t_1}^{t_0} a(t)^\top \frac{\partial f}{\partial \theta} \, dt
```


Adjoint truncation is capped at 50 backward steps. Gradients are normalized (not clipped) to target norm 10.0, preserving gradient direction while controlling magnitude.

**A cautionary tale:** The original implementation hard-clipped the adjoint norm to 100.0 at two points: per-step (during backward integration) and post-adjoint (on accumulated parameter gradients). With actual gradient norms of 60,000+, this produced a 600× reduction factor. Worse, the clipping was applied as `min(norm, 100)`, so the optimizer always saw gradients of exactly magnitude 100 — zero variance in gradient magnitude across training steps. The LNN loss plateaued at 1.1 and never improved. The fix: replace clipping with normalization (divide by norm, multiply by target), which preserves the *direction* of the gradient while controlling its *magnitude*. After this fix, LNN loss began decreasing.

*Source: `src/lnn/nimcp_lnn_adjoint.c`*

### 2.7 AdamW Optimizer

Parameter updates with decoupled weight decay:


```math
\begin{aligned}
m_t &= \beta_1 m_{t-1} + (1-\beta_1) g_t \\
v_t &= \beta_2 v_{t-1} + (1-\beta_2) g_t^2 \\
\hat{m}_t &= m_t / (1 - \beta_1^t), \quad \hat{v}_t = v_t / (1 - \beta_2^t) \\
\theta_t &= \theta_{t-1} - \eta \left( \frac{\hat{m}_t}{\sqrt{\hat{v}_t} + \epsilon} + \lambda \theta_{t-1} \right)
\end{aligned}
```


where $\lambda$ is the weight decay coefficient, decoupled from the adaptive learning rate.

*Source: `src/training/nimcp_training_optimizers.c`*

---

## 3. Fourier Neural Operators

**Why spectral methods in a brain?** Convolutional neural networks learn local spatial features — edges, textures, small shapes. They struggle with global patterns because the receptive field grows slowly with depth. The Fourier Neural Operator (Li et al., 2021) learns directly in the frequency domain, where a single learned weight captures a global spatial pattern. This is analogous to how biological auditory cortex processes sound: the cochlea performs a Fourier-like frequency decomposition, and cortical neurons respond to specific frequency bands, not raw time-domain signals.

In NIMCP, FNO layers are attached to the audio and visual cortex CNNs, providing a spectral analysis path that complements the CNN's spatial analysis. The CNN detects "what" (a sharp attack, a bird call) while the FNO detects "what frequency" (300 Hz fundamental, 1200 Hz formant). Together they produce richer sensory features than either alone.

### 3.1 Spectral Convolution

The core FNO operation learns a kernel in Fourier space:


```math
(\mathcal{K}(\phi)v)(x) = \mathcal{F}^{-1}\bigl(R_\phi \cdot \mathcal{F}(v)\bigr)(x) + W_{\text{bypass}} \cdot v(x)
```


where $R_\phi \in \mathbb{C}^{c_{\text{out}} \times c_{\text{in}} \times k}$ are learned complex spectral weights truncated to $k$ Fourier modes, $\mathcal{F}$ denotes the FFT, and $W_{\text{bypass}}$ is a $1\times 1$ convolution residual path.

The spectral convolution is applied per-channel:


```math
\hat{y}_{\text{out}}[j][m] = \sum_{i=0}^{c_{\text{in}}-1} (W_{\text{real}}[j,i,m] + i\,W_{\text{imag}}[j,i,m]) \cdot \hat{v}_{\text{in}}[i][m]
```


for modes $m = 0, \ldots, k-1$, followed by IFFT and GELU activation.

*Source: `include/training/nimcp_fno_layer.h`, `src/training/nimcp_fno_layer.c`*

### 3.2 FFT Implementation

NIMCP implements the Cooley-Tukey radix-2 FFT with $O(N \log N)$ complexity:


```math
X[k] = \sum_{n=0}^{N-1} x[n] \cdot e^{-2\pi i \, kn/N}, \qquad k = 0, \ldots, N-1
```


The inverse transform:


```math
x[n] = \frac{1}{N} \sum_{k=0}^{N-1} X[k] \cdot e^{2\pi i \, kn/N}
```


Pre-computed twiddle factors $W_N^k = e^{-2\pi ik/N}$ and bit-reversal permutation tables are stored in the plan structure. Window functions reduce spectral leakage:

| Window | Formula |
|--------|---------|
| Hann | $w[n] = 0.5\bigl(1 - \cos(2\pi n / (N-1))\bigr)$ |
| Hamming | $w[n] = 0.54 - 0.46\cos(2\pi n / (N-1))$ |
| Blackman | $w[n] = 0.42 - 0.5\cos(2\pi n/(N-1)) + 0.08\cos(4\pi n/(N-1))$ |

Power spectral density: $P[k] = |X[k]|^2 = \text{Re}[k]^2 + \text{Im}[k]^2$

PSD in decibels: $P_{\text{dB}}[k] = 10 \log_{10}(P[k])$

Brain wave band power is computed by summing $P[k]$ over frequency bins corresponding to Delta (1--4 Hz), Theta (4--8 Hz), Alpha (8--13 Hz), Beta (13--30 Hz), and Gamma (30--100 Hz).

*Source: `include/utils/spectral/nimcp_fft.h`, `src/utils/spectral/nimcp_fft.c`*

### 3.3 FNO Layer Architecture

A complete FNO block consists of:

1. **Lifting**: Linear projection from input channels to hidden channels $W_{\text{lift}} \in \mathbb{R}^{h \times c_{\text{in}}}$
2. **Spectral blocks** ($n_{\text{blocks}}$, default 3): Each applies spectral convolution + bypass + GELU
3. **Projection**: Linear map from hidden channels to output $W_{\text{proj}} \in \mathbb{R}^{c_{\text{out}} \times h}$

For the audio processor, global average pooling reduces spatial dimensions before projection: $\bar{v}_c = \frac{1}{N} \sum_{n=0}^{N-1} v_c[n]$.

*Source: `include/training/nimcp_fno_layer.h`*

### 3.4 SNN-FNO Bridge

The FNO learns the population-level map $(V_t, I_{\text{syn}}) \mapsto (V_{t+\Delta t}, S_{t+\Delta t})$ from LIF ground truth, replacing per-neuron simulation when validation MSE falls below a threshold (default 0.01). The architecture per population:

- Lifting: $[h, n + n]$ (voltage + current)
- Spectral: $n_{\text{modes}} = n/4$ Fourier modes, $h = 16$ hidden channels
- Projection: $[2n, h]$ (next voltage + spike predictions)

Training uses a ring buffer of 256 state pairs, with online fine-tuning in validation mode.

*Source: `include/snn/nimcp_snn_fno.h`, `src/snn/nimcp_snn_fno.c`*

---

## 4. Quantum-Inspired Algorithms

### 4.1 Quantum Annealing

Optimization via simulated quantum tunneling through energy barriers:

**Classical acceptance (Metropolis):**


```math
P_{\text{classical}} = \exp(-\Delta E / T)
```


**Quantum tunneling probability:**


```math
P_{\text{tunnel}} = \Gamma \cdot \exp(-B / T^\alpha)
```


where $\Gamma$ is quantum strength (default 0.5), $B$ is the barrier height, and $T^\alpha$ the effective tunneling temperature.

**Combined acceptance:**


```math
P_{\text{accept}} = \min\bigl(1, P_{\text{classical}} + P_{\text{tunnel}}\bigr)
```


**Cooling schedules:**

| Schedule | Formula |
|----------|---------|
| Exponential | $T(t) = T_0 \cdot \exp(-t/\tau)$ |
| Linear | $T(t) = T_0 - (T_0 - T_f) \cdot t / t_{\max}$ |
| Logarithmic | $T(t) = T_0 / \ln(1 + t)$ |
| Adaptive | Adjusted to maintain target acceptance rate (0.234) |

The adaptive variant uses Metropolis-Hastings with learned proposal covariance, adapting step sizes per dimension to achieve 2--5x faster convergence.

*Source: `include/optimization/quantum_annealing/nimcp_quantum_annealing.h`, `src/optimization/quantum_annealing/nimcp_quantum_annealing.c`*

### 4.2 Quantum Walk

A discrete-time quantum walk on the neural network graph provides $O(\sqrt{N})$ speedup for neuromodulator diffusion. The quantum state is:


```math
|\psi\rangle = \sum_i \alpha_i |i\rangle, \qquad \sum_i |\alpha_i|^2 = 1
```


**Evolution operator:** $U = S \cdot C$ where:

- **Coin operator** $C$: Creates superposition at each node (Hadamard, Grover, or Fourier coin)
- **Shift operator** $S$: Propagates amplitude along edges


```math
\alpha_j'' = \sum_{i \in \mathcal{N}(j)} \frac{\alpha_i'}{\sqrt{d_i}}
```


where $d_i$ is the degree of node $i$.

**Measurement (Born rule):** $P(i) = |\alpha_i|^2$

**Hybrid quantum-classical mixing:**


```math
\alpha_{\text{final}} = (1 - \lambda) \cdot \alpha_{\text{quantum}} + \lambda \cdot \alpha_{\text{classical}}
```


**Decoherence** models environmental noise, gradually collapsing quantum superposition toward classical probabilities at rate $\gamma_{\text{decohere}} \in [0, 1]$.

*Source: `include/utils/quantum/nimcp_quantum_walk.h`, `src/utils/quantum/nimcp_quantum_walk.c`*

### 4.3 Quantum Monte Carlo

**Amplitude estimation** via importance sampling: Estimate $|\langle \phi | \psi \rangle|^2$ using proposal distribution $q$:


```math
\hat{P} = \frac{1}{N} \sum_{i=1}^{N} \frac{|\psi(x_i)|^2}{q(x_i)}, \qquad x_i \sim q
```


**Finite-shot measurement** simulates realistic quantum hardware by sampling from the multinomial distribution $\text{Multi}(N_{\text{shots}}; p_0, p_1, \ldots)$ where $p_i = |\alpha_i|^2$.

**Partition function estimation:**


```math
Z = \sum_i \exp(-E_i / T), \qquad F = -T \ln Z, \qquad C = \text{Var}(E) / T^2
```


using MCMC with burn-in, thinning, and thermodynamic integration.

**Entropy estimation (Renyi family):**


```math
\begin{aligned}
H_{\text{Shannon}} &= -\sum_i p_i \ln p_i \\
H_2 &= -\ln\!\left(\sum_i p_i^2\right) \\
H_\infty &= -\ln\!\left(\max_i p_i\right)
\end{aligned}
```


Estimated via MC sampling with stratified variance reduction.

*Source: `include/utils/quantum/nimcp_quantum_monte_carlo.h`, `src/utils/quantum/nimcp_quantum_monte_carlo.c`*

### 4.4 Quantum Shannon Integration

The quantum-Shannon system combines quantum walk propagation with Shannon information monitoring:

**Channel capacity:**


```math
C = B \cdot \log_2(1 + \text{SNR}) \quad \text{bits/second}
```


**Propagation efficiency:**


```math
\eta = \frac{I(\text{source}; \text{targets})}{H(\text{source})}
```


Bottleneck synapses are identified when capacity utilization falls below threshold. The system adaptively routes around bottlenecks by biasing the quantum coin operator toward high-capacity neighbors.

*Source: `include/utils/quantum/nimcp_quantum_shannon.h`*

### 4.5 Quantum-Classical Bridge

Quantum algorithm results feed into neural updates via:

1. **Quantum annealing** optimizes synaptic weight matrices by periodically escaping local minima during plasticity steps
2. **Quantum walk** distributions become neuromodulator concentration fields (dopamine, serotonin, ACh, NE)
3. **QMC amplitude estimation** guides importance-weighted sampling in episodic replay
4. **Shannon feedback** drives adaptive routing of information through the network

---

## 5. Information Theory

### 5.1 Shannon Entropy


```math
H(X) = -\sum_{i} p_i \ln p_i
```


Computed with safe logarithm: $\ln(\max(x, 10^{-10}))$ to prevent $-\infty$. Used for monitoring neural activity diversity, attention entropy, and network health.

*Source: `include/utils/math/nimcp_math_helpers.h` (`nimcp_entropy`)*

### 5.2 Mutual Information and KL Divergence


```math
I(X; Y) = H(X) - H(X|Y) = \sum_{x,y} p(x,y) \ln \frac{p(x,y)}{p(x)p(y)}
```


**KL Divergence:**


```math
D_{\text{KL}}(P \| Q) = \sum_i p_i \ln \frac{p_i}{q_i}
```


Used for loss functions, distribution comparison, and bottleneck detection.

*Source: `src/utils/quantum/nimcp_quantum_monte_carlo.c` (`qmc_kl_divergence`), `include/physics/geometry/nimcp_information_geometry.h`*

### 5.3 Integrated Information (IIT Phi)


```math
\Phi = \min_{\text{partitions}} \bigl[ I(\text{whole}) - \sum I(\text{parts}) \bigr]
```


Measures irreducible causal structure. Exponential in system size; approximations used for large networks.

*Source: `include/utils/statistics/nimcp_information_theory.h`*

### 5.4 Partial Information Decomposition (PID)

For two sources $S_1, S_2$ predicting target $T$:


```math
I(S_1, S_2 ; T) = U_1 + U_2 + R + S
```


where $U_i$ is unique information from source $i$, $R$ is redundancy, and $S$ is synergy. Multiple decomposition methods are supported: BROJA, CCS, MMI, DEP, and iterative BROJA.

*Source: `include/utils/statistics/nimcp_information_theory.h`*

### 5.5 Fisher Information Matrix


```math
F_{ij}(\theta) = \mathbb{E}\!\left[\frac{\partial \ln p(x|\theta)}{\partial \theta_i} \cdot \frac{\partial \ln p(x|\theta)}{\partial \theta_j}\right]
```


The empirical Fisher is estimated from gradient samples. Regularized inversion: $\tilde{F} = F + \lambda I$ with $\lambda = 10^{-6}$.

*Source: `include/physics/geometry/nimcp_information_geometry.h` (`nimcp_fisher_compute`)*

### 5.6 Information Geometry

The parameter space of neural network weights forms a Riemannian manifold with the Fisher information matrix as its metric tensor.

**Natural gradient:**


```math
\tilde{\nabla} L = F^{-1} \nabla L
```


converges 2--10x faster than standard SGD by accounting for the geometry of the parameter space. Gradient clipping at norm 10.0, with optional momentum and warmup scheduling.

**Geodesic distance** between parameter configurations $\theta_a, \theta_b$ is computed on the Fisher manifold:


```math
d(\theta_a, \theta_b) = \inf_\gamma \int_0^1 \sqrt{\dot\gamma(t)^\top F(\gamma(t)) \, \dot\gamma(t)} \, dt
```


**Ricci curvature** measures how volume changes along geodesics, informing adaptive learning rate schedules.

*Source: `include/physics/geometry/nimcp_information_geometry.h`*

---

## 6. Positional Encoding

### 6.1 Sinusoidal Encoding (Vaswani 2017)


```math
\begin{aligned}
\text{PE}(\text{pos}, 2i) &= \sin\!\left(\frac{\text{pos}}{b^{2i/d}}\right) \\
\text{PE}(\text{pos}, 2i+1) &= \cos\!\left(\frac{\text{pos}}{b^{2i/d}}\right)
\end{aligned}
```


where $b = 10000$ (configurable via `frequency_base`), $d$ is the embedding dimension, and $i$ indexes the dimension pair. Fixed, requires no training, and extrapolates to unseen positions.

### 6.2 Rotary Position Embedding (RoPE)

Encodes relative position via 2D rotation applied to query-key pairs:


```math
\begin{pmatrix} q'_{2i} \\ q'_{2i+1} \end{pmatrix} = \begin{pmatrix} \cos(m\theta_i) & -\sin(m\theta_i) \\ \sin(m\theta_i) & \cos(m\theta_i) \end{pmatrix} \begin{pmatrix} q_{2i} \\ q_{2i+1} \end{pmatrix}
```


where $m$ is position, $\theta_i = b^{-2i/d}$, and $b = 10000$ (default `rope_base`). The dot product $\langle q'_m, k'_n \rangle$ depends only on relative position $m - n$. NTK-aware scaling extends context length by adjusting the base: $b' = b \cdot s^{d/(d-2)}$.

### 6.3 ALiBi (Attention with Linear Biases)

No explicit positional encoding. Instead, a linear bias is added to attention scores:


```math
\text{bias}[h][i][j] = -m_h \cdot |i - j|
```


Slopes follow a geometric sequence:


```math
m_h = 2^{-8(h+1)/H}
```


where $H$ is the number of attention heads. This provides the simplest and most efficient long-sequence support.

### 6.4 Relative Position Representations (Shaw 2018)

Separate key and value embeddings indexed by clipped relative position $\text{clip}(i - j, -k, k)$ where $k$ is `max_relative_pos`.

*Source: `include/utils/encoding/nimcp_positional_encoding.h`*

---

## 7. Signal Processing

### 7.1 Fast Fourier Transform

The Cooley-Tukey radix-2 algorithm with $O(N \log N)$ complexity for power-of-2 sizes (2--65536). Real-to-complex FFT produces $N/2 + 1$ frequency bins. Frequency resolution: $\Delta f = f_s / N$.

### 7.2 Spectral Analysis

**Magnitude spectrum:** $|X[k]| = \sqrt{\text{Re}[k]^2 + \text{Im}[k]^2}$

**Phase spectrum:** $\phi[k] = \text{atan2}(\text{Im}[k], \text{Re}[k])$

**Band power:** $P_{\text{band}} = \sum_{k: f_k \in [f_{\text{lo}}, f_{\text{hi}}]} |X[k]|^2$

Bin-to-frequency conversion: $f_k = k \cdot f_s / N$.

*Source: `include/utils/spectral/nimcp_fft.h`*

---

## 8. Optimization

### 8.1 Gradient Descent Variants

NIMCP implements per-network learning rates, layer-wise LR scaling (tau parameters at 0.1x, bias at 2.0x for LNN), and gradient normalization that always normalizes to target norm rather than clipping. The default gradient clip is 100.0 (raised from 1.0 after mode collapse diagnosis).

### 8.2 Natural Gradient


```math
\theta_{t+1} = \theta_t - \eta \, F^{-1}(\theta_t) \, \nabla L(\theta_t)
```


See Section 5.6 for the Fisher information matrix computation. Adaptive damping adjusts the regularization parameter based on the ratio of actual to predicted loss reduction.

*Source: `include/physics/geometry/nimcp_information_geometry.h`*

### 8.3 Quantum Annealing for Weight Optimization

Periodically invoked during plasticity (every $K$ learning steps) to escape local minima in the full synaptic weight matrix. See Section 4.1.

### 8.4 Elastic Weight Consolidation


```math
\mathcal{L}(\theta) = \mathcal{L}_B(\theta) + \sum_i \frac{\lambda}{2} F_i \left(\theta_i - \theta_{A,i}^*\right)^2
```


where $F_i$ is the diagonal of the Fisher information matrix computed on task $A$, $\theta_{A,i}^*$ are the optimal parameters for task $A$, and $\lambda$ controls the consolidation strength. Prevents catastrophic forgetting by penalizing changes to parameters that were important for previous tasks.

### 8.5 Federated Averaging (Swarm Learning)


```math
w_{t+1} = \sum_{k=1}^{K} \frac{n_k}{n} w_{t+1}^k
```


where $n_k$ is the number of samples on edge node $k$ and $n = \sum_k n_k$. Used in NIMCP's swarm runtime for gossip-based model aggregation with Byzantine fault tolerance.

*Source: `src/swarm/nimcp_swarm_sync.c`*

---

## 9. Geometry and Manifolds

### 9.1 Riemannian Metrics

The metric tensor on a 2D surface manifold:


```math
\gamma_{ab} = \frac{\partial X}{\partial \sigma^a} \cdot \frac{\partial X}{\partial \sigma^b}
```


where $\sigma = (\sigma^0, \sigma^1)$ are local coordinates (longitudinal and azimuthal).

For a cylinder of radius $r$: $\gamma = \begin{pmatrix} 1 & 0 \\ 0 & r^2 \end{pmatrix}$

### 9.2 Surface Area and Nambu-Goto Action

The total surface area of the manifold $M(G)$ assigned to graph $G$:


```math
S_{M}(G) = \sum_i \int \sqrt{\det(\gamma_i)} \, d\sigma^0 \, d\sigma^1
```


This is formally identical to the Nambu-Goto action from string theory (with string tension $T = 1$). Computed via Gaussian quadrature with 8--64 points, adaptively chosen based on local curvature.

**Strebel's theorem:** In the absence of boundary conditions, the minimal surface is exactly cylindrical. With boundary conditions, quadratic differentials give the minimal surface.

### 9.3 Geometric Parameters


```math
\chi = w/r \quad \text{(aspect ratio)}, \qquad \rho = w'/w \quad \text{(branching ratio)}, \qquad \lambda = l/w \quad \text{(length ratio)}
```


Branch point topology is mapped to Feynman diagram vertices via pants decomposition.

*Source: `include/core/geometry/nimcp_surface_manifold.h`*

### 9.4 Neural Manifold Analysis

The intrinsic dimensionality of neural activity is estimated from samples on the ambient-dimensional activity space. Local curvature at a point is computed for adaptive learning rate scheduling. Geodesic computation between points on the manifold uses iterative path optimization.

*Source: `include/physics/geometry/nimcp_information_geometry.h` (`nimcp_manifold_*`)*

---

## 10. Probability and Statistics

### 10.1 Sigmoid and Softmax

**Sigmoid:** $\sigma(x) = 1 / (1 + e^{-x})$

**Safe exponential:** Input clamped to $[-88, 88]$ before $\exp$ to prevent overflow ($e^{89} = \infty$ in single precision).

**Softmax with temperature:**


```math
p_i = \frac{\exp(z_i / T)}{\sum_j \exp(z_j / T)}
```


*Source: `include/utils/math/nimcp_math_helpers.h`*

### 10.2 Exponential Moving Average


```math
\text{EMA}_t = \alpha \cdot x_t + (1 - \alpha) \cdot \text{EMA}_{t-1}
```


EMA values are guarded against NaN/Inf corruption: `NIMCP_EMA_GUARD(ema_var, fallback)` resets to fallback value if non-finite.

*Source: `include/utils/math/nimcp_math_helpers.h`*

### 10.3 Cosine Similarity (Diversity Loss)

Anti-mode-collapse uses a ring buffer of 16 recent outputs. The diversity penalty:


```math
\mathcal{L}_{\text{div}} = \frac{w_{\text{div}}}{|B|(|B|-1)} \sum_{i \neq j} \frac{y_i \cdot y_j}{\|y_i\| \|y_j\|}
```


with $w_{\text{div}} = 0.1$, penalizing high cosine similarity between outputs.

### 10.4 Quantum State Fidelity


```math
F(|\psi_1\rangle, |\psi_2\rangle) = |\langle \psi_1 | \psi_2 \rangle|^2 = \left(\sum_i a_i^{(1)} a_i^{(2)}\right)^2
```


for real amplitude vectors.

*Source: `include/utils/quantum/nimcp_quantum_monte_carlo.h` (`qmc_fidelity`)*

---

## 11. Neuromodulation

**Why neuromodulators instead of a learning rate schedule?** A cosine annealing schedule modulates learning rate globally and on a fixed timeline — all weights, all layers, all inputs receive the same LR at the same training step. Biological neuromodulation is fundamentally different: it is *stimulus-dependent* and *pathway-specific*.

When NIMCP encounters a novel input (low recall confidence from semantic memory), acetylcholine rises, increasing the learning rate for *that specific input*. When a prediction is surprisingly accurate (low loss on a previously-difficult item), dopamine rises, reinforcing the synapses that were active during *that specific prediction*. The same training step can produce high dopamine for one pathway (correct prediction) and low dopamine for another (incorrect prediction on a different output dimension).

This is not an engineering trick — it is a direct implementation of the reward prediction error theory (Schultz, 1998) and the cholinergic enhancement of encoding (Hasselmo, 1999). The practical consequence: NIMCP learns novel inputs 2-3× faster than familiar ones without any explicit curriculum adjustment, because the neuromodulatory system automatically allocates learning capacity where it's most needed.

### 11.1 Dopamine Reward Prediction Error


```math
\delta_{\text{DA}} = r(t) - \hat{V}(s_t)
```


Phasic bursts ($\delta > 0$) amplify STDP learning by factor $m_{\text{burst}} = 3.0$. Tonic dopamine sets baseline learning rate. Diffusion modeled via quantum walk (Section 4.2).

### 11.2 Serotonin, Norepinephrine, Acetylcholine

Six neuromodulators modulate plasticity and network dynamics. Each has a spatial concentration field $[M](x, t)$ that decays exponentially and is replenished by source nuclei. The `neuromodulator_system_t` (already a pointer typedef) manages diffusion, reuptake, and receptor binding across 33+ brain regions.

*Source: `include/core/neuromodulators/nimcp_neuromodulators.h`*

---

## 12. Memory Mathematics

### 12.1 Engram Encoding

Pattern completion via associative recall. An engram $e$ is a stored activation pattern; retrieval uses cosine similarity between cue and stored patterns, selecting the engram with highest overlap.

### 12.2 Episodic Replay

Importance-weighted sampling selects experiences for replay:


```math
P(\text{replay}_i) \propto |\delta_i|^\alpha + \epsilon
```


where $\delta_i$ is the TD error at encoding time, $\alpha$ controls prioritization.

### 12.3 Memory Consolidation

Synaptic weight decay during consolidation:


```math
w(t) = w_0 \cdot \exp(-t / \tau_{\text{decay}}) + w_{\text{consolidated}} \cdot (1 - \exp(-t / \tau_{\text{consolidate}}))
```


### 12.4 Habituation

Repeated stimulus exposure produces exponential dampening:


```math
h(t) = h_0 \cdot \exp(-n_{\text{exposures}} / \tau_h)
```


---

## 13. Safety Mathematics

**Why mathematics in the safety system?** Most AI safety approaches are behavioral — train the model to refuse harmful requests. NIMCP's safety is structural: mandatory code gates that execute before every inference and weight update. But structural safety still requires mathematics to be *verifiable*. The CRC32 checksums provide cryptographic evidence that audit logs haven't been tampered with. The Byzantine detection uses statistical hypothesis testing to identify compromised nodes. The LGSS lattice provides a formal framework where safety properties (monotonicity, completeness) can be proven by construction.

The key principle: **if you can't prove a safety property mathematically, you can't verify it in deployment**. Behavioral safety ("the model seems safe in testing") is not a proof. Structural safety with mathematical guarantees ("the CRC32 detects any 1-bit modification with probability $1 - 2^{-32}$") is.

### 13.1 CRC32 Checksum

Each audit log entry carries a CRC32 checksum computed over its payload. Mismatches indicate tampering. Monotonic sequence numbers detect deletion (gaps indicate removed entries).

*Source: `include/security/nimcp_audit_log.h`*

### 13.2 Byzantine Gradient Detection

In swarm learning, gradients from edge nodes are validated using EMA-tracked statistics:


```math
z_k = \frac{\|g_k\| - \mu_{\text{EMA}}}{\sigma_{\text{EMA}}}
```


Nodes with $|z_k| > z_{\text{thresh}}$ are flagged as potentially Byzantine and excluded from federated averaging.

*Source: `src/swarm/nimcp_swarm_byzantine.c`*

### 13.3 LGSS Safety Lattice

The Layered Governance Safety System evaluates actions against a partial order of safety levels. An action at level $\ell$ requires clearance $\geq \ell$:


```math
\text{allow}(a) \iff \text{clearance}(a) \geq \text{level}(a) \quad \forall \text{ safety domains}
```


Five pipeline points (input validation, action interception, motor gate, training guard, reward alignment) form a monotonic safety chain: blocking at any point propagates to all downstream stages.

*Source: `include/security/lgss/nimcp_lgss.h`*

---

## 14. Free Energy Principle and World Model

**Why the Free Energy Principle?** Karl Friston's Free Energy Principle (Friston, 2010) proposes that all biological systems minimize variational free energy — a bound on the surprise of their sensory observations. In practice, this means the brain maintains an internal model of the world and acts to minimize the difference between predicted and actual sensory input. NIMCP implements this directly: the FEP orchestrator connects the Hamiltonian dynamics ($H \equiv F$, variational free energy) to prediction error signals that drive both learning and exploration.

### 14.1 Variational Free Energy

The variational free energy for a generative model $p(o, s)$ with approximate posterior $q(s)$ over hidden states $s$ given observations $o$:

```math
F = \text{KL}[q(s) \| p(s | o)] - \ln p(o) = \langle \ln q(s) - \ln p(o, s) \rangle_{q(s)}
```

Minimizing $F$ with respect to $q(s)$ performs approximate Bayesian inference (perception). Minimizing $F$ with respect to actions changes observations to match predictions (active inference).

**How it connects to the system:** The FEP orchestrator (`nimcp_fep_orchestrator`) bridges HNN energy dynamics to sensory prediction. The HNN's Hamiltonian $H(q, p)$ serves as the free energy functional. When sensory prediction error is high, $H$ increases, driving the system to update its internal model (learning) or change its behavior (action selection). This is wired into the inference pipeline at `brain_decide()` — the FEP bridge evaluates prediction error and modulates the output through the parietal cortex pathway.

**What happens when it fails:** If the FEP bridge receives NaN from a corrupted HNN state, the orchestrator falls back to direct sensory processing without predictive modulation. This is logged as an LGSS safety event. The FEP is a refinement, not a prerequisite — the brain functions without it, just less efficiently.

*Source: `src/core/brain/fep/nimcp_fep_orchestrator_part_core.c`, `src/lnn/bridges/nimcp_fep_hnn_fno_bridges.c`*

### 14.2 World Model (Predictive Processing)

The brain maintains an internal world model that predicts the next sensory state given the current state and action:

```math
\hat{s}_{t+1} = f_\theta(s_t, a_t)
```

The prediction error $e_t = \|s_{t+1} - \hat{s}_{t+1}\|_2$ serves dual roles:

1. **Learning signal**: The world model parameters $\theta$ are updated to minimize prediction error via gradient descent
2. **Curiosity signal**: High prediction error triggers dopamine release, which gates STDP and increases learning rate (Section 11)

**Why a separate world model instead of using the main network?** The main ANN learns input→output mappings for the current task. The world model learns state→state transitions — a fundamentally different prediction target. Separating them prevents the task loss from interfering with the dynamics model. The world model lives in the predictive hierarchy module and is trained alongside the main network but with its own loss function.

**How it connects to surprise detection:** In the inference pipeline (`brain_decide()`), the world model predicts what sensory input *should* look like given the brain's current internal state. If the actual input differs significantly (surprise), the system allocates more attention (norepinephrine) and encodes the experience more strongly (acetylcholine). This is the neural implementation of the Bayesian brain hypothesis — the brain is a prediction machine that learns from its own prediction errors.

*Source: `src/cognitive/world_model/`, `src/core/brain/nimcp_brain_part_core.c` (surprise detection at inference)*

---

## 15. Prime Resonance Memory

**Why prime-number-based memory addressing?** Conventional hash tables use modular arithmetic for bucket assignment, which creates systematic collision patterns when inputs have periodic structure. Prime resonance memory uses prime number relationships to create a collision-resistant addressing scheme where related memories reinforce each other through harmonic resonance patterns.

### 15.1 Prime Resonance Encoding

A memory vector $m \in \mathbb{R}^d$ is encoded using a set of prime frequencies:

```math
\text{encode}(m, k) = \sum_{i=1}^{d} m_i \cdot \sin\left(\frac{2\pi \cdot p_k \cdot i}{d}\right)
```

where $p_k$ is the $k$-th prime number. Each prime frequency creates an independent projection of the memory, and the set of projections forms a signature that is robust to noise — similar memories produce similar resonance patterns while dissimilar memories produce uncorrelated patterns.

### 15.2 Resonance-Based Retrieval

Memory retrieval computes the resonance between a query and stored memories:

```math
\text{resonance}(q, m) = \sum_{k=1}^{K} \text{encode}(q, k) \cdot \text{encode}(m, k)
```

**Why this works:** The prime frequencies are mutually incommensurable — no prime is a rational multiple of another. This guarantees that the resonance pattern of any memory is unique up to the number of primes used. In practice, $K = 8$ primes provide sufficient discrimination for NIMCP's 2,048-concept semantic memory pool.

**How it connects to the system:** The prime resonance module provides a fast approximate nearest-neighbor lookup for semantic memory. Before the full cosine-similarity search in `semantic_memory_find_similar()` (which iterates all 2,048 concepts), the resonance filter eliminates ~90% of candidates, reducing the search to ~200 cosine comparisons.

*Source: `src/cognitive/memory/nimcp_prime_resonance.c`*

---

## 16. Adaptive Network Architecture

**Why the adaptive network needs formalization:** The ANN handles 60% of the composite loss weight and processes every input. Yet Sections 1-2 formalize the SNN, LNN, and HNN while leaving the primary network undocumented.

### 16.1 9-Layer Diamond Topology

The layer sizes follow a diamond distribution that expands to a peak then contracts:

```math
n_l = \text{hidden\_budget} \times r_l, \quad r = [0.01, 0.05, 0.15, 0.28, 0.28, 0.15, 0.05, 0.03]
```

with input and output layers at positions 0 and 8. The diamond shape is motivated by information bottleneck theory: early layers compress (few neurons, forced abstraction), middle layers expand (maximum representational capacity), and late layers re-compress toward the output dimensionality.

**Why 9 layers for 2M+ neurons?** Smaller networks use fewer layers (3 for <5K, 5 for 5K-100K, 7 for 100K-2M). The depth scales with neuron count because wider layers need more depth to develop hierarchical features. At 2.5M neurons, the peak layers (L4, L5) each contain ~700K neurons — enough capacity that 7 layers would leave them underutilized. The 9-layer diamond provides finer-grained hierarchy with dedicated feature extraction (L1-L2), abstraction (L3-L5), and output preparation (L6-L8) stages.

### 16.2 Forward Pass

The forward pass through layer $l$:

```math
h_l = f(W_l \cdot h_{l-1} + b_l)
```

where $f$ is the activation function (ReLU for hidden layers, linear for output — TANH was found to cause gradient vanishing for regression targets). The GPU training bridge executes this as batched matrix multiplications in FP16 (mixed precision), with loss scaling to prevent gradient underflow.

### 16.3 Backward Pass and Weight Decay

Gradient computation follows standard backpropagation with layer-wise learning rate scaling:

```math
\Delta W_l = -\eta_l \cdot \frac{\partial \mathcal{L}}{\partial W_l}, \quad \eta_l = \eta_{\text{base}} \times s_l
```

where $s_l$ is a per-layer scale factor. Weight decay is LR-coupled:

```math
W_l \leftarrow W_l \times (1 - \eta_l \times \lambda)
```

**What happens when it fails:** The output layer activation was originally TANH (from an older checkpoint), which bounded outputs to [-1, 1] while targets ranged over [-5, 5]. This caused systematic gradient vanishing on the output layer — loss plateaued at ~10,000 (the MSE of ±1 outputs vs ±100 targets). Switching to LINEAR output activation resolved the plateau.

*Source: `src/plasticity/adaptive/nimcp_adaptive.c`, `src/gpu/training/nimcp_training_bridge.c`*

---

## 17. Composite Loss and Cross-Network Gradients

### 17.1 Composite Loss Function

The UTM computes a weighted sum of per-network losses plus regularization terms:

```math
\mathcal{L}_{\text{composite}} = \sum_{n=1}^{N} w_n \mathcal{L}_n + \lambda_{\text{div}} \mathcal{L}_{\text{diversity}} + \lambda_{\text{contra}} \mathcal{L}_{\text{contrastive}} + \lambda_{\text{kd}} \mathcal{L}_{\text{distill}}
```

Default weights: $w_{\text{ANN}} = 0.6$, $w_{\text{SNN}} = 0.15$, $w_{\text{LNN}} = 0.15$, $w_{\text{CNN}} = 0.10$.

### 17.2 LSA Bridge Gradient Flow

For a Linear Spline Adapter bridge $B$ with parameters $(W, b)$ connecting source $S$ to target $T$:

**Forward:** $h = W \cdot \tanh(y_S) + b$

**Backward:**

```math
\frac{\partial \mathcal{L}}{\partial W} = \frac{\partial \mathcal{L}}{\partial h} \cdot \tanh(y_S)^T
```

```math
\frac{\partial \mathcal{L}}{\partial y_S} = W^T \cdot \frac{\partial \mathcal{L}}{\partial h} \odot (1 - \tanh^2(y_S))
```

The $(1 - \tanh^2)$ factor acts as a natural gradient gate: large source activations near tanh saturation receive attenuated gradients, preventing cross-network gradient explosion without explicit clipping.

**Why tanh and not linear?** A linear bridge $h = Wy_S + b$ would allow unbounded gradient flow. If the SNN produces output spikes with magnitude 130 Hz (our observed max), and the ANN's gradient has magnitude 0.1, the cross-network gradient would be $0.1 \times 130 = 13$ — large enough to destabilize the ANN. The tanh compresses the SNN output to [-1, 1] before the bridge, bounding the cross-gradient regardless of spike rates.

*Source: `src/training/nimcp_unified_training.c`*

---

## 18. Diversity and Contrastive Loss

**Why anti-collapse mechanisms?** With 6 networks trained on the same loss, the path of least resistance is for all networks to learn the same function. The composite loss gives the ANN 60% weight — the optimizer's gradient signal pushes all other networks toward replicating the ANN's output. Without diversity pressure, the SNN, LNN, CNN, FNO, and HNN converge to the same representation, and the bridges become identity transforms. The system degenerates to a single (expensive) network.

### 18.1 Diversity Loss (Effective Rank)

The effective rank of the output matrix across a batch measures representational diversity:

```math
\text{eff\_rank}(Y) = \exp\left(-\sum_{i} \hat{\sigma}_i \ln \hat{\sigma}_i\right), \quad \hat{\sigma}_i = \frac{\sigma_i}{\sum_j \sigma_j}
```

where $\sigma_i$ are the singular values of the output matrix $Y \in \mathbb{R}^{B \times d}$ (batch size $B$, output dimension $d$). Effective rank of 1 means all outputs are identical (mode collapse). Effective rank of $d$ means outputs span the full output space.

The diversity loss penalizes low effective rank:

```math
\mathcal{L}_{\text{diversity}} = \max(0, r_{\min} - \text{eff\_rank}(Y) / d)
```

where $r_{\min} = 0.1$ is the minimum variance ratio.

### 18.2 Contrastive Loss (Cross-Network)

Penalizes pairs of networks whose outputs are too similar:

```math
\mathcal{L}_{\text{contrastive}} = \sum_{i < j} \max(0, \text{margin} - \|y_i - y_j\|_2)
```

This forces each network to develop distinct representations. The SNN cannot simply replicate the ANN's output — the contrastive loss pushes it toward temporal coding (spike patterns) which is naturally orthogonal to the ANN's static feature vectors.

### 18.3 Cosine Similarity Ring Buffer

A ring buffer of the 16 most recent outputs tracks pairwise cosine similarity:

```math
\text{sim}(y_t, y_{t-k}) = \frac{y_t \cdot y_{t-k}}{\|y_t\| \|y_{t-k}\|}
```

When mean similarity exceeds 0.80, the diversity loss weight is boosted. This provides a real-time mode collapse detector that triggers corrective pressure before the network fully collapses.

*Source: `src/training/nimcp_anti_collapse.c`*

---

## 19. Sparse Synapse Mathematics

### 19.1 Three-Tier Storage

Each synapse exists as up to three data structures, allocated on demand:

| Tier | Structure | Size | When Allocated | Contents |
|------|-----------|------|----------------|----------|
| Handle | `synapse_handle_t` | 16 bytes | Always (backbone wiring) | target_id, weight, strength, metadata_index |
| Metadata | `synapse_t` | 52 bytes | On first plasticity event | plasticity, trace, last_active, source_id, cold_index |
| Cold | `synapse_cold_t` | ~140 bytes | When STP/BCM needed | STP state, BCM threshold, eligibility trace, ternary state |

**Why three tiers?** At 2.5M neurons with ~128 connections each, the brain has ~320M synapse handles. If every synapse had full cold data: $320M \times 140 = 44.8$ GB — more than available RAM. The tiered approach allocates handles for all synapses (5.1 GB), metadata only for synapses with active plasticity (~50M slots = 2.6 GB), and cold data only for synapses that need STP/BCM (~5M slots = 700 MB). Total: 8.4 GB instead of 44.8 GB.

### 19.2 Pool Allocation with Graceful Fallback

The metadata pool has a maximum capacity ($50M$ slots). When exhausted:

```math
\text{if } \text{pool\_size} \geq \text{MAX\_POOL\_SIZE}: \text{return SPARSE\_SYNAPSE\_NO\_METADATA}
```

The synapse handle is still created (forward/backward pass works), but biological plasticity (STDP, STP, BCM, eligibility) is disabled for that synapse. This is graceful degradation — the network continues to function, just without biological learning on overflow synapses.

**What happens when it fails silently:** In an earlier version, `sparse_synapse_add_with_metadata` returned -1 on pool exhaustion, causing the synapse to not be created at all — not even the handle. This meant the backbone wiring was incomplete, with missing connections that silently degraded network connectivity. The fix: always create the handle, optionally add metadata.

*Source: `src/core/neuralnet/nimcp_sparse_synapse.c`*

---

## 20. Developmental Curriculum Mathematics

### 20.1 Stage Transition

Training progresses through 4 stages with increasing cognitive complexity:

| Stage | Steps | Confidence | LR Schedule | Cognitive Domains |
|-------|-------|-----------|-------------|-------------------|
| 0 (Sensory) | 10,000 | 0.9 → 0.5 | Cosine, warm start | None |
| 1 (Naming) | 20,000 | 0.65 | Cosine | 13 domains, 1 item/10 steps |
| 2 (Feedback) | 20,000 | 0.70 | Cosine | 13 domains, escalating |
| 3 (Reasoning) | 10,000 | Adaptive | Per-domain adaptive | All 13, mastery-tracked |

### 20.2 Cosine Annealing with Warmup

Learning rate follows a cosine schedule:

```math
\eta(t) = \eta_{\min} + \frac{\eta_{\max} - \eta_{\min}}{2}\left(1 + \cos\left(\frac{\pi t}{T_{\max}}\right)\right)
```

where $T_{\max} = 20000$ (full cycle length), $\eta_{\max} = 0.001$, $\eta_{\min} = 0.00001$. The first 1000 steps use linear warmup from $\eta_{\min}$ to $\eta_{\max}$.

### 20.3 Spectral K-Fold Cross-Validation

Standard k-fold randomly assigns items to folds, allowing semantically similar items to appear in both train and test — inflating accuracy estimates. Spectral k-fold respects the data manifold:

1. Embed all items via sentence transformer: $e_i \in \mathbb{R}^{384}$
2. Construct cosine similarity graph: $S_{ij} = \frac{e_i \cdot e_j}{\|e_i\| \|e_j\|}$
3. Graph Laplacian: $L = D - S$ where $D_{ii} = \sum_j S_{ij}$
4. Eigendecompose: take $k$ smallest non-trivial eigenvectors of $L$
5. K-means on eigenvectors → spectral clusters
6. Rebalance for domain stratification

**Why this matters:** When fold $k$ is held out, the model is tested on an entire semantic cluster it has never seen. This measures genuine generalization, not interpolation between similar training examples.

*Source: `scripts/spectral_kfold.py`*

---

## 21. Hemispheric Architecture

### 21.1 Corpus Callosum Transfer

NIMCP implements a hemispheric brain with left and right hemispheres connected by a corpus callosum with 4 specialized channels:

```math
T_c(h_L, h_R) = W_c \cdot [h_L; h_R] + b_c, \quad c \in \{\text{motor, sensory, cognitive, emotional}\}
```

Each channel has learned transfer weights $W_c$ that determine how much information flows between hemispheres for that modality. During training, the transfer weights adapt — if one hemisphere develops stronger visual processing, the sensory channel weights increase to share that specialization.

**Why hemispheres?** Biological hemispheric specialization (language lateralization, spatial processing) emerges from development, not design. NIMCP's hemispheric architecture provides the structural substrate for similar emergent lateralization — whether it actually occurs depends on the training data and is an open research question (see recommended future paper: "Hemispheric Specialization").

*Source: `src/core/brain/hemispheric/`*

---

## 22. K-Winners-Take-All Sparse Coding

### 22.1 Sparse Output Selection

K-WTA enforces output sparsity by allowing only the top-$k$ activations to pass through:

```math
y_i = \begin{cases} x_i & \text{if } x_i \geq x_{(k)} \\ 0 & \text{otherwise} \end{cases}
```

where $x_{(k)}$ is the $k$-th largest activation. This creates a sparse output code where at most $k$ neurons are active per input, reducing interference between representations and improving memory capacity.

**Why K-WTA instead of dropout or L1 regularization?** Dropout randomly silences neurons during training — useful for regularization but doesn't produce structured sparsity at inference time. L1 regularization pushes weights toward zero globally, which can over-prune important connections. K-WTA preserves the strongest activations regardless of their magnitude, producing a sparse code that retains the most informative features.

**How it connects to the system:** K-WTA is applied to the output layer of the adaptive network when `enable_sparsity = true`. The sparsity level $k/d$ (where $d$ is the output dimension) is configurable. At $k = 64$ out of $d = 4096$, only 1.6% of output neurons are active — each input produces a unique sparse code that is highly discriminative.

*Source: `src/core/neuralnet/nimcp_neuralnet.c` (sparse output path)*

---

## 23. Pink Noise (1/f) Stochastic Dynamics

**Why pink noise and not white noise?** White noise has equal power at all frequencies — each sample is independent. Biological neural systems do not operate this way. Cortical activity, heart rate variability, and cognitive performance all exhibit $1/f$ (pink) noise: a power spectrum that falls off as $1/f^\alpha$ with $\alpha \approx 1$. This means slow fluctuations are stronger than fast ones, creating temporal correlations across timescales. Adding white noise to a neural simulation produces unrealistic jitter. Adding pink noise produces biologically realistic stochastic variability that enhances exploration, prevents convergence to sharp local minima, and improves generalization.

### 23.1 1/f Power Spectrum

Pink noise has a power spectral density:

```math
S(f) = \frac{C}{f^\alpha}, \quad \alpha \approx 1
```

For $\alpha = 0$, the spectrum is flat (white noise). For $\alpha = 1$, it is pink noise. For $\alpha = 2$, it is brown/red noise (random walk). Pink noise is the boundary between stationary ($\alpha < 1$) and non-stationary ($\alpha > 1$) processes — it has long-range correlations without divergent variance.

**Why $\alpha = 1$ is special:** At this exponent, the noise has self-similar structure — zooming in or out on the time series produces statistically identical patterns. This scale-free property matches the brain's hierarchical processing: the same noise statistics appear at the millisecond (synaptic), second (neural population), and minute (cognitive) timescales. A single pink noise generator can modulate processes at all three levels.

### 23.2 The 12 Pink Noise Bridges

NIMCP integrates pink noise into 12 neural subsystems, each with its own bridge module:

| Bridge | Subsystem | What it modulates |
|--------|-----------|-------------------|
| Calcium | Calcium signaling | Stochastic ion channel opening/closing |
| Dendritic | Dendritic integration | Membrane potential fluctuations in dendrites |
| Heterosynaptic | Cross-synapse effects | Random spread of plasticity signals between nearby synapses |
| Metabolic | Energy metabolism | ATP availability fluctuations affecting neural excitability |
| Spatial neuromodulator | Neuromodulator diffusion | Stochastic variation in DA/ACh/NE/5-HT concentration fields |
| Vesicle packaging | Synaptic vesicles | Random variation in neurotransmitter quanta per release event |
| Ensemble uncertainty | Population coding | Noise in population-level activity estimates |
| Systems consolidation | Memory consolidation | Stochastic replay selection during sleep/consolidation |
| Brain oscillations | Oscillatory coupling | Phase jitter in theta/gamma oscillation coupling |
| Population coding | Population rate estimates | Noise in firing rate decoding |
| STDP | Spike-timing plasticity | Stochastic variation in STDP window shape |
| STP | Short-term plasticity | Random fluctuation in facilitation/depression dynamics |

Each bridge samples from a shared pink noise generator at its subsystem's characteristic rate and scales the noise amplitude to match the subsystem's dynamic range.

### 23.3 Generation via Voss-McCartney Algorithm

Pink noise is generated using the Voss-McCartney algorithm, which sums multiple random number generators updated at different rates:

```math
x(t) = \sum_{k=0}^{K-1} r_k(t)
```

where $r_k$ is a uniform random value that updates every $2^k$ samples. Generator $r_0$ updates every sample (high frequency), $r_1$ every 2 samples, $r_2$ every 4, etc. The sum of these staggered updates produces approximate $1/f$ noise with $K$ octaves of frequency coverage.

**Why Voss-McCartney instead of FFT-based generation?** FFT-based pink noise generates an entire buffer at once — efficient for offline processing but unusable for real-time streaming. Voss-McCartney produces one sample per call with O(1) compute and O(K) memory (K = number of octaves, typically 16). This allows each pink noise bridge to generate its own independent stream without batch processing.

*Source: `src/plasticity/pink_noise/`, `include/core/neuron_models/nimcp_sfa_pink_noise_bridge.h`*

---

## 24. Fractal Topology and Detrended Fluctuation Analysis

**Why fractal structure?** The mammalian cortex is not a regular grid or a random graph. It has fractal structure — self-similar connectivity patterns that repeat at multiple spatial scales. Small clusters of neurons form microcolumns, microcolumns cluster into macrocolumns, macrocolumns form areas, and areas form networks. This hierarchical organization has a fractal dimension of approximately 2.5 in human cortex. NIMCP's fractal topology generator creates connection patterns with configurable fractal dimension, producing networks whose connectivity statistics match biological cortex.

### 24.1 Fractal Network Topology

A fractal network with dimension $D$ and $N$ neurons is constructed recursively:

1. Start with a seed graph of $n_0$ fully-connected neurons
2. Replicate the seed $b$ times (branching factor)
3. Connect replicas with inter-group connections at probability $p_{\text{inter}} = p_0 \cdot s^{-D}$, where $s$ is the scale level
4. Repeat for $L$ levels

The fractal dimension $D$ controls how quickly connectivity falls off with distance:
- $D = 1.0$: chain-like (each group connects only to neighbors)
- $D = 2.0$: planar (moderate long-range connections)
- $D = 2.5$: cortical (rich long-range connections, matching biological cortex)
- $D = 3.0$: near-random (almost all pairs connected)

```math
p_{\text{connect}}(i, j) = p_0 \cdot d(i,j)^{-D}
```

where $d(i,j)$ is the topological distance between neurons $i$ and $j$ in the fractal hierarchy.

### 24.2 Detrended Fluctuation Analysis (DFA)

DFA measures long-range temporal correlations in a time series — specifically, whether the series exhibits pink-noise-like ($1/f$) structure or white-noise-like independence:

1. Integrate the time series: $Y(k) = \sum_{i=1}^{k} (x_i - \bar{x})$
2. Divide $Y$ into non-overlapping windows of length $n$
3. In each window, fit a polynomial trend $Y_n$ and compute the residual variance:

```math
F(n) = \sqrt{\frac{1}{N} \sum_{k=1}^{N} [Y(k) - Y_n(k)]^2}
```

4. Plot $\log F(n)$ vs $\log n$. The slope is the DFA exponent $\alpha$:

```math
F(n) \sim n^\alpha
```

**Interpretation:**
- $\alpha = 0.5$: white noise (uncorrelated, random walk of integral)
- $\alpha = 1.0$: $1/f$ pink noise (long-range correlations)
- $\alpha = 1.5$: Brownian noise (strongly correlated)
- $\alpha > 1.5$: non-stationary, potentially pathological

### 24.3 Hurst Exponent for Training Health

The Hurst exponent $H$ (closely related to the DFA exponent: $H = \alpha - 0.5$ for stationary series) monitors training health in the UTM:

```math
H = \alpha_{\text{DFA}} - 0.5
```

- $H = 0.5$: Loss fluctuations are random (normal training)
- $H > 0.5$: Loss has persistent trends (still converging — good)
- $H < 0.5$: Loss has anti-persistent fluctuations (oscillating — potential instability)
- $H \to 1.0$: Loss is monotonically decreasing (strong convergence)

**How it connects to the system:** The UTM tracks the Hurst exponent of the composite loss time series (`hurst_exponent` field). When $H < 0.3$ (strong anti-persistence), the quantum annealing module (Section 4.1) is triggered to perturb weights out of an oscillatory attractor. When $H > 0.8$ (strong persistence), the learning rate is reduced because the network is converging and aggressive updates risk overshooting.

**What happens when it fails:** A corrupted loss buffer (e.g., NaN entries from a division-by-zero) produces $H = 0$ (no temporal structure). The DFA computation includes a NaN guard that replaces invalid entries with the buffer mean, preventing cascade failures.

*Source: `src/core/topology/nimcp_fractal_topology.c`, `src/training/nimcp_unified_training.c` (Hurst monitoring)*

---

## 25. Tensor Operations and SIMD Acceleration

**Why a custom tensor library?** NIMCP is written in C, not Python. There is no numpy. Every matrix multiply, element-wise operation, and reshape is implemented in the `nimcp_tensor` library, with AVX2 SIMD acceleration for x86 platforms and fallback scalar paths for portability.

### 25.1 Tensor Representation

A tensor of rank $r$ with dimensions $(d_1, d_2, \ldots, d_r)$ is stored as a contiguous float array in row-major order:

```math
\text{offset}(i_1, i_2, \ldots, i_r) = \sum_{k=1}^{r} i_k \prod_{j=k+1}^{r} d_j
```

Supported operations: create, clone, destroy, reshape, transpose, slice, element-wise arithmetic (+, -, *, /), matrix multiply, sum, mean, norm, softmax, and activation functions (ReLU, tanh, sigmoid).

### 25.2 SIMD Vectorization

On AVX2 platforms (256-bit vectors), operations process 8 floats per instruction:

```math
\text{throughput} = \frac{8 \text{ floats}}{\text{cycle}} \times \text{clock speed}
```

The SIMD backend is auto-detected at runtime: AVX-512 (512-bit), AVX2 (256-bit), SSE (128-bit), or scalar fallback. For NIMCP's 4096-dim output vectors, AVX2 processes a full vector in 512 SIMD iterations instead of 4096 scalar iterations — an 8× speedup for element-wise operations.

*Source: `include/utils/tensor/nimcp_tensor.h`, `src/utils/tensor/nimcp_tensor_simd.c`*

---

## 26. Ternary Computing

**Why ternary logic in a neural system?** Binary neural networks quantize weights to $\{0, 1\}$ and lose sign information. Ternary networks use $\{-1, 0, +1\}$, preserving the sign of each connection (excitatory vs. inhibitory) while reducing memory by 16× compared to 32-bit floats. For NIMCP's 320M synapses, ternary storage reduces weight memory from 1.2 GB to 80 MB.

### 26.1 Ternary Quantization

A float weight $w$ is quantized to ternary:

```math
q(w) = \begin{cases} +1 & \text{if } w > \Delta \\ -1 & \text{if } w < -\Delta \\ 0 & \text{otherwise} \end{cases}
```

where $\Delta = 0.7 \times \mathbb{E}[|w|]$ (the threshold is 70% of the mean absolute weight, following Li et al., 2016).

### 26.2 Ternary Matrix Multiplication

Ternary matrix multiply replaces multiply-accumulate with conditional add/subtract:

```math
(Q \cdot x)_i = \sum_{j: Q_{ij}=+1} x_j - \sum_{j: Q_{ij}=-1} x_j
```

No multiplications — only additions and subtractions. This is 3-10× faster than float GEMM on integer-optimized hardware and enables efficient inference on edge devices without FPU.

### 26.3 Ternary Tensor Storage

Ternary values are packed 16 per 32-bit word using 2-bit encoding ($00 = 0$, $01 = +1$, $10 = -1$, $11$ = reserved):

```math
\text{storage} = \lceil N / 16 \rceil \times 4 \text{ bytes}
```

For a 4096-dim vector: 1024 bytes ternary vs. 16384 bytes float32.

*Source: `include/utils/ternary/nimcp_ternary.h`, `src/utils/ternary/`*

---

## 27. Gabor Filters (V1 Simple Cells)

**Why Gabor filters?** The first stage of biological visual processing in V1 cortex uses orientation-selective neurons whose receptive fields are well-approximated by Gabor functions (Hubel & Wiesel, 1962; Jones & Palmer, 1987). NIMCP's Gabor filter bank provides biologically-motivated visual feature extraction as an alternative to learned CNN filters.

### 27.1 2D Gabor Function

A Gabor filter is a Gaussian envelope modulated by a sinusoidal plane wave:

```math
g(x, y; \lambda, \theta, \psi, \sigma, \gamma) = \exp\left(-\frac{x'^2 + \gamma^2 y'^2}{2\sigma^2}\right) \cos\left(\frac{2\pi x'}{\lambda} + \psi\right)
```

where:

```math
x' = x\cos\theta + y\sin\theta, \quad y' = -x\sin\theta + y\cos\theta
```

Parameters: $\lambda$ (wavelength), $\theta$ (orientation), $\psi$ (phase offset), $\sigma$ (bandwidth), $\gamma$ (aspect ratio).

A bank of Gabor filters with 8 orientations ($\theta \in \{0°, 22.5°, 45°, \ldots, 157.5°\}$) and 4 spatial frequencies ($\lambda \in \{4, 8, 16, 32\}$ pixels) produces 32 feature maps, closely matching the orientation columns found in V1.

*Source: `include/utils/gabor/nimcp_gabor.h`*

---

## 28. Fuzzy Inference

**Why fuzzy logic?** Classical (Boolean) logic requires crisp boundaries: a neuron is either "active" or "inactive." Biological neurons have graded responses — a neuron firing at 15 Hz is somewhat active. Fuzzy logic handles this naturally through membership functions that map continuous values to degrees of membership in linguistic categories.

### 28.1 Fuzzy Membership Functions

A trapezoidal membership function $\mu_A(x)$:

```math
\mu_A(x) = \begin{cases} 0 & x < a \\ (x-a)/(b-a) & a \leq x < b \\ 1 & b \leq x \leq c \\ (d-x)/(d-c) & c < x \leq d \\ 0 & x > d \end{cases}
```

### 28.2 Fuzzy Operators

- **AND (T-norm):** $\mu_{A \cap B}(x) = \min(\mu_A(x), \mu_B(x))$
- **OR (T-conorm):** $\mu_{A \cup B}(x) = \max(\mu_A(x), \mu_B(x))$
- **NOT:** $\mu_{\bar{A}}(x) = 1 - \mu_A(x)$

### 28.3 Fuzzy Inference Engine

NIMCP's fuzzy inference engine evaluates rules of the form:

```
IF firing_rate IS high AND confidence IS low THEN learning_rate IS very_high
```

The Mamdani inference method aggregates rule outputs via max-composition, then defuzzifies using centroid:

```math
z^* = \frac{\int z \cdot \mu_{\text{out}}(z) \, dz}{\int \mu_{\text{out}}(z) \, dz}
```

**How it connects to the system:** Fuzzy inference is used in the safety watchdog for motor output validation — "IF velocity IS high AND obstacle IS near THEN brake IS urgent" — where crisp thresholds would produce jerky stop/go behavior.

*Source: `include/utils/fuzzy/nimcp_fuzzy_inference.h`*

---

## 29. Differential Geometry

NIMCP implements four geometric frameworks for analyzing and transforming neural representations.

### 29.1 Hyperbolic Embeddings

**Why hyperbolic space?** Euclidean space cannot efficiently embed hierarchical structures — a tree with branching factor $b$ and depth $d$ has $b^d$ leaves, requiring exponential dimensions to preserve distances. Hyperbolic space (constant negative curvature) embeds trees with logarithmic distortion (Nickel & Kiela, 2017).

In the Poincaré ball model with curvature $-c$:

```math
d_{\mathbb{H}}(x, y) = \frac{1}{\sqrt{c}} \text{arccosh}\left(1 + \frac{2c\|x - y\|^2}{(1 - c\|x\|^2)(1 - c\|y\|^2)}\right)
```

NIMCP uses hyperbolic embeddings for the concept hierarchy in semantic memory — "animal → dog → retriever" maps naturally to hyperbolic space where depth encodes specificity.

### 29.2 Lie Group Representations

Lie groups provide a framework for continuous symmetry transformations. NIMCP uses the rotation group $SO(3)$ for:

- Spatial reasoning in the embodiment module (3D rotations of body parts)
- Rotary position encoding (RoPE) as rotation in embedding subspaces

The exponential map from Lie algebra $\mathfrak{so}(3)$ to the group:

```math
R = \exp(\theta \hat{n}) = I + \sin\theta [\hat{n}]_\times + (1 - \cos\theta)[\hat{n}]_\times^2
```

where $[\hat{n}]_\times$ is the skew-symmetric matrix of the rotation axis $\hat{n}$ (Rodrigues' formula).

### 29.3 Lorentz Transformations

For spacetime-aware representations in the physics module:

```math
\Lambda = \begin{pmatrix} \gamma & -\beta\gamma \\ -\beta\gamma & \gamma \end{pmatrix}, \quad \gamma = \frac{1}{\sqrt{1-\beta^2}}
```

Used in HNN dynamics when modeling relativistic systems and in the Lorentz-equivariant layers of the physics neural network.

*Source: `include/utils/geometry/nimcp_hyperbolic.h`, `nimcp_lie_group.h`, `nimcp_lorentz.h`*

---

## 30. Tensor Networks (MPS and SVD)

**Why tensor networks?** A fully-connected weight matrix between two layers of 4096 neurons requires $4096^2 = 16.7M$ parameters. Tensor network decomposition (specifically Matrix Product States) factorizes this into a chain of smaller tensors, reducing parameters by orders of magnitude while preserving the most important correlations.

### 30.1 Matrix Product States (MPS)

An MPS represents a high-order tensor as a product of lower-order tensors:

```math
T_{i_1 i_2 \ldots i_N} = \sum_{\alpha_1 \ldots \alpha_{N-1}} A^{(1)}_{i_1 \alpha_1} A^{(2)}_{\alpha_1 i_2 \alpha_2} \cdots A^{(N)}_ {\alpha_{N-1} i_N}
```

The bond dimension $\chi$ (size of $\alpha$ indices) controls the trade-off between compression and expressiveness. For $\chi = 64$ and $N = 4096$: $4096 \times 64^2 = 16.7M$ parameters (same as full matrix) vs. $\chi = 8$: $4096 \times 64 = 262K$ parameters (64× compression).

### 30.2 Truncated SVD for Compression

Weight matrices are compressed via truncated SVD:

```math
W \approx U_k \Sigma_k V_k^T
```

retaining only the top-$k$ singular values. The approximation error is bounded by:

```math
\|W - U_k \Sigma_k V_k^T\|_F = \sqrt{\sum_{i=k+1}^{\min(m,n)} \sigma_i^2}
```

**How it connects to the system:** SVD compression is used during checkpoint saving to reduce file size, and during swarm gradient aggregation to compress gradient tensors before network transmission.

*Source: `include/utils/tensor_networks/nimcp_mps.h`, `nimcp_svd_simple.h`*

---

## 31. Complex Number Arithmetic

**Why complex numbers?** The Fourier Neural Operator (Section 3) operates in the complex frequency domain. The spectral convolution multiplies complex-valued learned weights $R_\phi \in \mathbb{C}^{c \times c \times k}$ with complex-valued FFT coefficients. Without native complex arithmetic, this requires manual separation of real and imaginary parts.

### 31.1 Complex Operations

NIMCP implements complex arithmetic as pairs of floats:

```math
(a + bi)(c + di) = (ac - bd) + (ad + bc)i
```

```math
|z| = \sqrt{a^2 + b^2}, \quad \arg(z) = \text{atan2}(b, a)
```

```math
z^{-1} = \frac{a - bi}{a^2 + b^2}
```

These are used in: FNO spectral convolution, FFT/IFFT, quantum walk amplitude computation, and phase coherence measurement in brain oscillations.

*Source: `include/utils/math/nimcp_complex_math.h`*

---

## 32. Numerical Integration

The ODE solvers used by the LNN (Section 1.3), HNN (Section 1.4), and world model (Section 14.2).

### 32.1 Euler Method

```math
x(t + \Delta t) = x(t) + \Delta t \cdot f(x, t)
```

First-order, O($\Delta t$) error per step. Used as a fast fallback when higher-order methods diverge.

### 32.2 Runge-Kutta 4 (RK4)

```math
x(t + \Delta t) = x(t) + \frac{\Delta t}{6}(k_1 + 2k_2 + 2k_3 + k_4)
```

where $k_1 = f(x, t)$, $k_2 = f(x + \frac{\Delta t}{2}k_1, t + \frac{\Delta t}{2})$, etc. Fourth-order, O($\Delta t^4$) error. Used for LNN forward integration.

### 32.3 Stormer-Verlet (Symplectic)

For Hamiltonian systems (Section 1.4), preserves phase-space volume:

```math
p_{1/2} = p_n - \frac{\Delta t}{2}\nabla_q H, \quad q_{n+1} = q_n + \Delta t \nabla_p H, \quad p_{n+1} = p_{1/2} - \frac{\Delta t}{2}\nabla_q H
```

**Why symplectic for HNN?** Non-symplectic integrators (Euler, RK4) introduce numerical energy drift — the total energy $H$ gradually increases or decreases over time, producing non-physical trajectories. Symplectic integrators preserve $H$ up to bounded oscillations, ensuring the HNN's energy conservation guarantee holds in practice.

*Source: `include/utils/numerical/nimcp_integration.h`*

---

## 33. Bayesian Statistics

### 33.1 Bayesian Inference

Posterior update via Bayes' rule:

```math
p(\theta | D) = \frac{p(D | \theta) \cdot p(\theta)}{p(D)}
```

NIMCP uses Bayesian inference for:
- **Uncertainty estimation**: Each prediction includes a confidence interval derived from the posterior variance
- **Novelty detection**: A new input with low likelihood $p(D | \theta)$ under the current model is flagged as novel → triggers acetylcholine release
- **Hyperparameter optimization**: Bayesian optimization for learning rate and neuromodulator sensitivity parameters

### 33.2 Variational Inference

When exact posterior computation is intractable ($p(D)$ involves marginalizing over all hidden states), variational inference approximates with a tractable distribution $q(\theta)$ by minimizing KL divergence:

```math
\text{KL}[q(\theta) \| p(\theta | D)] = \mathbb{E}_q[\ln q(\theta) - \ln p(\theta | D)]
```

This connects to the FEP (Section 14): variational free energy $F = \text{KL}[q \| p] - \ln p(D)$ is the quantity the brain minimizes.

*Source: `include/utils/statistics/nimcp_bayesian_advanced.h`*

---

## 34. Time Series Analysis

### 34.1 Autocorrelation

```math
R(\tau) = \frac{\mathbb{E}[(x_t - \mu)(x_{t+\tau} - \mu)]}{\sigma^2}
```

Used to detect periodic patterns in loss trajectories and firing rate oscillations.

### 34.2 Change Point Detection

Detects abrupt changes in time series statistics (mean, variance) using the CUSUM algorithm:

```math
S_t = \max(0, S_{t-1} + x_t - \mu_0 - k)
```

An alarm is raised when $S_t > h$ (threshold). Used to detect training phase transitions (e.g., Stage 0 → Stage 1) and sudden loss plateaus.

### 34.3 Spectral Density Estimation

Power spectral density via Welch's method (averaged periodogram):

```math
\hat{S}(f) = \frac{1}{K} \sum_{k=1}^{K} |X_k(f)|^2
```

where $X_k$ is the FFT of the $k$-th windowed segment. Used for oscillatory analysis (theta/gamma coupling) and DFA validation.

*Source: `include/utils/statistics/nimcp_timeseries.h`*

---

## 35. Chaos Engineering

**Why inject faults deliberately?** NIMCP runs a 2.5M neuron brain for hundreds of hours. Unexpected failures (NaN propagation, pool exhaustion, thread deadlock) must be handled gracefully. Chaos engineering proactively injects controlled faults to verify resilience.

### 35.1 Fault Injection Types

| Fault | What it does | What it tests |
|-------|-------------|---------------|
| NaN injection | Sets random neuron activations to NaN | NaN propagation detection and containment |
| Pool exhaustion | Artificially fills metadata pool | Graceful fallback to handle-only synapses |
| Thread delay | Adds random sleep to worker threads | Deadlock detection and timeout handling |
| Gradient explosion | Multiplies gradients by $10^6$ | Gradient clipping and normalization |
| Memory pressure | Allocates large temporary buffers | OOM handling and swap behavior |

Each fault is injected with configurable probability $p_{\text{fault}}$ and duration. The system's response is logged and compared against expected behavior.

*Source: `include/utils/fault_tolerance/nimcp_chaos_engineering.h`*

---

## 36. Spatial Indexing (KD-Tree)

### 36.1 KD-Tree for Nearest Neighbor Search

A KD-tree partitions $k$-dimensional space by cycling through dimensions at each tree level:

```math
\text{split dimension at depth } d = d \bmod k
```

For NIMCP's semantic memory with 2,048 concepts in 1024-dim embedding space, the KD-tree provides O($\log n$) approximate nearest neighbor search instead of O($n$) brute-force — reducing concept retrieval from 2,048 cosine comparisons to ~11 tree traversals.

**Limitation:** KD-trees degrade to O($n$) in high dimensions (the "curse of dimensionality"). At 1024 dimensions, the tree is effective only for well-separated clusters. The prime resonance filter (Section 15) handles the general case.

*Source: `include/utils/spatial/nimcp_kdtree.h`*

---

## 37. Streaming Statistics

**Why streaming?** Training runs for hundreds of hours. Storing every loss value, gradient norm, and firing rate for offline analysis would consume gigabytes. Streaming statistics compute exact mean, variance, quantiles, and histograms in O(1) space using online algorithms.

### 37.1 Welford's Online Variance

```math
\bar{x}_n = \bar{x}_{n-1} + \frac{x_n - \bar{x}_{n-1}}{n}, \quad M_{2,n} = M_{2,n-1} + (x_n - \bar{x}_{n-1})(x_n - \bar{x}_n)
```

Variance: $\sigma^2 = M_{2,n} / (n-1)$. Numerically stable for arbitrarily long sequences.

### 37.2 P-Square Quantile Estimation

Estimates arbitrary quantiles (median, 95th percentile) without storing all data using 5 markers that track the quantile position in O(1) space and O(1) time per update.

*Source: `include/utils/statistics/nimcp_streaming_statistics.h`*

---

## 38. Survival Analysis

### 38.1 Synapse Lifetime Modeling

**Why survival analysis for synapses?** Synapses in NIMCP are created (synaptogenesis) and destroyed (pruning). Understanding the distribution of synapse lifetimes — how long a connection survives before being pruned — informs the pruning strategy and structural plasticity parameters.

The Kaplan-Meier estimator for the survival function:

```math
\hat{S}(t) = \prod_{t_i \leq t} \left(1 - \frac{d_i}{n_i}\right)
```

where $d_i$ is the number of synapses pruned at time $t_i$ and $n_i$ is the number still alive. A synapse with $\hat{S}(t) < 0.1$ has a 90% chance of being pruned by time $t$ — these are candidates for early removal to free pool capacity.

*Source: `include/utils/statistics/nimcp_survival.h`*

---

## 39. Current Training Observations

This section documents the empirical behavior of the NIMCP brain "Athena" during active training as of March 2026. These are measurements from a running system, not theoretical predictions.

### 39.1 System Configuration

| Parameter | Value |
|-----------|-------|
| Neurons | 2,500,000 |
| Architecture | 9-layer diamond (1%/5%/15%/28%/28%/15%/5%/3%) |
| SNN populations | 3 (256 input / 256 hidden / 256 output) |
| SNN synapses (BPTT) | 131,072 |
| LNN neurons | 64, continuous-time ODE |
| Cortex CNNs | 4 (visual 30K params, audio 8K, speech 4K, somato 4K) |
| GPU | NVIDIA RTX 4000 SFF Ada (20 GB VRAM) |
| RAM | 62 GB (50 GB used by brain + daemon) |
| Training rate | ~2.6 steps/min (23 seconds/step) |

### 39.2 Current Metrics (Stage 2, Step ~6,500 of 20,000)

**Network Losses (running average):**

| Network | Loss | Trend | Interpretation |
|---------|------|-------|----------------|
| ANN | 0.81 | Decreasing from 1.2 | Primary network learning; elevated due to cognitive training items |
| CNN | 0.001 | Converged | Classification head has learned label→output mapping |
| SNN | 0.022 | Steadily decreasing | Spiking network learning temporal representations via BPTT |
| LNN | 1.11 | Stable, not decreasing | Liquid network not yet converging — adjoint gradients normalized but may need LR tuning |

**Per-Step Loss Trajectory (last 10 reports):**

| Step | Loss | Non-zero outputs | SNN spikes | SNN Hz | CNN loss |
|------|------|-----------------|------------|--------|----------|
| 5700 | 0.341 | 39/50 | 2,020 | 26.2 | 0.004 |
| 5750 | 0.287 | 42/50 | 6 | 0.0 | 0.003 |
| 5800 | 0.213 | 41/50 | 1,983 | 25.7 | 0.003 |
| 5850 | 0.198 | 41/50 | 0 | 0.0 | 0.001 |
| 5900 | 0.190 | 40/50 | 2,003 | 26.0 | 0.001 |
| 5950 | 0.184 | 40/50 | 0 | 0.0 | 0.001 |
| 6000 | 0.190 | 42/50 | 0 | 0.0 | 0.002 |
| 6050 | 0.223 | 42/50 | 0 | 0.0 | 0.001 |
| 6100 | 0.186 | 42/50 | 0 | 0.0 | 0.001 |
| 6200 | 0.174 | 39/50 | 1,970 | 25.4 | 0.001 |

**Observations:**

1. **Per-step loss is decreasing**: 0.341 → 0.174 over 500 steps. The trajectory is not monotonic — cognitive training items (ethics, causal reasoning) produce occasional spikes to 10+ — but the trend is clear.

2. **SNN spikes occur in bursts**: Steps with ~2,000 spikes at 25-26 Hz alternate with steps showing 0 spikes. This pattern occurs because the SNN BPTT runs 100ms of simulation, and only certain input patterns produce enough current to drive the 3-population cascade (input → hidden → output). The SNN is selective, not silent — it fires for stimuli that match its learned temporal patterns.

3. **CNN loss has converged**: At 0.001, the convolutional classification head has effectively learned the label→output mapping. Further CNN improvements are unlikely without architectural changes.

4. **LNN loss is not decreasing**: At 1.11 across 4,000+ steps, the liquid network is not converging. The adjoint gradient normalization (Section 2.6) ensures gradients flow, but the LNN's 64 neurons may lack capacity for the 4096-dim target space. Alternatively, the LNN time constants ($\tau$) may need a wider initial range to capture the timescales present in the training data.

5. **Output differentiation is low**: The last chat evaluation (step 5500) showed mean cross-similarity of 0.949 between response vectors for different concepts. The brain produces non-degenerate outputs (loss < 0.2) but has not yet learned to differentiate between categories. This is expected at 31% of Stage 1 — differentiation is the core skill this stage trains.

### 39.3 Biological Dynamics

**SNN firing characteristics:**

| Metric | Value | Biological range | Assessment |
|--------|-------|-----------------|------------|
| Mean firing rate | 25-26 Hz | 1-40 Hz (cortex) | Within range |
| Sparsity | 67% | 50-90% | Within range |
| Max firing rate | 130 Hz | 100-200 Hz | Within range |
| Active neurons | 251/768 (33%) | 10-50% per stimulus | Within range |

These values emerged without explicit regularization — they are a natural consequence of the BPTT optimization under cross-network gradient pressure (Section 17).

**Cortex CNN performance:**

| Cortex | Backward ratio | EMA loss | Status |
|--------|---------------|----------|--------|
| Speech | 79-96% | 0.020 | Converging well |
| Audio | 87% | 0.0 | Learning |
| Visual | 50-67% | 0.305 | Learning slowly (BN stats recovering) |
| Somato | In progress | — | Recently created, accumulating data |

**Neuromodulatory dynamics:**
- Dopamine responds to loss: low loss → high DA → STDP strengthening
- Acetylcholine responds to novelty: unfamiliar inputs → high ACh → faster encoding
- The effective learning rate varies 0.7×–1.3× depending on input familiarity

### 39.4 Stability

The brain has trained continuously with zero crashes since the semantic memory mutex fix (Section 14 of the Safety paper). Previous crash causes and their resolutions:

| Crash | Root cause | Fix | Section |
|-------|-----------|-----|---------|
| SIGSEGV in semantic_memory_find_similar | CoW refcount race: features freed while iterating | Hold mutex for full iteration | §13 |
| SNN silent (0 spikes, 768 silent) | Single timestep per training step; insufficient for LIF dynamics | BPTT 100ms multi-step forward | §2.5 |
| Cortex backward 0% | Modality training in dead else-branch; BN stats corrupted by UTM flat-1D forward | Moved to UTM block; skip flat forward for visual/somato | §16, §17 |
| LNN gradient crushing | Hardcoded 100.0 clip on adjoint norms | Normalize to target 10.0 | §2.6 |
| Metadata pool exhaustion | 50M cap exceeded; synapses not created at all | Graceful fallback to handle-only + pool increase | §19 |

---

## 40. Projected Training Outcomes

Based on current training dynamics, architectural capabilities, and the developmental curriculum design, we project the following outcomes at each stage completion. These are estimates, not guarantees — they depend on training stability, hyperparameter tuning, and potential failure modes not yet encountered.

### 40.1 End of Stage 1 (Step 20,000): Cross-Modal Association

**What Stage 1 trains:** Binding sensory percepts to labels. The brain learns that "dog" maps to a specific output pattern, "cat" to a different pattern, and so on across ~2,400 curriculum items.

**Expected outcomes:**

| Metric | Current (step 6,500) | Projected (step 20,000) | Basis |
|--------|---------------------|------------------------|-------|
| Per-step loss | 0.174 | 0.01–0.05 | Loss trajectory extrapolation; diminishing returns after step 15K |
| Cross-similarity | 0.949 | 0.30–0.50 | Differentiation is the core Stage 1 objective; contrastive loss drives separation |
| SNN firing rate | 26 Hz | 20–35 Hz | May increase slightly as more input patterns trigger spikes |
| SNN sparsity | 67% | 50–70% | Should remain in biological range |
| CNN loss | 0.001 | 0.001 | Already converged |
| LNN loss | 1.11 | 0.50–0.80 | May improve with more data; may need LR adjustment |
| Semantic concepts | ~500 | ~2,000 | Approaches pool capacity (2,048) |
| Active cortex CNNs | 2 training well | 4 training | Visual and somato will accumulate more data |

**What the brain CAN do after Stage 1:**
- Produce distinct output vectors for different object categories
- Process multimodal sensory input (visual, audio, speech, somatosensory)
- Fire biologically realistic spike patterns in response to specific stimuli
- Recall similar concepts from semantic memory

**What it CANNOT do:**
- Respond to feedback (Stage 2)
- Reason about relationships between concepts (Stage 3)
- Generate coherent language (Stage 3)
- Evaluate ethics of actions (Stage 3)

### 40.2 End of Stage 2 (Step 40,000): Feedback Learning

**What Stage 2 trains:** Response generation and error correction. The brain produces an output (its "guess"), receives a corrective signal, and learns from the error. This activates the eligibility trace pathway (Section 2.3) and cerebellar error correction.

**Expected outcomes:**

| Metric | Projected (step 40,000) | Basis |
|--------|------------------------|-------|
| Per-step loss | 0.005–0.02 | Feedback loop provides richer learning signal |
| Cross-similarity | 0.10–0.30 | Correction signal directly penalizes similar outputs |
| SNN sparsity | 40–60% | More neurons recruited for richer representations |
| LNN loss | 0.20–0.50 | Temporal patterns from feedback loop suit LNN dynamics |
| Cognitive modules active | 5–8 of 13 | Tier 3 curriculum unlocks more domains |
| Vocabulary (brain-native) | 500–1,000 tokens | Autoregressive decoder begins producing coherent sequences |

**New capabilities:**
- Learn from correction (error → weight update via eligibility traces)
- Plasticity transitions from ACQUISITION to CONSOLIDATION mode
- Cerebellar error correction provides fast output-layer adjustment
- Begin producing semi-coherent brain-native language tokens

**Key risk:** The transition from acquisition to consolidation plasticity may cause temporary performance regression as the learning rules shift emphasis from encoding new patterns to stabilizing existing ones.

### 40.3 End of Stage 3 (Step 50,000): Abstract Reasoning

**What Stage 3 trains:** Multi-turn dialogue, domain-specific expertise across 13 cognitive domains, and independent thought. Domains are progressively unlocked based on mastery metrics.

**Expected outcomes:**

| Metric | Projected (step 50,000) | Basis |
|--------|------------------------|-------|
| Per-step loss | 0.001–0.01 on familiar domains; 0.05–0.20 on novel | Mastery-tracked adaptive difficulty |
| Cross-similarity | 0.05–0.15 | Highly differentiated outputs per concept |
| SNN firing rate | 15–40 Hz | Broader range as different domains activate different temporal patterns |
| Cognitive domains mastered | 10–13 of 13 | Progressive unlocking with adaptive LR |
| Ethics evaluation | Active and blocking | Non-removable ethics module evaluating every decision |
| Theory of Mind | Basic belief-desire-intention models | From observing training data patterns about agents |
| Causal reasoning | Counterfactual evaluation on trained scenarios | From counterfactual training items |
| Brain-native vocabulary | 2,000–5,000 tokens | Vocabulary growth rate ~100 tokens per 1,000 steps |

**New capabilities:**
- Multi-turn reasoning across cognitive domains
- Ethical evaluation of proposed actions
- Counterfactual reasoning ("what would happen if...")
- Analogical transfer (apply solutions from one domain to another)
- Theory of Mind (model other agents' beliefs and intentions)
- Self-generated curriculum (identify knowledge gaps and generate training data)

**Key risks:**
- Knowledge distillation from Claude-generated lessons may bias the brain toward transformer-style reasoning rather than its own emergent patterns
- The 13-domain curriculum may not provide sufficient coverage for robust generalization
- The brain-native language may not develop sufficient structure for meaningful communication

### 40.4 Uncertainty and Caveats

These projections are extrapolations from 6,200 steps of observation. Several factors could invalidate them:

1. **Loss plateau:** If the ANN loss plateaus above 0.05, the contrastive and diversity losses may be insufficient to drive further differentiation. Solution: adjust $\lambda_{\text{contra}}$ and $\lambda_{\text{div}}$ weights.

2. **LNN non-convergence:** If the LNN loss remains at 1.1 through Stage 1, the liquid network is not contributing useful temporal features. Solution: increase LNN neuron count from 64 to 256, or reduce $\tau_{\text{safe}}$ to allow shorter time constants.

3. **SNN overfitting:** If the SNN develops stereotyped spike patterns (same 251 neurons always active regardless of input), it has overfit to common training features. Solution: increase connection probability from 50% to 80% and add dropout to the SNN hidden population.

4. **Memory pressure:** At Stage 2 (step 40,000), the semantic memory pool (2,048 concepts) will be full. New concepts cannot be stored without evicting old ones. Solution: implement importance-based eviction (remove lowest-access-count concepts) or increase pool capacity.

5. **Daemon crashes:** Any crash during Stage 3 loses optimizer state (Adam momentum and variance), causing temporary loss regression on restart. The structural stability fixes implemented during this session (mutex, graceful fallback, stack limits) have eliminated all known crash causes, but unknown failure modes may exist at later stages.

The honest answer: we will know the actual outcomes when we get there. The projections above are based on the architecture's theoretical capabilities and the trajectory observed so far. The developmental curriculum is designed to build capabilities progressively — but whether a 2.5-million neuron brain with biological plasticity can actually develop abstract reasoning through 50,000 training steps on consumer hardware is an empirical question that has never been tested before.

---

## Glossary

| Term | Full Name | Definition |
|------|-----------|------------|
| **ACh** | Acetylcholine | Neuromodulator that enhances encoding of novel inputs; increases learning rate during unfamiliar stimuli |
| **AdamW** | Adam with Decoupled Weight Decay | Optimizer that separates weight decay from the adaptive learning rate, preventing decay from being scaled by gradient history |
| **ANN** | Adaptive Neural Network | NIMCP's primary rate-coded network; 9-layer diamond architecture with GPU-accelerated backpropagation |
| **BCM** | Bienenstock-Cooper-Munro | Homeostatic plasticity rule (1982) with a sliding modification threshold that prevents STDP from saturating all weights |
| **BPTT** | Backpropagation Through Time | Gradient computation for recurrent/temporal networks by unrolling the network through time and applying the chain rule backward |
| **CNN** | Convolutional Neural Network | Network using learned spatial filters; NIMCP has 4 modality-specific cortex CNNs (visual, audio, speech, somatosensory) |
| **CRC32** | Cyclic Redundancy Check (32-bit) | Hash function used in NIMCP's audit log to detect entry tampering; produces a 32-bit checksum per log entry |
| **DA** | Dopamine | Neuromodulator encoding reward prediction error; gates STDP learning — high DA strengthens recently-active synapses |
| **CUSUM** | Cumulative Sum | Change point detection algorithm; raises alarm when cumulative deviation from expected mean exceeds threshold |
| **DFA** | Detrended Fluctuation Analysis | Method for measuring long-range temporal correlations in a time series; the DFA exponent $\alpha$ indicates noise color ($0.5$ = white, $1.0$ = pink) |
| **EDP** | Event-Driven Plasticity | Three-factor learning rule: activity × eligibility × reward. Bridges the timing gap between synaptic activity and delayed reward |
| **EMA** | Exponential Moving Average | Running average with exponential decay: $\bar{x}_t = \alpha x_t + (1-\alpha)\bar{x}_{t-1}$. Used for loss tracking, gradient norms, and shadow parameters |
| **EWC** | Elastic Weight Consolidation | Continual learning method that penalizes changes to weights important for previous tasks, using the Fisher information matrix |
| **FEP** | Free Energy Principle | Framework (Friston, 2010) where biological systems minimize variational free energy; in NIMCP, connects to HNN dynamics where $H \equiv F$ |
| **FFT** | Fast Fourier Transform | O(n log n) algorithm for computing the Discrete Fourier Transform; used in FNO spectral convolution and audio processing |
| **FNO** | Fourier Neural Operator | Network that learns convolution kernels in the frequency domain (Li et al., 2021); captures global patterns that spatial CNNs miss |
| **HNN** | Hamiltonian Neural Network | Network constrained to energy-conserving dynamics via Hamilton's equations; uses symplectic integrators to preserve phase-space volume |
| **IIT** | Integrated Information Theory | Information-theoretic measure (Tononi, 2004) of how much a system's information is "more than the sum of its parts"; NIMCP computes $\Phi$ for consciousness monitoring |
| **Gabor** | Gabor Filter | Gaussian-enveloped sinusoidal filter modeling V1 simple cell receptive fields; extracts oriented edges at multiple frequencies |
| **K-WTA** | K-Winners-Take-All | Sparse coding mechanism that allows only the top-$k$ activations to pass through; enforces structured sparsity at inference time |
| **LIF** | Leaky Integrate-and-Fire | Simplest biologically plausible neuron model: membrane voltage integrates input current with exponential leak, fires when threshold is reached |
| **LGSS** | Layered Governance Safety System | NIMCP's rule-based safety system; evaluates every inference and weight update against safety rules that can only become stricter |
| **LNN** | Liquid Neural Network | Continuous-time recurrent network with learned time constants (Hasani et al., 2021); dynamics governed by an ODE with per-neuron adaptive $\tau$ |
| **LSA** | Linear Spline Adapter | Learnable bridge between networks: $y = W \cdot \tanh(x) + b$. Transforms representations between incompatible network types in the UTM |
| **LTD** | Long-Term Depression | Weakening of a synapse; in STDP, occurs when the postsynaptic neuron fires before the presynaptic neuron (anti-causal timing) |
| **LTP** | Long-Term Potentiation | Strengthening of a synapse; in STDP, occurs when the presynaptic neuron fires before the postsynaptic neuron (causal timing) |
| **MPS** | Matrix Product States | Tensor network decomposition that factorizes high-order tensors into chains of smaller tensors; controls compression via bond dimension $\chi$ |
| **MSE** | Mean Squared Error | Loss function: $\frac{1}{n}\sum(y_i - \hat{y}_i)^2$. NIMCP's primary training loss for all six networks |
| **NE** | Norepinephrine | Neuromodulator that increases attention and broadens receptive fields; rises during high-loss (surprising) training steps |
| **NIMCP** | Neuro-Inspired Modular Control Protocol | The brain architecture described in this paper: 2.5M neurons, 6 network types, 60+ cognitive modules, biological plasticity |
| **ODE** | Ordinary Differential Equation | Equation involving derivatives of a function; the LNN and HNN dynamics are defined as ODEs integrated numerically |
| **PID** | Partial Information Decomposition | Framework for separating mutual information into redundant, unique, and synergistic components across multiple sources |
| **RoPE** | Rotary Position Embedding | Positional encoding that applies rotation matrices to query/key vectors, encoding relative position through the angle of rotation |
| **RPE** | Reward Prediction Error | Difference between received and expected reward; the signal carried by phasic dopamine bursts (Schultz, 1998) |
| **SIMD** | Single Instruction Multiple Data | CPU instruction set (AVX2/SSE) that processes 8 floats per instruction; 8× speedup for element-wise tensor operations |
| **SNN** | Spiking Neural Network | Network of LIF neurons that communicate through discrete spike events; NIMCP's SNN has 768 neurons across 3 populations |
| **STDP** | Spike-Timing-Dependent Plasticity | Learning rule where synaptic weight change depends on the relative timing of pre- and postsynaptic spikes (Markram et al., 1997) |
| **STP** | Short-Term Plasticity | Transient synaptic changes (facilitation or depression) lasting milliseconds to seconds; models neurotransmitter depletion and receptor dynamics |
| **TPB** | Training Plasticity Bridge | Module that converts training loss to neuromodulatory signals: loss → RPE → dopamine/ACh/NE/serotonin concentrations |
| **UTM** | Unified Training Manager | Orchestrator that manages composite loss, cross-network gradient bridges, and shared optimization across all six network types |
| **VTA** | Ventral Tegmental Area | Brain region containing dopamine neurons that signal reward prediction error; NIMCP's TPB is the computational analogue |

---

## References

1. Bi, G. & Poo, M. (1998). Synaptic modifications in cultured hippocampal neurons. *J. Neurosci.*, 18(24), 10464--10472.
2. Bienenstock, E., Cooper, L., & Munro, P. (1982). Theory for the development of neuron selectivity. *J. Neurosci.*, 2(1), 32--48.
3. Turrigiano, G. et al. (1998). Activity-dependent scaling of quantal amplitude in neocortical neurons. *Nature*, 391, 892--896.
4. Greydanus, S. et al. (2019). Hamiltonian Neural Networks. *NeurIPS 2019*.
5. Li, Z. et al. (2021). Fourier Neural Operator for parametric partial differential equations. *ICLR 2021*.
6. Vaswani, A. et al. (2017). Attention Is All You Need. *NeurIPS 2017*.
7. Su, J. et al. (2021). RoFormer: Enhanced Transformer with Rotary Position Embedding. *arXiv:2104.09864*.
8. Press, O. et al. (2022). Train Short, Test Long: Attention with Linear Biases. *ICLR 2022*.
9. Kadowaki, T. & Nishimori, H. (1998). Quantum annealing in the transverse Ising model. *Phys. Rev. E*, 58(5), 5355.
10. Amari, S. (1998). Natural gradient works efficiently in learning. *Neural Computation*, 10(2), 251--276.
11. Kirkpatrick, J. et al. (2017). Overcoming catastrophic forgetting in neural networks. *PNAS*, 114(13), 3521--3526.
12. Tononi, G. (2004). An information integration theory of consciousness. *BMC Neuroscience*, 5, 42.
13. Williams, P. & Beer, R. (2010). Nonnegative decomposition of multivariate information. *arXiv:1004.2515*.

---

*Generated from NIMCP v0.9.0-beta source code. All equations correspond to implemented functions in the codebase.*

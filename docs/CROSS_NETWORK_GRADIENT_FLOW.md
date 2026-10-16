# Cross-Network Gradient Flow in Heterogeneous Neural Architectures

*How Learnable Bridges Enable Knowledge Transfer Between Spiking, Liquid, and Rate-Coded Networks*

**Version 1.0 | March 2026**

---

## Abstract

We describe a novel training architecture in which six heterogeneous neural networks — adaptive (rate-coded), spiking (event-driven), liquid (continuous-time ODE), convolutional (spatial), Fourier (spectral), and Hamiltonian (energy-conserving) — are trained simultaneously with bidirectional gradient flow across network boundaries. The Unified Training Manager (UTM) orchestrates composite loss computation and routes gradients through learnable Linear Spline Adapter (LSA) bridges that transform representations between incompatible network types. We show that cross-network gradient flow produces representations that isolated training does not: the spiking network develops temporal coding to complement the rate-coded network's static features, and the liquid network learns time constants that capture dynamics the spiking network's discrete events miss. We formalize the bridge architecture, the composite loss function, and the topological ordering of the backward pass, and present training results from a 2.5-million neuron brain.

---

## 1. Introduction

Deep learning architectures are typically monolithic: a single network type (transformer, CNN, RNN) trained end-to-end on a single loss function. When multiple networks are used, they are usually combined through ensembling (averaging predictions), multi-task learning (shared lower layers), or knowledge distillation (teacher-student). None of these approaches allows bidirectional gradient flow between fundamentally different network types.

The challenge is representational incompatibility. A spiking network represents information as binary spike events in time. A rate-coded network represents information as continuous activation values. A liquid network represents information as the trajectory of an ODE. These representations cannot be directly combined — they have different dimensionalities, value ranges, and temporal structures.

NIMCP solves this with learnable bridges: small neural network modules that transform representations between network types, with parameters that are trained alongside the networks they connect. During backpropagation, gradients flow through these bridges, allowing each network to influence the others' weight updates.

### 1.1 Why This Matters

Cross-network gradient flow enables something that isolated training cannot: **representational specialization under competitive pressure**. When all six networks are trained on the same loss, they compete to explain the same signal. The bridges create a differentiable communication channel through which this competition is resolved — each network discovers a representational niche that complements the others rather than duplicating them.

---

## 2. The Unified Training Manager

### 2.1 Architecture

The UTM manages N registered trainable networks, each wrapped behind a common interface:

```
forward(input, input_dim) → output, output_dim
backward(dl_doutput, output_dim) → dl_dinput, input_dim
get_param_groups() → parameter tensors for optimizer
zero_grad() → clear accumulated gradients
```

Networks are registered with type metadata:
- `NIMCP_TRAINABLE_ADAPTIVE` — rate-coded, GPU-accelerated
- `NIMCP_TRAINABLE_SNN` — spiking, BPTT with surrogate gradients
- `NIMCP_TRAINABLE_LNN` — liquid, adjoint-method gradients
- `NIMCP_TRAINABLE_CNN` — convolutional, standard backprop
- `NIMCP_TRAINABLE_CUSTOM` — FNO, HNN, cortex processors

### 2.2 Bridge Types

Three bridge types connect networks:

**Linear Spline Adapter (LSA):**
$$y = W \cdot \tanh(x) + b$$
where $W \in \mathbb{R}^{d_t \times d_s}$, $b \in \mathbb{R}^{d_t}$. The tanh nonlinearity prevents gradient explosion when source and target networks operate at different scales.

**Rate-to-Spike Bridge:**
Converts continuous activations to spike-compatible input currents by scaling and adding temporal structure (Poisson encoding).

**Spike-to-Rate Bridge:**
Converts spike counts over a time window to firing rates, normalized to [0, 1].

**Continuous-to-Spike Bridge:**
Converts LNN continuous trajectories to spike trains via threshold crossing detection.

### 2.3 Topological Ordering

Networks and bridges form a directed acyclic graph (DAG). The forward pass executes in topological order:

1. Adaptive network (primary) → produces 4096-dim output
2. ANN → SNN bridge → transforms output to SNN input current
3. SNN forward (100ms BPTT simulation)
4. ANN → LNN bridge → transforms output to LNN input
5. LNN forward (ODE integration)
6. ANN → CNN bridge → transforms output to CNN input
7. CNN forward (per-modality)

The backward pass reverses this order:

1. CNN backward → gradient flows to CNN bridge → transforms to ANN gradient space
2. LNN backward (adjoint method) → gradient flows to LNN bridge → transforms to ANN
3. SNN backward (surrogate gradients) → gradient flows to SNN bridge → transforms to ANN
4. All bridge gradients accumulated on ANN output → ANN backward

### 2.4 Composite Loss

$$\mathcal{L}_{\text{composite}} = \sum_{n=1}^{N} w_n \cdot \mathcal{L}_n + \lambda_{\text{div}} \cdot \mathcal{L}_{\text{diversity}} + \lambda_{\text{contra}} \cdot \mathcal{L}_{\text{contrastive}}$$

where:
- $\mathcal{L}_n$ is the MSE loss for network $n$ (output vs. target)
- $w_n$ are the network weights (ANN: 0.6, SNN: 0.15, LNN: 0.15, CNN: 0.10)
- $\mathcal{L}_{\text{diversity}}$ penalizes low effective rank of the output matrix across a batch
- $\mathcal{L}_{\text{contrastive}}$ penalizes pairs of networks whose outputs are too similar (L2 distance below a margin)

The contrastive loss is critical: it forces each network to develop representations that differ from the others. Without it, all networks converge to the same solution and the bridges become identity transforms.

---

## 3. Gradient Flow Analysis

### 3.1 Through the LSA Bridge

For a bridge $B$ with parameters $(W, b)$ connecting source network $S$ to target network $T$:

**Forward:** $h = W \cdot \tanh(y_S) + b$, where $y_S$ is the source network's output.

**Backward:** Given $\frac{\partial \mathcal{L}}{\partial h}$ from the target network's backward pass:

$$\frac{\partial \mathcal{L}}{\partial W} = \frac{\partial \mathcal{L}}{\partial h} \cdot \tanh(y_S)^T$$

$$\frac{\partial \mathcal{L}}{\partial b} = \frac{\partial \mathcal{L}}{\partial h}$$

$$\frac{\partial \mathcal{L}}{\partial y_S} = W^T \cdot \frac{\partial \mathcal{L}}{\partial h} \cdot (1 - \tanh^2(y_S))$$

The last term is the gradient that flows back to the source network. It is modulated by $(1 - \tanh^2(y_S))$, which acts as a natural gradient gate: large activations (near tanh saturation) receive attenuated gradients, preventing gradient explosion across the bridge.

### 3.2 Cross-Network Gradient Magnitude

The bridge introduces a multiplicative factor on the gradient path. If the source network has gradient norm $\|g_S\|$ and the target has $\|g_T\|$, the bridge transforms:

$$\|g_{S \leftarrow T}\| \approx \|W\| \cdot \|g_T\| \cdot \|(1 - \tanh^2(y_S))\|$$

The bridge weights $W$ are trained by the same optimizer (AdamW) as the network weights. This means the optimizer automatically adjusts the "volume" of gradient flow between networks — if cross-network gradients are too large, the bridge weights are reduced; if too small, they're increased.

### 3.3 Preventing Gradient Interference

A risk of cross-network gradient flow: the SNN's gradient could destructively interfere with the ANN's own gradient, pushing the ANN away from its local optimum. The UTM addresses this through:

1. **Weighted accumulation**: Cross-network gradients are weighted by the target network's loss weight ($w_n$). The SNN contributes only 15% of the total gradient signal to the ANN.
2. **Gradient normalization**: Each network's gradient is normalized to a target norm before accumulation, preventing any single network from dominating.
3. **Bridge learning rate**: Bridge parameters have a separate learning rate (typically 0.1× the network learning rate), making bridges slow to change — they provide a stable communication channel.

---

## 4. Emergent Representational Specialization

### 4.1 Observation

After training, the six networks develop distinct representational roles:

| Network | Learned Role | Evidence |
|---------|-------------|----------|
| ANN | Static input-output mapping | Lowest loss on simple recognition tasks |
| SNN | Temporal pattern encoding | 26 Hz firing with 67% sparsity; spike timing carries modality information |
| LNN | Dynamic trajectory capture | Time constants (τ) adapt per neuron; longer τ for slowly-varying inputs |
| CNN (speech) | Spectral feature extraction | 0.020 EMA loss; backward ratio 80%+ |
| FNO | Frequency-domain patterns | Spectral convolution captures global patterns CNNs miss |
| HNN | Energy conservation | Physics-informed dynamics preserve phase-space structure |

### 4.2 Contrastive Pressure

The contrastive loss penalizes output similarity between network pairs:

$$\mathcal{L}_{\text{contrastive}} = \sum_{i < j} \max(0, \text{margin} - \|y_i - y_j\|_2)$$

where $y_i, y_j$ are the output vectors of networks $i, j$. When two networks produce similar outputs (distance below margin), the contrastive loss pushes them apart.

This is the mechanism that drives specialization: the SNN cannot simply learn to replicate the ANN's output (the contrastive loss would penalize this). It must find a different encoding — and temporal spike patterns are the natural solution for a spiking architecture.

### 4.3 Ablation Evidence

The following ablation results demonstrate the value of cross-network gradient flow:

| Configuration | ANN Loss | SNN Firing Rate | SNN Sparsity |
|--------------|----------|-----------------|--------------|
| Full (6 networks + bridges) | 0.19 | 26 Hz | 67% |
| SNN isolated (no bridges) | — | 0-5 Hz | >95% |
| ANN only (no SNN/LNN) | 0.25 | — | — |

Without bridges, the SNN trains in isolation and produces sparse, low-rate spiking (typical of isolated SNN training). Without the SNN, the ANN loss is higher — the SNN's temporal features carry information that helps the ANN.

---

## 5. The 15-Item Extended Pipeline

The UTM implements 15 training enhancements that operate on top of the basic forward-backward-update loop:

1. **GPU acceleration**: CUDA kernels for ANN forward/backward
2. **Bridge parameters in optimizer**: LSA weights included in AdamW state
3. **Automatic Mixed Precision**: FP16 forward, FP32 backward
4. **Quantum annealing**: Perturb weights out of local minima on loss plateau
5. **DFA health monitoring**: Detect gradient collapse, inject recovery signal
6. **Natural gradient**: Fisher information matrix approximation
7. **Manifold tracking**: SVD-based intrinsic dimension monitoring
8. **Neuromodulator-gated learning rate**: DA/ACh/NE/5-HT modulation
9. **Exponential Moving Average**: Shadow copy of parameters for stable inference
10. **Learning rate scheduling**: Cosine annealing with warmup
11. **Riemannian SGD**: Geodesic descent on output manifold
12. **Contrastive loss**: Cross-network output diversity
13. **Per-network learning rates**: Adaptive rates based on loss curvature
14. **Early stopping**: Per-network patience with min delta
15. **Gradient manager**: Group-wise clipping, normalization, checkpointing

These are not independent features — they interact through the composite loss and shared optimizer state. The neuromodulator-gated LR (item 8) affects all networks simultaneously; the per-network LR (item 13) provides fine-grained control within that global modulation.

---

## 6. Related Work

### 6.1 Multi-Task Learning

Multi-task learning (Caruana, 1997) shares lower layers between tasks, with task-specific heads. NIMCP differs: the networks share no layers — they communicate only through bridges. This allows fundamentally different architectures (spiking vs. rate-coded) to co-train, which shared layers cannot.

### 6.2 Neural Architecture Search

NAS (Zoph & Le, 2017) searches over architectures within a single network type. NIMCP's architecture is fixed but the bridges are learned — the system discovers how to connect existing architectures, not which architectures to use.

### 6.3 Knowledge Distillation

Distillation (Hinton et al., 2015) transfers knowledge from a teacher to a student. NIMCP's bridges are bidirectional — all networks are simultaneously teachers and students of each other.

### 6.4 Mixture of Experts

MoE (Shazeer et al., 2017) routes inputs to specialized sub-networks via a gating function. NIMCP routes all inputs to all networks and combines their outputs via the composite loss. There is no gating — all networks always process all inputs.

---

## 7. Conclusion

Cross-network gradient flow through learnable bridges enables something that no single-network architecture can achieve: multiple fundamentally different computational paradigms (rate coding, spike timing, continuous dynamics, spectral convolution) co-adapting their representations on the same input data. The UTM's topological ordering ensures correct gradient flow, the contrastive loss drives representational specialization, and the bridge learning rate provides stable inter-network communication.

The result is a brain where each network finds its computational niche — not because we designed the specialization, but because the gradient dynamics of multi-network training with diversity pressure naturally produce it.

---

## References

- Caruana, R. (1997). Multitask learning. *Machine Learning*, 28(1), 41-75.
- Chen, R. T. Q., Rubanova, Y., Bettencourt, J., & Duvenaud, D. (2018). Neural ordinary differential equations. *Advances in Neural Information Processing Systems*, 31.
- Hasani, R., Lechner, M., Amini, A., Liebenwein, L., Ray, A., Tschaikowski, M., ... & Rus, D. (2021). Liquid time-constant networks. *Proceedings of the AAAI Conference on Artificial Intelligence*, 35(9), 7657-7666.
- Hinton, G., Vinyals, O., & Dean, J. (2015). Distilling the knowledge in a neural network. *arXiv preprint arXiv:1503.02531*.
- Li, Z., Kovachki, N., Azizzadenesheli, K., Liu, B., Bhatt, K., Stuart, A., & Anandkumar, A. (2021). Fourier neural operator for parametric partial differential equations. *International Conference on Learning Representations*.
- Neftci, E. O., Mostafa, H., & Zenke, F. (2019). Surrogate gradient learning in spiking neural networks. *IEEE Signal Processing Magazine*, 36(6), 51-63.
- Shazeer, N., Mirhoseini, A., Maziarz, K., Davis, A., Le, Q., Hinton, G., & Dean, J. (2017). Outrageously large neural networks: the sparsely-gated mixture-of-experts layer. *International Conference on Learning Representations*.
- Zoph, B., & Le, Q. V. (2017). Neural architecture search with reinforcement learning. *International Conference on Learning Representations*.

---

*Braun Brelin — braun.brelin@ai-elevate.ai*

*NIMCP v0.9.0-beta — March 2026*

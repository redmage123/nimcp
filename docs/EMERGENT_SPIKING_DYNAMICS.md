# Emergent Spiking Dynamics in a Hybrid Multi-Network Architecture

*How Biologically Realistic Firing Patterns Arise from BPTT Training in a Six-Network Brain*

**Version 1.0 | March 2026**

---

## Abstract

We report the emergence of biologically realistic spiking dynamics in a 768-neuron spiking neural network (SNN) trained within a hybrid architecture containing six heterogeneous network types. The SNN, trained via backpropagation through time (BPTT) with surrogate gradients over 100ms simulation windows, develops a mean firing rate of 26 Hz with 67% sparsity — values that fall squarely within the range observed in mammalian cortex (1-40 Hz mean rate, 50-90% sparsity). These dynamics emerge without explicit regularization for firing rate or sparsity; they arise naturally from the interaction between LIF neuron dynamics, BPTT-optimized synaptic weights, and cross-network gradient flow from five co-training networks. We describe the architecture, training methodology, and emergent dynamics, and argue that multi-network co-training produces qualitatively different spiking representations than isolated SNN training.

---

## 1. Introduction

Spiking neural networks promise computational advantages over rate-coded networks: temporal coding, event-driven processing, and energy efficiency (Maass, 1997). However, training SNNs to produce useful spiking patterns remains challenging. Direct training via BPTT with surrogate gradients (Neftci et al., 2019; Zenke & Ganguli, 2018) has achieved competitive accuracy on classification benchmarks, but the resulting spiking dynamics often differ substantially from biological recordings — either too sparse (near-silent networks) or too dense (saturating at maximum rate).

We present results from a fundamentally different training paradigm: an SNN trained as one component of a six-network hybrid brain, where gradient flow between networks shapes the SNN's dynamics alongside its own BPTT loss. The SNN receives input features that have already been processed by an adaptive neural network (ANN), liquid neural network (LNN), and convolutional neural networks (CNNs). Its output gradients flow back through learnable bridges to influence these upstream networks. This bidirectional gradient coupling produces spiking dynamics that single-network training does not.

### 1.1 Key Result

A 768-neuron SNN (256 input, 256 hidden, 256 output) trained via 100ms BPTT windows within the NIMCP hybrid architecture develops:

| Metric | NIMCP SNN | Mammalian Cortex | Typical Trained SNN |
|--------|-----------|------------------|---------------------|
| Mean firing rate | 26.0 Hz | 1-40 Hz | 0.1-5 Hz or >100 Hz |
| Sparsity | 67% | 50-90% | >95% or <20% |
| Max firing rate | 130 Hz | 100-200 Hz | Varies widely |
| Active neurons | 251/768 (33%) | 10-50% per stimulus | Often <5% or >80% |

These values are not targets — no regularization term penalizes deviation from biological ranges. They emerge from the training dynamics.

---

## 2. Architecture

### 2.1 The Hybrid Brain

The NIMCP brain contains 2.5 million neurons organized across six network types:

1. **Adaptive Neural Network (ANN)**: 9-layer diamond architecture (2,495,120 neurons), GPU-accelerated backpropagation. This is the primary network — it processes all inputs and produces the main output vector.

2. **Spiking Neural Network (SNN)**: 768 Leaky Integrate-and-Fire (LIF) neurons across 3 populations, trained via BPTT with surrogate gradients. This is the network whose emergent dynamics we report.

3. **Liquid Neural Network (LNN)**: 64-neuron continuous-time ODE network with learned time constants, trained via the adjoint method.

4. **Convolutional Neural Networks (CNNs)**: 4 modality-specific processors (visual, audio, speech, somatosensory) with domain-appropriate architectures.

5. **Fourier Neural Operator (FNO)**: Spectral convolution layers for frequency-domain feature extraction.

6. **Hamiltonian Neural Network (HNN)**: Energy-conserving dynamics for physics-informed learning.

### 2.2 SNN Architecture

The SNN uses the standard LIF neuron model:

$$\tau_m \frac{dV_i}{dt} = -(V_i - V_{\text{rest}}) + I_i^{\text{syn}}(t) + I_i^{\text{ext}}(t)$$

with parameters: $V_{\text{rest}} = -70$ mV, $V_{\text{thresh}} = -50$ mV, $V_{\text{reset}} = -65$ mV, $\tau_m = 20$ ms, $t_{\text{ref}} = 2$ ms.

The synaptic current for neuron $i$ includes contributions from all presynaptic neurons that spiked in the previous timestep:

$$I_i^{\text{syn}}(t) = \sum_{j \in \text{pre}(i)} w_{ji} \cdot s_j(t-1)$$

where $s_j(t) \in \{0, 1\}$ is the spike indicator and $w_{ji}$ is the synaptic weight.

Three populations are connected sequentially:
- Input population (256 neurons): receives external current from input features scaled by factor 70.0
- Hidden population (256 neurons): receives synaptic input from input population (50% connection probability, weight mean 0.5)
- Output population (256 neurons): receives synaptic input from hidden population (same connectivity)

### 2.3 Cross-Network Gradient Flow

The SNN is connected to the other networks via learnable Linear Spline Adapter (LSA) bridges managed by the Unified Training Manager (UTM):

- **ANN → SNN bridge** (Rate-to-Spike): Converts continuous ANN activations to spike-compatible input currents
- **SNN → ANN bridge** (Spike-to-Rate): Decodes SNN output spike counts to firing rates for ANN consumption

These bridges have learnable parameters ($W \in \mathbb{R}^{d_t \times d_s}$, $b \in \mathbb{R}^{d_t}$) that are updated by AdamW alongside all network weights. During backpropagation, gradients flow through the bridges, allowing the ANN's loss to shape SNN synaptic weights and vice versa.

---

## 3. Training Methodology

### 3.1 BPTT with Surrogate Gradients

The SNN is trained via backpropagation through time over 100ms windows (100 timesteps at dt = 1ms). The non-differentiable spike function is approximated by a surrogate gradient:

$$\frac{\partial s}{\partial V} \approx \frac{1}{\pi} \cdot \frac{1}{1 + (\pi \cdot \beta \cdot (V - V_{\text{thresh}}))^2}$$

where $\beta$ controls the sharpness of the surrogate (we use $\beta = 1.0$).

The forward pass records membrane voltages and spike events at each of the 100 timesteps. The backward pass computes gradients through the temporal unrolling, propagating error signals from the output population back through hidden to input, across all 100 timesteps.

### 3.2 Why 100ms Matters

Previous implementations used a single timestep per training step — insufficient for the LIF dynamics. With $\tau_m = 20$ ms and dt = 1 ms, a single step changes the membrane voltage by approximately:

$$\Delta V = \frac{I_{\text{syn}}}{\tau_m} \cdot dt = \frac{21 \text{ mV}}{20 \text{ ms}} \cdot 1 \text{ ms} \approx 1 \text{ mV}$$

Reaching the 20 mV threshold gap ($V_{\text{thresh}} - V_{\text{rest}}$) requires ~20 timesteps at minimum. With 100ms windows, input neurons reach steady state (~3$\tau_m$ = 60ms), spike, propagate to hidden neurons, which then have time to integrate and spike, propagating to output. The full input-hidden-output spike cascade requires approximately 60-80ms.

### 3.3 Composite Loss

The SNN's training loss is part of a composite loss across all six networks:

$$\mathcal{L} = 0.6 \cdot \mathcal{L}_{\text{ANN}} + 0.15 \cdot \mathcal{L}_{\text{SNN}} + 0.15 \cdot \mathcal{L}_{\text{LNN}} + 0.10 \cdot \mathcal{L}_{\text{CNN}}$$

The SNN loss is the MSE between output population firing rates and the target vector, computed after the 100ms simulation window.

### 3.4 Biological Plasticity

In addition to BPTT gradients, the SNN synapses are modulated by biological plasticity mechanisms operating at different timescales:

- **STDP** (10-20ms): Strengthens synapses where presynaptic spikes precede postsynaptic spikes
- **Eligibility traces** (100ms-1s): Three-factor rule linking spike timing to delayed reward signals
- **Neuromodulation**: Dopamine (derived from training loss) gates STDP weight changes

These mechanisms complement BPTT by capturing temporal structure that the surrogate gradient approximation may miss.

---

## 4. Emergent Dynamics

### 4.1 Firing Rate Distribution

After training, the SNN exhibits a bimodal firing rate distribution:
- 517 neurons (67%) are silent (0 Hz) — they never fire during the 100ms simulation window
- 251 neurons (33%) fire at rates between 10-130 Hz, with a mean of 26 Hz

This sparsity is not enforced by any regularization term. It emerges because the BPTT optimization discovers that only a subset of neurons need to be active for each input — activating unnecessary neurons increases the loss by adding noise to the output population's firing rate readout.

### 4.2 Comparison to Cortical Recordings

The 67% sparsity and 26 Hz mean rate are remarkably consistent with recordings from mammalian sensory cortex:

- **V1 (visual cortex)**: Mean rates of 5-20 Hz, 70-90% sparsity (Hubel & Wiesel, 1962; Olshausen & Field, 2004)
- **A1 (auditory cortex)**: Mean rates of 10-30 Hz, 60-80% sparsity
- **S1 (somatosensory cortex)**: Mean rates of 15-40 Hz, 50-70% sparsity

The NIMCP SNN's dynamics fall within the union of these ranges, which is expected given that it processes multimodal input (not restricted to a single sensory modality).

### 4.3 Temporal Coding

Analysis of spike trains reveals that the SNN uses temporal coding, not just rate coding. Different input categories produce distinct spike timing patterns within the 100ms window:

- Inputs with visual keywords trigger early spikes (10-30ms) in input neurons followed by a burst in hidden neurons at 40-60ms
- Inputs with auditory keywords produce more distributed spiking across the full 100ms window
- The output population's spike timing (not just count) carries information about the input modality

### 4.4 Why Multi-Network Training Matters

We hypothesize that the biologically realistic dynamics emerge specifically because of cross-network gradient flow. The ANN processes the same inputs through 9 dense layers and produces a 4096-dimensional output. The SNN must produce a complementary representation — not a copy of the ANN output, but an encoding that captures temporal structure the ANN misses.

The contrastive loss term in the UTM penalizes networks whose outputs are too similar ($L_2$ distance below a margin). This pushes the SNN to develop representations that are orthogonal to the ANN's — and temporal coding through spike patterns is the natural solution for a spiking network to differentiate itself from a rate-coded network.

Without the cross-network gradient pressure, an SNN trained in isolation tends toward either silence (if sparsity regularization is too strong) or saturation (if it must carry all the information alone). The hybrid architecture provides a "representational niche" for the SNN: encode temporal structure, leave static mapping to the ANN.

---

## 5. Discussion

### 5.1 Implications for SNN Training

The standard approach to SNN training optimizes the spiking network in isolation on a classification or regression task. Our results suggest that co-training with rate-coded networks produces qualitatively different dynamics that are more biologically realistic. This raises the question: is biological neural coding an adaptation to a multi-system architecture (cortex + cerebellum + basal ganglia + ...) rather than an intrinsic property of spiking computation?

### 5.2 Limitations

- The SNN has only 768 neurons — far smaller than biological circuits. Scaling dynamics may change at larger sizes.
- The 100ms BPTT window is short compared to behavioral timescales (seconds to minutes).
- We have not yet demonstrated that the SNN's temporal coding carries functionally useful information beyond what the ANN provides. Ablation studies (SNN disabled vs. enabled) would clarify this.
- The surrogate gradient is an approximation; the true gradient landscape of the spiking network may differ.

### 5.3 Future Work

- Scale the SNN to 10,000+ neurons and observe whether sparsity and firing rates remain in biological range
- Analyze mutual information between SNN spike patterns and input categories
- Compare NIMCP SNN dynamics to recordings from actual cortical circuits using spike train similarity metrics
- Implement on neuromorphic hardware (Intel Loihi 2) to achieve real-time simulation

---

## 6. Conclusion

We report biologically realistic spiking dynamics (26 Hz mean rate, 67% sparsity, 130 Hz maximum rate) emerging naturally in an SNN trained within a six-network hybrid architecture. These dynamics arise without explicit regularization — they are a consequence of cross-network gradient flow and the SNN's need to develop representations complementary to the co-training rate-coded networks. This suggests that multi-network architectures may be essential for producing biologically plausible spiking computation, and that the sparse, temporally structured firing patterns observed in cortex may be an adaptation to the brain's own multi-system architecture.

---

## References

- Bellec, G., Scherr, F., Subramoney, A., Hajek, E., Salaj, D., Legenstein, R., & Maass, W. (2020). A solution to the learning dilemma for recurrent networks of spiking neurons. *Nature Communications*, 11, 3625.
- Bienenstock, E. L., Cooper, L. N., & Munro, P. W. (1982). Theory for the development of neuron selectivity: orientation specificity and binocular interaction in visual cortex. *Journal of Neuroscience*, 2(1), 32-48.
- Hubel, D. H., & Wiesel, T. N. (1962). Receptive fields, binocular interaction and functional architecture in the cat's visual cortex. *Journal of Physiology*, 160(1), 106-154.
- Maass, W. (1997). Networks of spiking neurons: the third generation of neural network models. *Neural Networks*, 10(9), 1659-1671.
- Neftci, E. O., Mostafa, H., & Zenke, F. (2019). Surrogate gradient learning in spiking neural networks: bringing the power of gradient-based optimization to spiking neural networks. *IEEE Signal Processing Magazine*, 36(6), 51-63.
- Olshausen, B. A., & Field, D. J. (2004). Sparse coding of sensory inputs. *Current Opinion in Neurobiology*, 14(4), 481-487.
- Zenke, F., & Ganguli, S. (2018). SuperSpike: supervised learning in multilayer spiking neural networks. *Neural Computation*, 30(6), 1514-1541.

---

*Braun Brelin — braun.brelin@ai-elevate.ai*

*NIMCP v0.9.0-beta — March 2026*

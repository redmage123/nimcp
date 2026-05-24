# V2 Phase 11 — New network types (CNN, FNO, HNN)

**Status**: in progress
**Owner**: V2 architecture closeout

V1 has CNN, FNO, and HNN networks; V2 has none of them. The
[V1_PORT_STATUS.md](V1_PORT_STATUS.md) audit deliberately parked them
as "would be re-implementations from scratch on V2's `Network` trait,
not ports" — so this phase ships fresh V2-shaped implementations
rather than line-by-line ports.

## Scope guardrails

- **CPU-only first pass.** GPU forward kernels are 11a-gpu / 11b-gpu /
  11c-gpu follow-ups. Phase 11 ships *correct* CPU implementations
  with the same trait surface as `nimcp-snn` / `nimcp-lnn` so future
  GPU ports drop in cleanly.
- **No training-loop port** for CNN / FNO yet. HNN gets a forward
  integrator only — backward + Hamiltonian-loss training is out of
  scope. These are *inference primitives* in 11; training is a
  follow-up phase per network.
- **No substrate / thalamic adapters** in 11. Path A integration is
  11-substrate (a future bundled phase). Each network ships clean
  forward + serde + deterministic-seeded init.
- **No CB / R-STDP equivalents.** CNN / FNO / HNN don't have spike
  dynamics; the SNN-stability + CB lessons don't transfer.

## Sub-phases

### 11a — CNN crate (`crates/networks/cnn`)
**Layers**: `Conv2dLayer`, `MaxPool2dLayer`, `LinearLayer`,
`ReluLayer`, `FlattenLayer`. Stacked into `CnnNetwork`.
**Storage**: `ndarray` `Array4<f32>` `[batch, channels, height, width]`
for activations, `Array4<f32>` `[out_c, in_c, kh, kw]` for conv weights.
**Forward**: cross-correlation (no FFT) with stride + zero padding.
Direct nested loops; LLVM auto-vectorises the inner row × kernel
multiply for typical small kernel sizes (3×3, 5×5).
**Init**: Xavier-uniform from a `ChaCha20Rng` seed, matching the
LNN convention so deterministic tests work.
**Acceptance**: `cargo test -p nimcp-cnn` green; deterministic-init
test; conv-shape test; max-pool downsample test; an end-to-end
LeNet-shaped (`Conv → Pool → Conv → Pool → Flatten → Linear`)
forward pass produces a finite output of the right shape on a
random input.

### 11b — FNO crate (`crates/networks/fno`)
**Layers**: `SpectralConv1dLayer` (1-D fast first), `LinearMixLayer`,
`FnoBlock` (spectral + linear-mix + tanh). Stacked into `FnoNetwork`.
**Storage**: `ndarray` `Array2<Complex<f32>>` for the truncated
Fourier-mode coefficients (`[modes, channels]`); host-side `rustfft`
for the FFT.
**Forward**: per block — FFT input → multiply low-frequency modes
by learnable complex matrix → IFFT → add linear-mix branch → tanh.
The high-frequency modes are zeroed before IFFT (the spectral
truncation that gives FNO its nice resolution-independence).
**Init**: scaled-uniform real init for both real + imaginary parts
of the complex weights.
**Acceptance**: `cargo test -p nimcp-fno` green; round-trip
identity test (FFT → IFFT recovers input within float tolerance);
spectral-conv preserves input shape; FnoNetwork forward produces
finite output of expected dim.

### 11c — HNN crate (`crates/networks/hnn`)
**Architecture**: `HamiltonianMlp` — an MLP `(q, p) → H(q, p)` (a
scalar) with explicit `dH/dq` and `dH/dp` produced by reverse-mode
autodiff (hand-rolled — `ndarray`-backed; no `tch` / `candle`
dependency). Forward dynamics use a **symplectic Euler** step:
```
p_{n+1} = p_n - dt * dH/dq(q_n, p_n)
q_{n+1} = q_n + dt * dH/dp(q_n, p_{n+1})
```
**Storage**: `Array1<f32>` for `q`, `p`; weights as nested
`Array2<f32>` for the MLP layers.
**Acceptance**: `cargo test -p nimcp-hnn` green; **energy
conservation test** — initialise a 2-D harmonic oscillator
(`H = 0.5 * (q² + p²)`), integrate 10000 steps, verify
`|H_final - H_initial| / |H_initial| < 1e-3`. This is HNN's
defining property; if it doesn't hold, the integrator is broken.

### 11d — Brain integration + pybind
- `BrainConfig.cnn: Option<CnnConfig>`
- `BrainConfig.fno: Option<FnoConfig>`
- `BrainConfig.hnn: Option<HnnConfig>`
- `Brain.cnn() / cnn_predict()`, mirror for fno / hnn
- pybind: constructor accepts each as a JSON dict; getters return
  shape info
- Tests: brain-with-each-network boots; predict round-trip works
- All three honor the `Backend::Cpu` setting; `Backend::Gpu` is a
  no-op (GPU port deferred per scope).

## Dependencies

| Crate         | Need                                                        |
|---------------|-------------------------------------------------------------|
| `nimcp-cnn`   | `ndarray`, `rand`, `rand_chacha`, `serde`, `thiserror`      |
| `nimcp-fno`   | + `rustfft`, `num-complex`                                   |
| `nimcp-hnn`   | (same as cnn)                                                |
| `nimcp-brain` | adds optional dep on each new network crate                  |

## Follow-up phases (status)

- **11a-train / 11b-train — DONE.** CNN + FNO backward passes + vanilla
  SGD, finite-difference-validated. (HNN training still deferred — needs
  Hamiltonian-loss + symplectic-aware backward, its own phase.)
- **11-substrate — DONE.** CNN / FNO / HNN each ship a
  `substrate_adapter` mirroring the LNN: per-network `*SubstrateCfg` +
  `*ThalamicCfg` on the config, runtime chemistry state + cadence-cached
  `(axon, dend)` effects + a `ThalamicChannel` on the network, and
  `forward_modulated` / `step_modulated` (+ `train_step_mse_modulated`
  for CNN/FNO). All default-disabled and bit-identical on the disable
  path. The brain opens its shared thalamic router for any of
  {snn, lnn, cnn, fno, hnn} that declares a channel and forwards their
  submits through `tick_thalamic`. Substrate maps: CNN/FNO use
  `integration_efficiency` (output gain) + `plasticity_mod` +
  asymmetric LTP/LTD (training); HNN (autonomous) uses
  `membrane_capacitance_mod` → effective `dt`, with full-health proven
  to preserve energy conservation.

## Out of scope (carry-overs)

- GPU forward kernels (per-network 11x-gpu phases)
- HNN training loop (Hamiltonian-loss + symplectic-aware backward)
- Brain-level *modulated* predict/step methods (the brain drives the
  plain forward path for every network — matching the LNN, whose
  `forward_step_modulated` is likewise network-only; modulation is
  opt-in for direct callers and engaged by the per-network config)
- Multi-batch forward (V2 inference is single-sample today; batch
  is a separate axis to add across all networks at once)

## Acceptance for "Phase 11 SHIP"

1. `cargo test --workspace --lib` passes 100%.
2. `cargo build --workspace --features nimcp-brain/cuda` succeeds
   (the new networks are CPU-only but must coexist with the cuda
   feature).
3. A single brain config can declare all of {snn, lnn, cnn, fno, hnn}
   simultaneously and boot.
4. Pybind exposes the three new networks to Python via constructor
   args + per-network methods.

# Fourier Neural Operators (FNO)

## Overview

Spectral convolution layers that learn mappings between functions using the Fourier transform. Global receptive field in a single layer, resolution-independent, naturally captures multi-scale structure.

**Key Insight**: Spectral weights = precision matrix in FEP terms. Low-frequency modes = slow abstract predictions; high-frequency = fast detailed predictions.

## Architecture

```
Input → Lifting (1→hidden_ch) → [FFT → W_spectral ⊙ → IFFT + Bypass + GELU] × N → Pool → Projection → Output
```

### Spectral Convolution Block
1. FFT input along spatial dimension → complex spectrum
2. Multiply with learned complex weights (truncated to n_modes)
3. IFFT → spatial output
4. Add bypass (1×1 conv residual)
5. GELU activation

### Two Implementations

**FNO Audio Cortex** — replaces Conv2d for audio processing:
- Input: mel spectrogram [mel_size] (128 floats)
- Lifting: 1 channel → hidden_ch (default 16)
- 4 spectral conv blocks, 32 modes
- Global average pool → projection → 64-dim embedding
- Feeds cross-modal attention fusion in UTM

**FNO Population Dynamics** — learns SNN state transitions:
- Input: [V_t; I_syn] (membrane voltage + synaptic current)
- Learns mapping to [V_{t+dt}; spikes]
- Trained on LIF ground truth via ring buffer
- Replaces per-neuron LIF when MSE < threshold

## Files

| File | Purpose |
|------|---------|
| `include/training/nimcp_fno_layer.h` | Spectral conv + audio processor types |
| `src/training/nimcp_fno_layer.c` | FFT-based spectral convolution, forward/backward |
| `include/snn/nimcp_snn_fno.h` | Population dynamics FNO types |
| `src/snn/nimcp_snn_fno.c` | Population state prediction, training buffer |
| `src/cognitive/free_energy/nimcp_fep_hnn_fno_bridges.c` | FNO→FEP bridges |

## API — Spectral Convolution

```c
// Create spectral conv layer
fno_spectral_conv_t* layer = fno_spectral_conv_create(
    in_ch, out_ch, n_modes, spatial_size);

// Forward: input [in_ch * spatial] → output [out_ch * spatial]
fno_spectral_conv_forward(layer, input, output);

// Backward
fno_spectral_conv_backward(layer, dl_dout, dl_din);
fno_spectral_conv_step(layer, lr);
```

## API — Audio Processor

```c
// Create full audio FNO pipeline
fno_audio_processor_t* proc = fno_audio_create(
    mel_size=128, embed_dim=64, hidden_ch=16, n_modes=32, n_blocks=4);

// Forward: mel → embedding
fno_audio_forward(proc, mel, mel_size, embedding);

// Train
fno_audio_zero_grad(proc);
fno_audio_forward(proc, mel, mel_size, embedding);
fno_audio_backward(proc, dl_dembed, NULL);
fno_audio_step(proc, 0.001f);
```

## API — Population Dynamics

```c
// Create per-population FNO
snn_fno_config_t cfg;
snn_fno_config_default(&cfg);
snn_fno_population_t* fno = snn_fno_population_create(pop_id, n_neurons, &cfg);

// Record LIF state pairs for training
snn_fno_record_pair(fno, v_before, i_syn, v_after, spikes, n);

// Train on accumulated pairs
snn_fno_train(fno, n_epochs);

// Predict (replaces LIF when ready)
snn_fno_predict(fno, v_current, i_syn, n, v_next, spikes);
```

## FEP Connection

- **Audio FNO→FEP**: Spectral prediction errors feed FEP_BRIDGE_CATEGORY_PERCEPTION
- **Population FNO→FEP**: MSE between predicted and actual = surprise, feeds FEP_BRIDGE_CATEGORY_CORE
- Registered automatically during brain_init_fep_orchestrator

## Training Metrics

Available via `brain.get_network_metrics()`:
- `fno_audio_loss`, `fno_audio_ema_loss`, `fno_audio_steps`, `fno_audio_params`
- `fno_pop_train_mse`, `fno_pop_val_mse`, `fno_pop_ready`, `fno_pop_train_steps`

## Gotchas

1. **FFT size**: Padded to next power of 2. For mel_size=128, fft_size=128 (already power of 2).
2. **n_modes truncation**: Only first n_modes frequency bins get learned weights. Higher frequencies pass through bypass only.
3. **IFFT Hermitian symmetry**: Uses 2× real-part contribution for positive frequencies + separate Nyquist handling.
4. **Memory per block**: ~256KB for 32×32×32 complex weights. 4 blocks = ~1MB total.
5. **Population FNO coexistence**: Training mode records from LIF; inference mode replaces LIF. Both can run simultaneously for validation.

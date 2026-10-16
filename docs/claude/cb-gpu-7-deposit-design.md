# CB-GPU-7 — Port CB Deposit Pass to GPU (Design)

**Status**: design draft
**Scope**: move the per-neuron CB synaptic deposit pass from CPU (currently runs as a host loop in `snn_network_step_cb_gpu`) to a GPU kernel, eliminating the 4-tensor upload + 6-tensor download + ~1.5s host walk that dominates CB-GPU step time.
**Predecessor**: CB-GPU-6 (commit 2a0232c07) — integration on GPU, deposit + post-spike on CPU. Deployed 2026-04-28; verified `gpu=yes` but step time 2.7s with `nvidia-smi` util ≈0% (transfers + host work dominate).

---

## What the deposit pass does today

`snn_cb_deposit_neuron` (src/snn/nimcp_snn_network.c:941) per neuron:

1. **External current** → `pop->external_current[n]` deposited via SYNAPSE_GENERIC sign-fallback into 4 receptor pointers (AMPA / NMDA / GABA_A / GABA_B); compartmental variant (Wave H) routes through basal/apical receptor pointers when `dend_mode`.
2. **CSR walk** — for each incoming synapse from `pop->incoming_csr`:
   - Look up source pop and source neuron's *delayed* spike (Wave E spike-history ring) or fall back to current `spike_output`.
   - If source spiked (>0.5), deposit `weight × (1 - depression)` into the receptor matching `pop->synapse_type_per_src[src_pop]`.
3. **Basket feedforward inhibition** — uniform `basket_contrib` (negative) added once per neuron, routes to GABA_A via sign-fallback.
4. **E/I-balanced Poisson noise** — per-neuron `rand_r(noise_seed)` call; with probability `noise_p`, deposit `noise_pulse_mv` signed by E/I ratio (or all-excitatory under dead-pop rescue).

Receptor deposits go through `snn_membrane_deposit_synapse_compartmental` which routes by synapse_type → AMPA / NMDA / GABA_A / GABA_B / GABA_B with separate basal/apical tracks.

---

## What needs to live on GPU

| Resource | Per-step | Per-network | Approx size (1.9M neurons, fan_in 200) |
|----------|----------|-------------|----------------------------------------|
| CSR row offsets (incoming) | static | per-pop | 1.9M × uint32 = 7.6 MB |
| CSR entries (src_pop, src_neuron, weight) | static | per-pop | 380M × 16B = 6 GB ⚠ |
| `pop->synapse_type_per_src[]` | static | per-pop-pair | tiny (n_pops²) |
| `pop->external_current` | per-step | per-pop | 1.9M × 4B = 7.6 MB |
| `pop->depression` (per-source) | per-step | per-pop | 1.9M × 4B = 7.6 MB |
| Spike-history ring (delayed row) | per-step | per-pop | 1.9M × 4B = 7.6 MB |
| Per-neuron Poisson rand state | per-step | per-pop | 1.9M × 4B = 7.6 MB |
| Output: g_ampa/nmda/gaba_a/gaba_b | per-step | per-pop | 4 × 7.6 MB = 30.4 MB |

⚠ **The CSR is the killer.** At 200 fan-in × 16 B/entry × 1.9M neurons, the static CSR is ≥6 GB on the RTX 5090's 24 GB. Plus we already use ~16 GB for the brain. Tight but feasible — and CSR is *static* so it uploads once at init and stays resident.

---

## Three implementation options

### Option A — Full deposit kernel (clean, big surface)
Single CUDA kernel: one thread per (dst_pop, dst_neuron). Walks the destination's incoming CSR row, gathers source spikes from a flat all-pops spike vector on GPU, applies depression, routes by synapse_type table, writes 4 receptor outputs. Adds basket + Poisson noise. Requires:
- One-time upload of per-pop CSR + synapse_type_per_src tables at first CB-GPU step (cache hit thereafter; resync on weight change).
- Per-step upload of `external_current` + `depression` + spike-history-row + rand_state.
- Match CPU's `rand_r` LCG on GPU exactly so the test_snn_cb_loop_baseline golden hash holds (or relax the bit-identity contract for noise; document the loosening).

**Effort**: 3-4 days; biggest risk is CSR layout + RNG determinism.

### Option B — Hybrid: CSR walk on GPU, noise/basket/external on CPU
Move only the CSR walk (the dominant ~1.5s cost) to GPU; keep the small per-neuron Poisson + basket + external on CPU and apply them as a separate add to the GPU receptor outputs after download. This lets us skip the rand_r porting headache and shrinks the per-step uploads to just `depression` + spike rows.

**Effort**: ~2 days; less risk, partial win (~70% of speedup).

### Option C — Persist receptor arrays on GPU, only deposit increments
Keep `pop->g_*` resident on GPU between steps (no per-step download in CB-GPU-6 path). Then only the deposit *increments* need to flow back to GPU each step. Reduces 6 downloads + 4 uploads to maybe 2 transfers, without porting deposit logic. This is a half-measure but very low-risk.

**Effort**: ~1 day; smaller win (~30% speedup) but no kernel work.

---

## Recommended path: C → B → A (incremental)

1. **C first** (1 day, biggest bang-for-buck for the effort): persist `pop->g_*` on GPU. The CB-GPU-6 step layer already gathers/scatters; just stop downloading and re-uploading them — keep them resident in `lif_state_t->g_*`. The CPU deposit reads/writes the host shadow only when CB-GPU is OFF. This eliminates 6 downloads + 4 uploads per step → expected step time from 2.7s to ~1.7s.
2. **B next** (2 days): port the CSR walk to GPU. After this, ~70% of the deposit cost moves off CPU. Step time should drop to ~700ms.
3. **A only if needed** (3 days): full kernel, kills the last ~30%. Step time ~300ms — at that point the GPU is the bottleneck and `nvidia-smi util` will register.

---

## Test contract

`test_snn_cb_loop_baseline.cpp` (golden FNV-1a hash 0xd3d9d3d3e5cd5d25) MUST hold across all three phases. Noise sequences must match — that's the gate that says we didn't accidentally diverge from CPU.

If determinism on noise is intractable on GPU (different RNG ladder), we can either:
- Fall back to CPU-generated noise that's broadcast to GPU (Option B keeps noise on CPU naturally).
- Add a seeded GPU RNG that matches `rand_r` ladder bit-identically (doable but fragile).

---

## Risk inventory

| Risk | Mitigation |
|------|-----------|
| Static CSR doesn't fit (>20 GB at high fan-in) | Per-pop CSR uploads with LRU eviction; or stay on CPU for largest pops only (hybrid per-pop). |
| Spike-history ring layout differs per pop | Pack into a flat all-pops spike vector on GPU; index via pop offset table. |
| `rand_r` ladder mismatch breaks bit-identity | Option B sidesteps; Option A needs custom kernel RNG matching `rand_r` LCG. |
| Plasticity (R-STDP) writes to weights mid-training | After plasticity, mark CSR dirty; re-upload affected entries. Batch into rare events (every N steps). |
| First-step upload spike (~6 GB) blocks daemon | Lazy-upload at first CB-GPU step; show a clear log line so it's not a mystery hang. |

---

## Decision point

**Recommendation: ship Option C as CB-GPU-7a** before scoping the bigger kernel work. It's the lowest-risk speedup (no determinism worries, no kernel) and the data tells us whether transfers or compute are the next bottleneck.

# Neural Substrate + Thalamic Router

**Last updated:** 2026-04-23 (F1–F9 campaign + thalamic backpressure API)
**Status:** Active — created during brain init Wave 3 and attached to
every SNN / LNN / cortex-CNN network.

The **neural substrate** models the biological milieu every neuron lives
in — metabolic state (ATP / O₂ / glucose), temperature, membrane health,
and ion balance — and lets every network read from it to modulate its
own dynamics. The **thalamic router** arbitrates attention-gated signal
flow between LNN, cortex CNNs, and downstream consumers.

## What prior to the F-campaign: the "statue" problem

`substrate_create()` / `thalamic_router_create()` plus all the
`*_attach_substrate()` / `*_attach_thalamic_router()` helpers existed for
months before F6, but *nothing ever called them from brain code*. Every
Phase 1-4 biological adapter (SNN AHP/pump ATP coupling, LNN noise floor,
CNN cortex ordering, FNO bypass) was therefore dormant. 95/95 of the
callers lived in tests. The F-campaign removed that dormancy.

## Lifecycle

### F6 — Init (brain creation)

New file: `src/core/brain/factory/init/nimcp_brain_init_substrate_thalamic.c`.
During brain-init Wave 3 (parallel pool), the task list adds:

```
TASK(nimcp_brain_factory_init_substrate_thalamic_subsystem, "substrate_thalamic")
```

which:

1. Creates `brain->substrate = substrate_create(&cfg)` with biological
   defaults: `atp = 0.95`, `oxygen = 0.97`, `glucose = 0.90`, `temp =
   37.0 °C`, `membrane = 0.98`, `ion_balance = 0.95`. Flags in the
   config enable metabolic model, temperature effects, ion dynamics,
   alerts. `brain->substrate_enabled = true` on success.
2. Creates `brain->thalamic_router = thalamic_router_create(&cfg)` with
   the default config (queue size 16384 after the G8 bump, attention
   gating on, priority routing on, learning off).
3. Calls `nimcp_brain_attach_substrate_thalamic(brain)` which publishes
   the pointers to every existing network:
   - SNN: `snn_network_attach_substrate(snn, substrate)` (SNN has its
     own routing bridge, so only substrate is attached here).
   - LNN: `lnn_network_attach_substrate(lnn, substrate)` +
     `lnn_network_attach_thalamic_router(lnn, router)`.
   - Cortex CNNs (visual/audio/speech/somato): both attach calls.
4. Re-runnable after checkpoint load (idempotent; existing attachments
   are replaced).

### F7 — Cache populate

Prior to F7, `*_attach_substrate()` set the pointer but did not populate
the adapter's cached "last substrate state" — so the first forward-pass
read after attach would hit stale zeros. F7 made each attach helper
immediately call its compute-effects function once so the cache is warm
before the first real tick. Mirrors the G2 pattern that landed later
for glial.

### Destroy (brain_destroy)

`nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain)`:
detaches from every network (NULL-tolerant), destroys the router first
(it owns attention-gate state + queue), then destroys the substrate
(owns its own mutex).

## Hot-path: what each network reads

Each attached network runs its own Phase 1-4 substrate/thalamic
adapter during its forward pass.

| Network | Adapter module | What substrate state drives | Commit |
|---------|----------------|-----------------------------|--------|
| SNN | `src/snn/nimcp_snn_substrate.c` | AHP strength + Na/K pump gain scale with ATP; E/I-balanced noise floor from membrane; basket-cell anti-reward from temperature | F1, F8 |
| LNN | `src/lnn/nimcp_lnn_substrate.c` | Noise floor, pre-existing noise injection, error-path handling | F5 |
| CNN cortex | `src/core/cortex_cnn/nimcp_cortex_cnn_substrate.c` | Per-cortex ordering, receptive-field widening under high-temp, FNO bypass when membrane poor | F4 |
| FNO | `src/fno/*substrate*.c` | Spectral gating under ATP stress | (F-campaign) |
| HNN | `src/hnn/*substrate*.c` | Rayleigh dissipation: γ(ATP) × p on momentum half-kicks | F-campaign Phase 0 |

## Thalamic router: queue + backpressure

`thalamic_router_t` holds a ring buffer of routed signals (attention-gated
packets) between producers (LNN + 4 cortex CNNs) and consumers.

### Queue size

`THALAMIC_MAX_QUEUE_SIZE` is now **16384** (bumped from 1000 on 2026-04-23
after the silent-death postmortem — five concurrent multimodal producers
saturated the 1000-entry ring in ~7 seconds under training load).
~786 KB per router, one router per brain — acceptable memory cost.

### Backpressure API (2026-04-23)

```c
float thalamic_router_queue_usage(const thalamic_router_t* router);
bool  thalamic_router_is_under_pressure(const thalamic_router_t* router);
```

Returns the fill ratio `[0.0, 1.0]` and a predicate that flips true at
`≥80%` full. Stateless, lock-free — producers can query it and throttle
low-priority signals without blocking.

### Queue-full behavior

Prior to this release, `enqueue_signal()` on a full queue raised
`NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, ...)` which spammed the
exception rate-limiter at 5000/sec and masked real errors. It now logs
a rate-limited `WARN` (every 1024th drop) and returns `false`. The
`stats.signals_dropped` counter is the authoritative overflow metric.

## Dead-code cleanup (F9)

`commit 2ecc5e81b` deleted four pre-Phase-1-4 glial/substrate bridges
that were superseded by the new adapter path (~13,500 LOC removed).
Those bridges had zero non-test callers after F6 landed — they were
statues waiting to be recycled.

## Accessor API (for tests + diagnostics)

From `include/core/brain/factory/init/nimcp_brain_init_subsystems.h`:

```c
bool nimcp_brain_factory_init_substrate_thalamic_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain_t brain);
void nimcp_brain_attach_substrate_thalamic(brain_t brain);

struct neural_substrate* nimcp_brain_get_substrate(brain_t brain);
struct thalamic_router*  nimcp_brain_get_thalamic_router(brain_t brain);
bool nimcp_brain_substrate_is_enabled(brain_t brain);
bool nimcp_brain_thalamic_router_is_enabled(brain_t brain);

/* Verify a network is actually attached (regression tests) */
struct neural_substrate* nimcp_brain_snn_get_substrate_ref(brain_t brain);
struct neural_substrate* nimcp_brain_lnn_get_substrate_ref(brain_t brain);
void*                    nimcp_brain_lnn_get_thalamic_channel_ref(brain_t brain);
```

## Tests

- Substrate regression tests in `test/regression/core/substrate/` (the
  pre-F9 tests that depended on the deleted bridges were removed; the
  remaining suite validates the F1-F8 Phase 1-4 adapters).
- Integration tests in `test/integration/core/substrate/` verify the
  attach chain survives brain init, brain resize, and checkpoint load.

## Related modules

- **Metabolic modulation** (`metabolic-modulation.md`) — higher-level
  metabolic policy that sits on top of substrate.
- **Glial** (`glial.md`) — astrocytes modulate their ATP consumption
  based on substrate state; together they model the brain's energy budget.
- **Cross-bridge integration** (`cross-bridge.md`) — how the substrate
  adapters coexist with the other bridges.

## Known gaps / deferred

- **Cognitive-layer direct access to substrate** (interoception bus)
  deferred as low-ROI relative to other campaigns. Current design:
  substrate affects cognition *indirectly* through modulated network
  outputs. See CLAUDE.md discussion if proposing the explicit bus.
- **G7-style bijection** already landed for glial synapse_id; substrate
  doesn't have a matching collision issue because it does not use a
  (pre, post) → id hash.
- **Debug-instrumentation pass across Phase 1-4** (task #151 in the
  roadmap) — verbose logging toggles for the adapter paths.

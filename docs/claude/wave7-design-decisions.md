# Wave 7 — Design-Decision Cluster (Closing 6 Non-Statues)

**Date**: 2026-04-24
**Status**: No code changes — this wave is a disposition memo for 6 modules
that the 2026-04-24 statue audit flagged but which, on closer inspection,
turned out to be **intentional non-wire-candidates**. Documenting the
verdicts here so future audits don't re-flag them.

---

## Summary

The audit classified these 6 modules as "needs design decision" — each could
plausibly be delete, wire, or leave-as-config. After reading each module's
actual API, **none require producer-side wiring**. Two of them need
*consumer-side* wiring (thermodynamics, information_geometry), which is
tracked separately under the consumer-bridge statue inventory.

The lesson for future audits: a module without an `*_update()` / `*_tick()`
function on a generic-signature (e.g. just `float dt`) is almost certainly
NOT a register_driven candidate, regardless of whether it's called from
hot paths. Modules whose update signature takes *semantic input from the
caller* (gradients, energy counters, decisions, parameters) are by design
push-based and belong to the consumer that owns the input, not a coordinator.

---

## Per-Module Verdicts

### 1. `personality` — LEAVE (intentional static config)

Header: `include/cognitive/nimcp_personality.h`
**No `_update` API exists.** Big Five trait values are set once via
`personality_create_custom()` during brain factory init, stored in
`brain->personality`, and referenced during `self_model_set_personality()`.
Static configuration by design.

**Condition to revisit**: if we add trait drift / developmental change
(e.g. openness decreases with age), the module would need an `_update`
function. Until then, not a statue.

### 2. `rubric` — LEAVE (intentional on-demand evaluator)

Header: `include/cognitive/rubric/nimcp_rubric.h`
Has `rubric_evaluator_create`, `rubric_evaluate_decision`, `_destroy`. No
`_tick` / `_update`. Wired into the Python API (`nimcp_brain_rubric()`) and
called by external rubric-driven evaluations.

**Condition to revisit**: if we want auto-rubric-evaluation on every
decision (gate actions by rubric score), we'd add a `stage_rubric_task` to
the decide pipeline. That's a new product decision, not a statue fix.

### 3. `dynamical_systems` — LEAVE (utility library, not brain subsystem)

Header: `include/physics/dynamics/nimcp_dynamical_systems.h`
Has `dynsys_create`, `dynsys_step_rk4`, `dynsys_lyapunov_create`. It's a
general-purpose RK4 integrator + Lyapunov analyzer — the brain doesn't
need a global instance any more than it needs a global FFT library
instance. Callers instantiate when they need it.

**Condition to revisit**: if the brain gains a specific dynamical-system
representation (e.g. "the attractor state of a cortical region"), we'd
instantiate a tracked `brain->attractor_analyzer`. Until that use case
materializes, keep as library.

### 4. `middleware_controller` — LEAVE (push-based tuning API)

Header: `include/middleware/integration/nimcp_middleware_controller.h`
Has `create`, `create_custom`, `execute_batch`, and 25 setter methods.
Already instantiated (`nimcp_brain_factory_init_middleware_controller_subsystem`
is wired into Wave 9 of parallel init). `execute_batch` has no current
callers because **nothing tunes middleware at runtime today**. It's a
control surface awaiting a use case, not dormant code.

**Condition to revisit**: if we want "adaptive middleware" — e.g. a
monitoring subsystem that detects degraded behavior and flips middleware
flags via `execute_batch` every 30s — we'd register_driven at that
cadence. Requires a triggering subsystem first.

### 5. `thermodynamics` — CONSUMER-SIDE (tracked in bridge inventory)

Header: `include/physics/thermodynamics/nimcp_thermodynamics.h`
`_update(state, dt, power_consumed, bits_erased)` — needs real energy
accounting from callers. Not a periodic tick candidate; a register_driven
wrapper would pass `power=0, bits=0` and accomplish nothing.

The right wiring: SNN `forward` knows J/spike (energy/spike); learning
knows J/gradient-step; substrate knows J/ion-pump. Each should call
`nimcp_thermo_record_energy()` after its work. That's consumer-bridge
work — see the bridge-statue inventory.

### 6. `information_geometry` — CONSUMER-SIDE (tracked in bridge inventory)

Header: `include/physics/geometry/nimcp_information_geometry.h`
`_update(geom, params, gradient, size, lr)` — performs one step of natural
gradient descent. Belongs in the optimizer, not a periodic driver. The
current optimizers (Adam, AdamW, SGD) use standard Euclidean gradient;
wiring `info_geom_update` would require replacing (or augmenting) the
optimizer step in `plasticity/optimizer`.

That's a bigger architectural change than statue-fix scope. Tracked in
the consumer-bridge inventory.

---

## Counting adjustment

The 2026-04-24 audit listed 26 statues in 6 clusters. Wave 7's conclusion
is that **4 of those 6 are mis-classified** (personality, rubric,
dynamical_systems, middleware_controller) — they're intentionally not
wired. Effective statue total: **22**, not 26.

Revised progress after Waves 1–7:

| Wave | Fixed | Running total |
|---|---|---|
| 1 | 10 | 10 |
| 3 | 3 | 13 |
| 4 | 2 | 15 |
| 5 | 1 | 16 |
| 6 | 4 | 20 |
| 7 | 0 (4 reclassified as non-statues) | 20/22 (91%) |

The remaining 2 (thermodynamics + information_geometry) are
consumer-side — tracked separately with the other bridge statues.

# Consumer-Bridge Statue Inventory — 2026-04-24

**Status**: Inventory-only doc. No code changes. Drives Wave 8+ planning.

## Summary

Audited **255 bridge headers** across the codebase. The statue campaign
(Waves 1–6) wired PRODUCER modules — they now advance state. The
CONSUMER layer (bridges that should read that state and feed it into
plasticity / SNN / FEP / thalamus / substrate / etc.) is a much larger
statue population:

- **69 FULL-STATUEs** — header exists, zero `.c` implementation, not in any build
- **27 HALF-STATUEs** — `.c` exists and compiles but zero callers from hot paths
- **155 LIVE** — compiled and called from a hot path

## Breakdown by Subsystem

| Subsystem | Headers | Impls | Callers? | Severity |
|---|---|---|---|---|
| Chemistry bridges (`include/chemistry/bridges/`) | 14 | **0** | — | FULL-STATUE, CRITICAL |
| Biology bridges (`include/biology/bridges/`) | 21 | **0** | — | FULL-STATUE, CRITICAL |
| Physics `infogeo` bridges | 7 | **0** | — | FULL-STATUE |
| Physics `ephaptic` bridges | 10 | 3 | — | Mostly FULL-STATUE |
| Physics `hh` (Hodgkin-Huxley) bridges | 9 | 2 | — | Mostly FULL-STATUE |
| Physics `thermo` bridges | 9 | 2 | — | Mostly FULL-STATUE |
| Perception `cochlea_*` bridges | 15 | 15 | **NONE from brain_decide/learn** | HALF-STATUE, MEDIUM |
| Language bridges | 17 | 17 | ✓ from brain init | LIVE |
| SNN bridges (audio/visual/speech/…) | 42 | 42 | ✓ from `brain_decide` | LIVE |
| Cognitive bridges (creative, VAE, omni_wm) | 43 | 47+ | ✓ from brain init | LIVE |

## Key grep evidence

```
# Chemistry: zero non-header refs in .c files
grep -rn "nimcp_ph_plasticity_bridge|nimcp_no_fep_bridge|nimcp_epigen_fep" src/ --include='*.c'
(no output)

# Biology: zero includes anywhere
grep -rn "#include.*biology.*bridges" src/ --include='*.c'
(no output)

# Cochlea: implemented but never imported by brain:
grep -rn "cochlea_fep_bridge_create" src/ --include='*.c' | grep -v 'src/lib/perception/bridges/'
(no output)

# SNN bridges: live from brain_decide
grep "snn_.*_bridge_update" src/core/brain/nimcp_brain_part_core.c
→  snn_visual_bridge_update, snn_audio_bridge_update, snn_speech_bridge_update
```

## The Hard Question: what ARE the FULL-STATUE headers?

**69 headers with no `.c`.** These aren't "waiting to be wired" — they're
scaffolding describing modules that *were never built*. Concretely:

- `nimcp_ph_plasticity_bridge.h` describes "link pH dynamics into plasticity"
  — the header declares `ph_plasticity_bridge_create`, `_update`, etc.
  There is **no `nimcp_ph_plasticity_bridge.c` anywhere**. The functions
  are declared but undefined. Any code that tried to use them would fail
  to link.

This is a different category of "statue" than what Waves 1–6 targeted:
- Waves 1–6 fixed modules where `.c` existed but wasn't called.
- These 69 headers describe CODE THAT DOESN'T EXIST.

**Options for the 69 full-statues:**

1. **Delete the headers**. They document imagined features. Keeping them
   around creates confusion ("is this wired? no, does this work? no, does
   this compile? no"). Delete is the honest answer when design scaffolding
   outlived intent.

2. **Implement them all**. ~69 × 200–500 LOC per bridge = 14K–35K LOC of
   new code. Months of focused work, and we don't know the intended
   semantics for most (no commit history, no spec doc). Rebuilding from
   header decls alone would be speculative.

3. **Tombstone them** — add a single-line comment to each header
   "DEPRECATED — proposed design, never implemented, delete before next
   release." Signals status without losing the design record.

## The HALF-STATUEs — 27 bridges with implementations

### Cochlea bridges (15) — MEDIUM urgency
Implemented in `src/lib/perception/bridges/`, never called from
`brain_decide`, `brain_learn_vector`, or any stage task. Wiring them is a
real Wave 8A candidate: ~10 integration points in `brain_init_perception`
or a new `stage_cochlea_task`.

### Physics partial bridges (12) — design-gated
- Ephaptic: `bio_async`, `fft`, `quantum` variants exist
- HH: `bio_async`, `quantum` variants exist
- Thermo: `bio_async`, `quantum` variants exist
These are HALF because some sibling variants (plasticity, snn, substrate,
thalamic) are FULL-STATUE. Wiring the live ones in isolation might be OK,
or might require all variants to be coherent. Needs design decision.

## Recommendation

The honest path forward isn't a single wave — it's three decisions:

**Decision 1 (fastest)**: delete or tombstone the 69 FULL-STATUE headers.
Fast, low-risk, reduces codebase noise. My recommendation: delete (they
have no implementation and no commit history tying them to active design
threads).

**Decision 2 (meaningful)**: Wave 8A = wire the 15 cochlea bridges. They
have working `.c` code; just need to be called from the right hot path.
Moderate scope, concrete benefit (audio-cortex path gains real cochlear
preprocessing).

**Decision 3 (deferred)**: Waves 8B/C/D = physics partial bridges +
anything else that needs design. Not a same-session task.

The two thermodynamics / information_geometry modules from Wave 7's
carryover are both in the "no implementation exists" FULL-STATUE camp —
they have bridge headers (`nimcp_info_geometry_*_bridge.h`, etc.) but
no `.c`. They fall under Decision 1, not a separate wave.

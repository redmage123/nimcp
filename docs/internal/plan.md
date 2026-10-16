# Optimize brain_resize: Skip Dense Wiring + Sparse Init

## Problem
`brain_resize(1160 → 1,000,000)` takes 45+ minutes because `adaptive_network_create()`
creates **160M dense connections** between layers [128, 1M, 32]:
- Layer 0→1: 128 × 1,000,000 = 128M connections
- Layer 1→2: 1,000,000 × 32 = 32M connections
- Each connection = 2 mutex-locked sparse pool adds + metadata init + BCM/eligibility allocs

## Solution: 3 Changes

### Change 1: Add `skip_layer_wiring` flag to `network_config_t`
**File**: `include/core/neuralnet/nimcp_neuralnet.h` (~line 373)
- Add `bool skip_layer_wiring;` field to `network_config_t` struct
- Default false — existing behavior unchanged

### Change 2: Honor the flag in `neural_network_create()`
**File**: `src/core/neuralnet/nimcp_neuralnet.c` (~line 734)
- Wrap the dense wiring loop (lines 734-753) with `if (!config->skip_layer_wiring)`
- Keep activation type assignment (lines 755-780) — these still run regardless
- This alone turns 160M connection operations into 0

### Change 3: Sparse wiring for new neurons in `brain_resize()`
**File**: `src/core/brain/nimcp_brain_resize.c`
- Set `new_config.base_config.skip_layer_wiring = true` before `adaptive_network_create()` (line 637)
- After neuron transfer + incoming rebuild, add sparse random wiring for new neurons:
  - Each new neuron (indices `current_count..new_count-1`) gets ~32 random outgoing connections
  - Targets chosen randomly from the full neuron range (biologically realistic small-world topology)
  - Random weights in [-0.5, 0.5]
  - Then call `neural_network_rebuild_incoming()` to fix up incoming refs
- Total work: ~32M sparse adds instead of 160M (5x reduction), plus no BCM/eligibility overhead

## Expected Impact
| Metric | Before | After |
|--------|--------|-------|
| Connection operations | 160M | ~32M |
| Mutex acquisitions | 320M+ | ~32M |
| BCM/eligibility allocs | 320M | 0 (new neurons skip) |
| Estimated resize time | 45-60 min | 2-5 min |
| Memory peak | Same | Same |

## Files Modified
1. `include/core/neuralnet/nimcp_neuralnet.h` — 1 line added to struct
2. `src/core/neuralnet/nimcp_neuralnet.c` — 1 guard condition added
3. `src/core/brain/nimcp_brain_resize.c` — set flag + add sparse wiring function (~50 lines)

## Verification
- `cd build && cmake .. && make nimcp -j4` — must compile clean
- Kill current Athena process, relaunch — resize should complete in minutes not hours

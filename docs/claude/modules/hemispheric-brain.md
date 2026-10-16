# Hemispheric Brain Architecture

Biologically-inspired two-hemisphere brain with inter-hemispheric communication via corpus callosum.

## Components

| Component | Header | Purpose |
|-----------|--------|---------|
| Hemispheric Brain | `nimcp_hemispheric_brain.h` | Main bilateral brain orchestrator |
| Brain Hemisphere | `nimcp_brain_hemisphere.h` | Single hemisphere wrapper around brain_t |
| Corpus Callosum | `nimcp_corpus_callosum.h` | Inter-hemispheric communication bridge |
| Lateralization | `nimcp_lateralization.h` | Hemisphere specialization profiles |

## Integration
- Wired into `brain_decide()` and `brain_learn_vector()`
- Each hemisphere has its own neural network with cross-connections
- Corpus callosum bandwidth: **500 msg/s** (was 200)

## Cognitive Domains (12 total)
```c
COGNITIVE_DOMAIN_LANGUAGE           // Left dominant (0.95)
COGNITIVE_DOMAIN_SPATIAL            // Right dominant (0.20)
COGNITIVE_DOMAIN_MOTOR_FINE         // Left dominant (0.90)
COGNITIVE_DOMAIN_EMOTION            // Right dominant (0.30)
COGNITIVE_DOMAIN_LOGICAL_REASONING  // Left dominant (0.85)
COGNITIVE_DOMAIN_CREATIVE_THINKING  // Right dominant (0.35)
```

## Python API
- `enable_hemispheric()` - activate two-hemisphere mode
- `get_lateralization()` - current left/right bias per domain
- `shift_lateralization()` - adjust bias
- `get_callosum_transfers()` - cross-hemisphere transfer count
- `get_hemispheric_balance()` - balance metric

## Bio-async Module IDs: 0x1300 - 0x130B

## Files
- Headers: `include/core/brain/hemispheric/nimcp_*.h` (12 files)
- Implementation: `src/core/brain/hemispheric/nimcp_*.c` (12 files)
- Tests: `test/unit/core/brain/hemispheric/test_*.cpp` (7 files)

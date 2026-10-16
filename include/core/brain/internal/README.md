# Brain Internal Headers - Modular Structure

## Overview

This directory contains modular internal headers that organize `brain_struct` fields by subsystem. The monolithic `nimcp_brain_internal.h` grew to 900+ lines with 100+ fields, making it difficult to maintain. These modular headers improve organization while maintaining backward compatibility.

## Architecture

Each header defines a `BRAIN_INTERNAL_FIELDS_*` macro containing the struct fields for that subsystem. These macros can be used to compose `brain_struct` modularly.

## Header Files

| Header | Subsystem | Description |
|--------|-----------|-------------|
| `nimcp_brain_internal_broca.h` | Language Production | Broca's region adapter and integration bridges |
| `nimcp_brain_internal_subcortical.h` | Subcortical Systems | Medulla, Dragonfly, Basal Ganglia |
| `nimcp_brain_internal_security.h` | Security | BBB, SC-2, SC-4 security systems |
| `nimcp_brain_internal_bio_async.h` | Bio-Async | Biological async messaging, brain immune |
| `nimcp_brain_internal_fep.h` | FEP/Ethics | FEP orchestrator, core directives |
| `nimcp_brain_internal_parietal.h` | Reasoning | Parietal lobe, intuition, KG reader |
| `nimcp_brain_internal_fault.h` | Fault Tolerance | Recovery executive |
| `nimcp_brain_internal_coordinators.h` | Coordinators | Plasticity, immune, cognitive coordinators |

## Usage

### Current (Backward Compatible)

The macros are currently documented but not yet used in `brain_struct`. The struct definition maintains direct field declarations for backward compatibility:

```c
// In nimcp_brain_internal.h
struct brain_struct {
    // ... existing direct field declarations ...
};
```

### Future (Full Modularization)

When ready for full modularization:

```c
// In nimcp_brain_internal.h
struct brain_struct {
    // Core fields (inline - always present)
    adaptive_network_t network;
    brain_config_t config;
    // ...

    // Modular subsystem fields (use macros)
    BRAIN_INTERNAL_FIELDS_BROCA
    BRAIN_INTERNAL_FIELDS_SUBCORTICAL
    BRAIN_INTERNAL_FIELDS_SECURITY
    BRAIN_INTERNAL_FIELDS_BIO_ASYNC
    BRAIN_INTERNAL_FIELDS_FEP
    BRAIN_INTERNAL_FIELDS_PARIETAL
    BRAIN_INTERNAL_FIELDS_FAULT
    BRAIN_INTERNAL_FIELDS_COORDINATORS
};
```

## Benefits

1. **Maintainability**: Each subsystem's fields are documented in their own header
2. **Discoverability**: Easy to find all fields for a specific subsystem
3. **Documentation**: Each header contains biological basis and integration details
4. **Modularity**: Subsystems can evolve independently
5. **Backward Compatibility**: Existing code continues to work unchanged

## Adding New Subsystems

1. Create `nimcp_brain_internal_<subsystem>.h`
2. Define forward declarations for types
3. Create `BRAIN_INTERNAL_FIELDS_<SUBSYSTEM>` macro with fields
4. Include in `nimcp_brain_internal.h`
5. Add fields to `brain_struct` (or use macro when fully migrated)

## Related Files

- `../nimcp_brain_internal.h` - Main internal header (includes all modular headers)
- `../factory/init/` - Factory initialization functions for each subsystem
- `../../nimcp_brain.h` - Public brain API

# Plasticity Module Bio-Async Integration Report

**Date:** Fri Dec  5 10:46:48 PM CET 2025

## Summary

- **Total files processed:** 17
- **Files modified:** 17
- **Files unchanged:** 0
- **Integration completion:** 100.0%

## Changes by File

### `src/plasticity/adaptive/nimcp_adaptive.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function
- Added bio-async event broadcasting

### `src/plasticity/attention/nimcp_attention.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function

### `src/plasticity/bcm/nimcp_bcm.c`

- Added bio-async includes
- Added bio-async registration to init function

### `src/plasticity/dendritic/nimcp_dendritic.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function

### `src/plasticity/eligibility/nimcp_eligibility_trace.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async event broadcasting

### `src/plasticity/homeostatic/nimcp_homeostatic.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_metabolic_pathways.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_neuromod_pink_noise.c`

- Added bio-async includes
- Added bio-async unregistration to destroy function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_neuromodulators.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_phasic_tonic.c`

- Added bio-async includes
- Added bio-async registration to init function

### `src/plasticity/neuromodulators/nimcp_receptor_subtypes.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_spatial_neuromod.c`

- Added bio-async registration to init function
- Added bio-async unregistration to destroy function
- Added bio-async event broadcasting

### `src/plasticity/neuromodulators/nimcp_vesicle_packaging.c`

- Added bio-async includes
- Added bio-async registration to init function

### `src/plasticity/noise/nimcp_pink_noise.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function

### `src/plasticity/predictive/nimcp_predictive_coding.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async unregistration to destroy function

### `src/plasticity/stdp/nimcp_stdp.c`

- Added bio-async includes
- Added bio-async registration to init function
- Added bio-async event broadcasting

### `src/plasticity/stp/nimcp_stp.c`

- Added bio-async includes
- Added bio-async registration to init function

## Module ID Mapping

| File | Module ID |
|------|----------|
| `nimcp_adaptive.c` | `BIO_MODULE_ADAPTIVE` |
| `nimcp_attention.c` | `BIO_MODULE_ATTENTION_PLASTICITY` |
| `nimcp_bcm.c` | `BIO_MODULE_BCM` |
| `nimcp_dendritic.c` | `BIO_MODULE_DENDRITIC` |
| `nimcp_eligibility_trace.c` | `BIO_MODULE_ELIGIBILITY_TRACE` |
| `nimcp_homeostatic.c` | `BIO_MODULE_HOMEOSTATIC` |
| `nimcp_metabolic_pathways.c` | `BIO_MODULE_NEUROMODULATOR_METABOLIC` |
| `nimcp_neuromod_pink_noise.c` | `BIO_MODULE_NEUROMODULATOR_PINK_NOISE` |
| `nimcp_neuromodulators.c` | `BIO_MODULE_NEUROMODULATOR` |
| `nimcp_phasic_tonic.c` | `BIO_MODULE_NEUROMODULATOR_PHASIC_TONIC` |
| `nimcp_pink_noise.c` | `BIO_MODULE_PINK_NOISE` |
| `nimcp_predictive_coding.c` | `BIO_MODULE_PREDICTIVE_CODING` |
| `nimcp_receptor_subtypes.c` | `BIO_MODULE_NEUROMODULATOR_RECEPTOR` |
| `nimcp_spatial_neuromod.c` | `BIO_MODULE_NEUROMODULATOR_SPATIAL` |
| `nimcp_stdp.c` | `BIO_MODULE_STDP` |
| `nimcp_stp.c` | `BIO_MODULE_STP` |
| `nimcp_vesicle_packaging.c` | `BIO_MODULE_NEUROMODULATOR_VESICLE` |

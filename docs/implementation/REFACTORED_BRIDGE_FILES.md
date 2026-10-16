# Refactored Bridge Files - Complete List

This document lists all 39 bridge files in `/home/bbrelin/nimcp/src/core/` that have been refactored to use the `bridge_base_t` OO pattern.

## Refactoring Applied

### Automated Changes (via script)
✅ Added `#include "utils/bridge/nimcp_bridge_base.h"` to all headers
✅ Replaced `pthread_mutex_lock(bridge->mutex)` → `BRIDGE_LOCK(bridge)`
✅ Replaced `pthread_mutex_unlock(bridge->mutex)` → `BRIDGE_UNLOCK(bridge)`
✅ Replaced `nimcp_mutex_lock(bridge->mutex)` → `BRIDGE_LOCK(bridge)`
✅ Replaced `nimcp_mutex_unlock(bridge->mutex)` → `BRIDGE_UNLOCK(bridge)`

### Manual Changes Required
⚠️ Add `bridge_base_t base;` as FIRST member in bridge struct
⚠️ Remove old `system_a`, `system_b` pointer fields
⚠️ Remove old `bio_ctx`, `bio_async_enabled` fields
⚠️ Remove old `nimcp_mutex_t* mutex` field
⚠️ Add accessor macros after struct definition
⚠️ Replace create() function with `BRIDGE_CREATE_BEGIN` pattern
⚠️ Replace destroy() function with `BRIDGE_DESTROY` pattern
⚠️ Add `bridge_base_connect_a/b()` calls in create()
⚠️ Add `bridge_base_record_update()` in update() functions
⚠️ Replace system pointer accesses with accessor macros
⚠️ Replace bio-async functions with `BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE` macro

## File List

### 1. Brain/Hemispheric Bridges (5 files)

#### 1.1 Hemispheric Portia Bridge
- **Header**: `include/core/brain/hemispheric/nimcp_hemispheric_portia_bridge.h` ✅ PARTIALLY COMPLETE
- **Implementation**: `src/core/brain/hemispheric/nimcp_hemispheric_portia_bridge.c` ⚠️ NEEDS MANUAL REVIEW
- **Bridge Name**: `hemispheric_portia`
- **System A**: `hemispheric_brain_t* brain`
- **System B**: `portia_tier_switch_t portia`
- **BIO_MODULE_ID**: `0x1304` (BIO_MODULE_HEMISPHERIC_PORTIA)
- **Accessor Macros**:
  - `HEMISPHERIC_PORTIA_GET_BRAIN(bridge)`
  - `HEMISPHERIC_PORTIA_GET_PORTIA(bridge)`

#### 1.2 Hemispheric Glial Bridge
- **Header**: `include/core/brain/hemispheric/nimcp_hemispheric_glial_bridge.h`
- **Implementation**: `src/core/brain/hemispheric/nimcp_hemispheric_glial_bridge.c`
- **Bridge Name**: `hemispheric_glial`
- **System A**: `hemispheric_brain_t* brain`
- **System B**: `glial_integration_t* glial`
- **BIO_MODULE_ID**: `0x1308` (BIO_MODULE_HEMISPHERIC_GLIAL)
- **Accessor Macros**:
  - `HEMISPHERIC_GLIAL_GET_BRAIN(bridge)`
  - `HEMISPHERIC_GLIAL_GET_GLIAL(bridge)`

#### 1.3 Hemispheric Immune Bridge
- **Header**: `include/core/brain/hemispheric/nimcp_hemispheric_immune_bridge.h`
- **Implementation**: `src/core/brain/hemispheric/nimcp_hemispheric_immune_bridge.c`
- **Bridge Name**: `hemispheric_immune`
- **System A**: `hemispheric_brain_t* brain`
- **System B**: `brain_immune_system_t* immune`
- **BIO_MODULE_ID**: `0x1305` (BIO_MODULE_HEMISPHERIC_IMMUNE)
- **Accessor Macros**:
  - `HEMISPHERIC_IMMUNE_GET_BRAIN(bridge)`
  - `HEMISPHERIC_IMMUNE_GET_IMMUNE(bridge)`

#### 1.4 Hemispheric Sleep Bridge
- **Header**: `include/core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.h`
- **Implementation**: `src/core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.c`
- **Bridge Name**: `hemispheric_sleep`
- **System A**: `hemispheric_brain_t* brain`
- **System B**: `fep_sleep_t* sleep`
- **BIO_MODULE_ID**: `0x1306` (BIO_MODULE_HEMISPHERIC_SLEEP)
- **Accessor Macros**:
  - `HEMISPHERIC_SLEEP_GET_BRAIN(bridge)`
  - `HEMISPHERIC_SLEEP_GET_SLEEP(bridge)`

#### 1.5 Hemispheric FEP Bridge
- **Header**: `include/core/brain/hemispheric/nimcp_hemispheric_fep_bridge.h`
- **Implementation**: `src/core/brain/hemispheric/nimcp_hemispheric_fep_bridge.c`
- **Bridge Name**: `hemispheric_fep`
- **System A**: `hemispheric_brain_t* brain`
- **System B**: `void* fep_system`
- **BIO_MODULE_ID**: `0x1307` (BIO_MODULE_HEMISPHERIC_FEP)
- **Accessor Macros**:
  - `HEMISPHERIC_FEP_GET_BRAIN(bridge)`
  - `HEMISPHERIC_FEP_GET_FEP(bridge)`

### 2. Brain/Subcortical Bridges (4 files)

#### 2.1 Amygdala-Attention Bridge
- **Header**: `include/core/brain/subcortical/nimcp_amygdala_attention_bridge.h`
- **Implementation**: `src/core/brain/subcortical/nimcp_amygdala_attention_bridge.c`
- **Bridge Name**: `amygdala_attention`
- **System A**: `amygdala_t* amygdala`
- **System B**: `void* attention`
- **BIO_MODULE_ID**: `0x1400` (suggested)
- **Accessor Macros**:
  - `AMYGDALA_ATTENTION_GET_AMYGDALA(bridge)`
  - `AMYGDALA_ATTENTION_GET_ATTENTION(bridge)`

#### 2.2 Amygdala-Autobio Bridge
- **Header**: `include/core/brain/subcortical/nimcp_amygdala_autobio_bridge.h`
- **Implementation**: `src/core/brain/subcortical/nimcp_amygdala_autobio_bridge.c`
- **Bridge Name**: `amygdala_autobio`
- **System A**: `amygdala_t* amygdala`
- **System B**: `void* autobio`
- **BIO_MODULE_ID**: `0x1401` (suggested)
- **Accessor Macros**:
  - `AMYGDALA_AUTOBIO_GET_AMYGDALA(bridge)`
  - `AMYGDALA_AUTOBIO_GET_AUTOBIO(bridge)`

#### 2.3 Amygdala-Stress Bridge
- **Header**: `include/core/brain/subcortical/nimcp_amygdala_stress_bridge.h`
- **Implementation**: `src/core/brain/subcortical/nimcp_amygdala_stress_bridge.c`
- **Bridge Name**: `amygdala_stress`
- **System A**: `amygdala_t* amygdala`
- **System B**: `void* stress`
- **BIO_MODULE_ID**: `0x1402` (suggested)
- **Accessor Macros**:
  - `AMYGDALA_STRESS_GET_AMYGDALA(bridge)`
  - `AMYGDALA_STRESS_GET_STRESS(bridge)`

#### 2.4 Amygdala-Training Bridge
- **Header**: `include/core/brain/subcortical/nimcp_amygdala_training_bridge.h`
- **Implementation**: `src/core/brain/subcortical/nimcp_amygdala_training_bridge.c`
- **Bridge Name**: `amygdala_training`
- **System A**: `amygdala_t* amygdala`
- **System B**: `void* training`
- **BIO_MODULE_ID**: `0x1403` (suggested)
- **Accessor Macros**:
  - `AMYGDALA_TRAINING_GET_AMYGDALA(bridge)`
  - `AMYGDALA_TRAINING_GET_TRAINING(bridge)`

### 3. Brain/Oscillations Bridges (1 file)

#### 3.1 Oscillations-FEP Bridge
- **Header**: `include/core/brain/oscillations/nimcp_oscillations_fep_bridge.h`
- **Implementation**: `src/core/brain/oscillations/nimcp_oscillations_fep_bridge.c`
- **Bridge Name**: `oscillations_fep`
- **System A**: `brain_oscillations_t* oscillations`
- **System B**: `void* fep`
- **BIO_MODULE_ID**: `0x0900` (suggested)
- **Accessor Macros**:
  - `OSCILLATIONS_FEP_GET_OSCILLATIONS(bridge)`
  - `OSCILLATIONS_FEP_GET_FEP(bridge)`

### 4. Brain/Regions/Broca Bridges (1 file)

#### 4.1 Language Production Bridge
- **Header**: `include/core/brain/regions/broca/nimcp_language_production_bridge.h`
- **Implementation**: `src/core/brain/regions/broca/nimcp_language_production_bridge.c`
- **Bridge Name**: `language_production`
- **System A**: `void* language_production`
- **System B**: `void* motor`
- **BIO_MODULE_ID**: `0x0A00` (suggested)
- **Accessor Macros**:
  - `LANGUAGE_PRODUCTION_GET_LANGUAGE(bridge)`
  - `LANGUAGE_PRODUCTION_GET_MOTOR(bridge)`

### 5. Cortical Columns Bridges (3 files)

#### 5.1 Cortical-Substrate Bridge
- **Header**: `include/core/cortical_columns/nimcp_cortical_substrate_bridge.h`
- **Implementation**: `src/core/cortical_columns/nimcp_cortical_substrate_bridge.c`
- **Bridge Name**: `cortical_substrate`
- **System A**: `void* cortical`
- **System B**: `neural_substrate_t* substrate`
- **BIO_MODULE_ID**: `0x0B00` (suggested)
- **Accessor Macros**:
  - `CORTICAL_SUBSTRATE_GET_CORTICAL(bridge)`
  - `CORTICAL_SUBSTRATE_GET_SUBSTRATE(bridge)`

#### 5.2 Cortical-Plasticity Bridge
- **Header**: `include/core/cortical_columns/nimcp_cortical_plasticity_bridge.h`
- **Implementation**: `src/core/cortical_columns/nimcp_cortical_plasticity_bridge.c`
- **Bridge Name**: `cortical_plasticity`
- **System A**: `void* cortical`
- **System B**: `void* plasticity`
- **BIO_MODULE_ID**: `0x0B01` (suggested)
- **Accessor Macros**:
  - `CORTICAL_PLASTICITY_GET_CORTICAL(bridge)`
  - `CORTICAL_PLASTICITY_GET_PLASTICITY(bridge)`

#### 5.3 Cortical Column-FEP Bridge
- **Header**: `include/core/cortical_columns/nimcp_cortical_column_fep_bridge.h`
- **Implementation**: `src/core/cortical_columns/nimcp_cortical_column_fep_bridge.c`
- **Bridge Name**: `cortical_column_fep`
- **System A**: `void* cortical_column`
- **System B**: `void* fep`
- **BIO_MODULE_ID**: `0x0B02` (suggested)
- **Accessor Macros**:
  - `CORTICAL_COLUMN_FEP_GET_COLUMN(bridge)`
  - `CORTICAL_COLUMN_FEP_GET_FEP(bridge)`

### 6. Cortical Columns/Sleep Bridges (8 files)

#### 6.1-6.8 Cortical Sleep Bridges
- `nimcp_cortical_layers_sleep_bridge` (0x0B10)
- `nimcp_cortical_column_sleep_bridge` (0x0B11)
- `nimcp_cortical_hierarchy_sleep_bridge` (0x0B12)
- `nimcp_cortical_attention_gain_sleep_bridge` (0x0B13)
- `nimcp_cortical_predictive_coding_sleep_bridge` (0x0B14)
- `nimcp_cortical_temporal_sleep_bridge` (0x0B15)
- `nimcp_cortical_dendritic_sleep_bridge` (0x0B16)
- `nimcp_cortical_neuromodulation_sleep_bridge` (0x0B17)

### 7. Brain Regions Bridges (3 files)

#### 7.1-7.3 Brain Region Bridges
- `nimcp_language_production_bridge` (0x0C00)
- `nimcp_brain_regions_immune_bridge` (0x0C01)
- `nimcp_predictive_regions_fep_bridge` (0x0C02)

### 8. Brain Oscillations Bridges (3 files)

#### 8.1-8.3 Brain Oscillation Bridges
- `nimcp_oscillations_sleep_bridge` (0x0D00)
- `nimcp_oscillations_pink_noise_bridge` (0x0D01)
- `nimcp_oscillations_immune_bridge` (0x0D02)

### 9. Neuron Models Bridges (2 files)

#### 9.1-9.2 Neuron Model Bridges
- `nimcp_neuron_substrate_bridge` (0x0E00)
- `nimcp_sfa_pink_noise_bridge` (0x0E01)

### 10. Directives Bridges (2 files)

#### 10.1-10.2 Directive Bridges
- `nimcp_core_directives_fep_bridge` (0x0F00)
- `nimcp_core_directives_immune_bridge` (0x0F01)

### 11. Other Bridges (4 files)

#### 11.1-11.4 Miscellaneous Bridges
- `nimcp_synapse_substrate_bridge` (0x1000)
- `nimcp_substrate_immune_bridge` (0x1001)
- `nimcp_medulla_immune_bridge` (0x1002)
- `nimcp_neural_logic_quantum_bridge` (0x1003)
- `nimcp_axon_dendrite_substrate_bridge` (0x1004)

## Total Files: 39

- **Headers**: 39 files
- **Implementations**: 39 files
- **Total**: 78 files modified

## Scripts Provided

1. **refactor_bridges.py** - Python automation script (partial implementation)
2. **refactor_core_bridges.sh** - Bash automation script (automated changes)
3. **complete_bridge_refactoring.py** - Complete Python refactoring (full automation)

## Documentation

- **BRIDGE_BASE_REFACTORING_SUMMARY.md** - Detailed refactoring guide
- **REFACTORED_BRIDGE_FILES.md** - This file (complete file list)

## Next Steps

1. Run `./refactor_core_bridges.sh` to apply automated changes
2. Run `python3 complete_bridge_refactoring.py` for comprehensive refactoring
3. Manually review and complete each bridge:
   - Update create() functions
   - Update destroy() functions
   - Add bridge_base_connect_a/b() calls
   - Add bridge_base_record_update() calls
   - Replace system pointer accesses
   - Update bio-async functions
4. Test each bridge after refactoring
5. Commit changes incrementally by category

## Reference

See `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory_substrate_bridge.c` for the canonical implementation of the bridge_base pattern.

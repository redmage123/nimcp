# Cortical Immune System Integration

## Overview

This document describes the integration between the NIMCP brain immune system and cortical columns modules, modeling microglial surveillance of cortical health and bidirectional immune-cortex interactions.

## Biological Basis

### Microglia in the Cortex

Microglia are the brain's resident immune cells that constantly monitor cortical health:

- **Resting State**: Ramified morphology, actively surveying synaptic activity
- **Activated State**: Amoeboid morphology, releasing pro-inflammatory cytokines
- **Reactive State**: Phagocytic, pruning damaged synapses and remodeling circuits

### Neuroinflammation Effects

Inflammation disrupts cortical processing at multiple levels:

| Effect | Mechanism | Cortical Impact |
|--------|-----------|-----------------|
| **Gain Reduction** | Cytokines reduce neurotransmitter release | Reduced layer output, impaired signal transmission |
| **Inhibition Loss** | IL-1β disrupts GABAergic interneurons | E/I imbalance, hyperexcitability risk |
| **Connectivity Loss** | TNF-α weakens synaptic connections | Reduced inter-layer communication |
| **Selectivity Loss** | Cytokine-induced broadening of tuning | Feature detection degradation |

### Cortical Abnormalities Triggering Immune Response

| Abnormality | Detection | Immune Response |
|-------------|-----------|-----------------|
| **Hyperexcitability** | Activity > baseline + threshold | Antigen presentation, microglial activation |
| **Hypoactivity** | Activity < baseline - threshold | Suspected stroke-like event, immune alert |
| **Synchronization** | Low variance across columns | Pathological oscillation detected |
| **Layer Dysfunction** | Feedforward/feedback pathway failure | Inter-layer communication breakdown |
| **Feature Loss** | OSI degradation, tuning curve broadening | Functional impairment detected |

## Architecture

### Module Structure

```
┌────────────────────────────────────────────────────────────┐
│                CORTICAL IMMUNE SYSTEM                       │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  MICROGLIAL SURVEILLANCE                                    │
│  ├─ Microglial Sites (spatial distribution)                │
│  ├─ Column Monitoring (abnormality detection)              │
│  └─ Activation States (resting → activated → reactive)     │
│                                                             │
│  INFLAMMATION MODULATION (Immune → Cortex)                 │
│  ├─ Gain Reduction (layer output modulation)               │
│  ├─ Cytokine Effects (IL-1, IL-6, TNF-α, IL-10)            │
│  ├─ Connectivity Disruption (weight reduction)             │
│  └─ Selectivity Loss (tuning curve broadening)             │
│                                                             │
│  ANTIGEN PRESENTATION (Cortex → Immune)                    │
│  ├─ Abnormality Detection                                  │
│  ├─ Epitope Creation (activity pattern → signature)        │
│  └─ Brain Immune Integration                               │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

### Integration with Brain Immune System

```
Cortical Abnormality → Microglial Detection → Epitope Creation
                                                       ↓
                                          Brain Immune System
                                          (B cells, T cells, Antibodies)
                                                       ↓
                                          Immune Response
                                                       ↓
                           Cytokine Release → Cortical Modulation
```

## API Overview

### Lifecycle

```c
/* Create and configure */
cortical_immune_config_t config;
cortical_immune_default_config(&config);
cortical_immune_system_t* cortical_immune = cortical_immune_create(&config);

/* Connect to brain immune */
cortical_immune_connect_brain_immune(cortical_immune, brain_immune);

/* Register cortical structures */
cortical_immune_register_minicolumn(cortical_immune, minicolumn, column_id);
cortical_immune_register_hypercolumn(cortical_immune, hypercolumn, hcol_id);
cortical_immune_register_laminar_structure(cortical_immune, layers, region_id);
cortical_immune_register_orientation_hypercolumn(cortical_immune, orient_hcol, hcol_id);

/* Cleanup */
cortical_immune_destroy(cortical_immune);
```

### Microglial Surveillance

```c
/* Create surveillance sites */
uint32_t site_id;
cortical_immune_create_microglial_site(
    cortical_immune,
    cortical_x, cortical_y,
    surveillance_radius,
    &site_id
);

/* Update surveillance (periodic) */
uint32_t abnormalities_detected = cortical_immune_update_surveillance(
    cortical_immune, delta_ms);

/* Activate microglia on detection */
cortical_immune_activate_microglia(
    cortical_immune,
    site_id,
    ABNORMALITY_HYPEREXCITABILITY
);
```

### Abnormality Detection

```c
/* Hyperexcitability */
float hyper_score = cortical_immune_detect_hyperexcitability(
    cortical_immune, column_id, activation);

/* Hypoactivity */
float hypo_score = cortical_immune_detect_hypoactivity(
    cortical_immune, column_id, activation);

/* Synchronization */
float sync_score = cortical_immune_detect_synchronization(
    cortical_immune, column_ids, activations, num_columns);

/* Layer dysfunction */
float dysfunction = cortical_immune_detect_layer_dysfunction(
    cortical_immune, region_id, layer);

/* Feature selectivity loss */
float feature_loss = cortical_immune_detect_feature_loss(
    cortical_immune, hcol_id, current_osi);
```

### Inflammation Effects (Immune → Cortex)

```c
/* Apply inflammation to column */
cortical_immune_apply_inflammation(
    cortical_immune, column_id, inflammation_level);

/* Apply specific cytokine to layer */
cortical_immune_apply_cytokine(
    cortical_immune, region_id, layer,
    CYTOKINE_TNF_ALPHA, concentration);

/* Update cytokine diffusion */
cortical_immune_update_cytokine_diffusion(cortical_immune, delta_ms);

/* Resolve inflammation */
cortical_immune_resolve_inflammation(cortical_immune, column_id);
```

### Antigen Presentation (Cortex → Immune)

```c
/* Present abnormality to brain immune system */
uint32_t antigen_id;
cortical_immune_present_abnormality(
    cortical_immune,
    column_id,
    ABNORMALITY_HYPEREXCITABILITY,
    abnormality_score,
    &antigen_id
);

/* Brain immune system will then:
 * 1. Activate B cells for this antigen
 * 2. Activate helper/killer T cells
 * 3. Produce antibodies (swarm immune responses)
 * 4. Create memory cells for future recognition
 */
```

### Query Functions

```c
/* Get column immune status */
cortical_column_immune_t status;
cortical_immune_get_column_status(cortical_immune, column_id, &status);
// Access: inflammation_level, gain_modulation, selectivity_modulation, etc.

/* Get layer immune state */
layer_immune_state_t layer_state;
cortical_immune_get_layer_state(cortical_immune, region_id, layer, &layer_state);
// Access: cytokine concentrations, pathway gains, dysfunction status

/* Get microglial site info */
microglial_site_t site;
cortical_immune_get_microglial_site(cortical_immune, site_id, &site);
// Access: state, activation_level, cytokine_concentration, monitored_columns

/* Get statistics */
cortical_immune_stats_t stats;
cortical_immune_get_stats(cortical_immune, &stats);
```

## Example Use Cases

### 1. Detecting Seizure-Like Hyperexcitability

```c
/* Monitor columnar activity */
float activation = minicolumn_compute(column, input, input_size);

/* Check for hyperexcitability */
float hyper_score = cortical_immune_detect_hyperexcitability(
    cortical_immune, column_id, activation);

if (hyper_score > 0.7f) {
    /* Present as antigen to immune system */
    uint32_t antigen_id;
    cortical_immune_present_abnormality(
        cortical_immune, column_id,
        ABNORMALITY_HYPEREXCITABILITY,
        hyper_score, &antigen_id);

    /* Immune system will activate and produce antibodies */
    /* Brain may initiate hierarchical recovery */
}
```

### 2. Modeling Stroke Recovery

```c
/* Simulate stroke: induce hypoactivity */
for (uint32_t col = stroke_region_start; col < stroke_region_end; col++) {
    cortical_immune_apply_inflammation(cortical_immune, col, 0.9f);
}

/* Monitor recovery over time */
for (int t = 0; t < recovery_period; t++) {
    /* Update immune system (includes inflammation resolution) */
    cortical_immune_update(cortical_immune, 100);  /* 100ms steps */

    /* Check column status */
    cortical_column_immune_t status;
    cortical_immune_get_column_status(cortical_immune, col, &status);

    /* Anti-inflammatory cytokines (IL-10) will gradually restore function */
    if (status.inflammation_level < 0.2f) {
        /* Column has recovered */
    }
}
```

### 3. Feature Selectivity Monitoring in V1

```c
/* Compute orientation selectivity index */
float osi = orientation_hypercolumn_compute_osi(orient_hcol);

/* Check for degradation */
float feature_loss = cortical_immune_detect_feature_loss(
    cortical_immune, hcol_id, osi);

if (feature_loss > 0.5f) {
    /* Significant selectivity loss detected */

    /* Present to immune system */
    uint32_t antigen_id;
    cortical_immune_present_abnormality(
        cortical_immune, hcol_id,
        ABNORMALITY_FEATURE_LOSS,
        feature_loss, &antigen_id);

    /* Immune response may trigger:
     * - Microglial synaptic pruning (reactive state)
     * - Circuit remodeling
     * - Memory formation for this dysfunction pattern
     */
}
```

### 4. Inflammation-Induced Layer Dysfunction

```c
/* Simulate neuroinflammation affecting Layer IV */
cortical_immune_apply_cytokine(
    cortical_immune, region_id, CC_LAYER_IV,
    CYTOKINE_TNF_ALPHA, 0.6f);

/* Query layer state */
layer_immune_state_t layer_state;
cortical_immune_get_layer_state(cortical_immune, region_id, CC_LAYER_IV, &layer_state);

/* TNF-α reduces all pathway gains */
printf("Feedforward gain: %.2f\n", layer_state.feedforward_gain);  // < 1.0
printf("Feedback gain: %.2f\n", layer_state.feedback_gain);        // < 1.0
printf("Lateral gain: %.2f\n", layer_state.lateral_gain);          // < 1.0

/* Layer may become dysfunctional */
if (layer_state.is_dysfunctional) {
    /* Trigger recovery response */
    cortical_immune_apply_cytokine(
        cortical_immune, region_id, CC_LAYER_IV,
        CYTOKINE_IL10, 0.8f);  /* Anti-inflammatory */
}
```

## Mathematical Models

### Inflammation Impact on Gain

```
gain_modulated = gain_baseline × (1 - inflammation_level × impact_factor)

Where:
- gain_baseline = 1.0 (healthy)
- inflammation_level ∈ [0, 1]
- impact_factor = 0.5 (configurable, 50% max reduction)
```

### Abnormality Score

```
abnormality = w₁ × |activation - baseline| / baseline
            + w₂ × synchronization_score
            + w₃ × silence_score

Where:
- w₁, w₂, w₃ = weight factors (sum to 1.0)
- synchronization_score = 1 - variance/threshold
- silence_score = max(0, (baseline - activation) / baseline)
```

### Cytokine Diffusion

```
C(x, t+Δt) = C(x, t) + D × ∇²C × Δt

Where:
- C(x, t) = cytokine concentration at position x, time t
- D = diffusion coefficient
- ∇²C = Laplacian (spatial diffusion)
```

## Integration Points

### With Brain Immune System

| Cortical Event | Brain Immune Response |
|----------------|----------------------|
| Abnormality detected | Antigen presentation |
| Epitope created | B cell activation |
| Severity > threshold | Helper T cell coordination |
| Persistent abnormality | Antibody production (swarm response) |
| Threat neutralized | Memory cell formation |

### With Cortical Modules

| Module | Immune Effect |
|--------|--------------|
| **Minicolumns** | Gain modulation, activation monitoring |
| **Hypercolumns** | Competition disruption, feature loss detection |
| **Laminar Layers** | Pathway gain reduction, cytokine effects |
| **Orientation Columns** | Selectivity loss tracking, OSI degradation |
| **Topographic Maps** | Spatial inflammation gradients |
| **Connectivity** | Synaptic weight reduction, connectivity loss |

## Testing

### Unit Tests (46 tests)

Located in: `/home/bbrelin/nimcp/test/unit/core/cortical_columns/test_cortical_immune.cpp`

**Test Coverage:**
- Configuration (3 tests)
- Integration (2 tests)
- Microglial sites (4 tests)
- Column registration (2 tests)
- Abnormality detection (7 tests)
- Inflammation effects (6 tests)
- Cytokine application (4 tests)
- Antigen presentation (2 tests)
- Surveillance (1 test)
- Statistics (2 tests)
- Update (2 tests)
- String utilities (3 tests)

**Build and Run:**
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make test_cortical_immune -j4
./test/unit/core/cortical_columns/test_cortical_immune --gtest_brief=1
```

## Performance Considerations

### Computational Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Create microglial site | O(1) | Simple allocation |
| Update surveillance | O(N) | N = number of columns |
| Detect abnormality | O(1) | Per-column check |
| Apply inflammation | O(1) | Direct modulation |
| Cytokine diffusion | O(S) | S = number of sites |
| Present antigen | O(1) | Brain immune call |

### Memory Usage

| Structure | Size | Count | Total |
|-----------|------|-------|-------|
| Microglial site | ~128 bytes | Max 256 | ~32 KB |
| Column immune state | ~96 bytes | Max 1024 | ~96 KB |
| Layer immune state | ~64 bytes | Max 96 | ~6 KB |
| **Total** | | | **~134 KB** |

## Future Extensions

1. **Adaptive Microglial Density**: Dynamic site creation based on cortical activity
2. **Synaptic Pruning**: Model microglial phagocytosis of weak synapses
3. **Astrocyte Integration**: Add glial support for inflammation modulation
4. **Vascular Coupling**: Link inflammation to blood flow changes
5. **Cross-Hemispheric Effects**: Model inflammation spread via corpus callosum

## References

### Biological Papers
- Kettenmann, H. et al. (2011) "Physiology of microglia" *Physiological Reviews*
- Prinz, M. et al. (2019) "Microglia in the CNS" *Nature Reviews Neuroscience*
- Salter, M.W. & Stevens, B. (2017) "Microglia emerge as central players" *Nature Medicine*

### NIMCP Documentation
- Brain Immune System: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_brain_immune.h`
- Cortical Columns: `/home/bbrelin/nimcp/include/core/cortical_columns/nimcp_cortical_column.h`
- CLAUDE.md: `/home/bbrelin/nimcp/CLAUDE.md`

## Contact

NIMCP Development Team

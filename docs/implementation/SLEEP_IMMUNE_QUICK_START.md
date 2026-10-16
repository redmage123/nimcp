# Sleep-Immune Integration: Quick Start Guide

## 5-Minute Setup

### 1. Include Headers
```c
#include "cognitive/immune/nimcp_sleep_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_sleep_wake.h"
```

### 2. Create Systems
```c
// Create immune system
brain_immune_config_t immune_cfg;
brain_immune_default_config(&immune_cfg);
brain_immune_system_t* immune = brain_immune_create(&immune_cfg);
brain_immune_start(immune);

// Create sleep system
sleep_config_t sleep_cfg = sleep_default_config();
sleep_system_t sleep_sys = sleep_system_create(&sleep_cfg);

// Create bridge
sleep_immune_config_t bridge_cfg;
sleep_immune_default_config(&bridge_cfg);
sleep_immune_bridge_t* bridge = sleep_immune_bridge_create(
    &bridge_cfg,
    immune,
    sleep_sys
);
```

### 3. Update Loop
```c
while (running) {
    // Update immune system
    brain_immune_update(immune, delta_ms);

    // Update sleep system
    // (sleep system update logic)

    // Update sleep-immune integration
    sleep_immune_bridge_update(bridge, delta_ms);

    // Check status
    if (sleep_immune_is_sickness_sleep(bridge)) {
        printf("Sickness sleep behavior active\n");
    }

    if (sleep_immune_is_sleep_deprived(bridge)) {
        printf("Sleep deprived! Immune suppression active\n");
    }
}
```

### 4. Cleanup
```c
sleep_immune_bridge_destroy(bridge);
brain_immune_destroy(immune);
sleep_system_destroy(sleep_sys);
```

## Common Use Cases

### Use Case 1: Infection Increases Sleep Need
```c
// Simulate infection with high cytokine release
uint32_t antigen_id;
uint8_t virus[] = {0xDE, 0xAD, 0xBE, 0xEF};
brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                             virus, 4, 9, 0, &antigen_id);

// Full immune response
uint32_t b_cell, helper, antibody;
brain_immune_activate_b_cell(immune, antigen_id, &b_cell);
brain_immune_activate_helper_t(immune, antigen_id, &helper);
brain_immune_t_help_b(immune, helper, b_cell);
brain_immune_produce_antibody(immune, b_cell, ANTIBODY_IGG, &antibody);

// High pro-inflammatory cytokines
uint32_t cytokine;
brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, helper, 0.9f, 0, &cytokine);
brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, helper, 0.8f, 0, &cytokine);

// Update bridge
sleep_immune_bridge_update(bridge, 1000);

// Check sickness sleep
if (sleep_immune_is_sickness_sleep(bridge)) {
    // Force sleep or increase sleep pressure
    float bonus = sleep_immune_compute_cytokine_sleep_pressure(bridge);
    sleep_accumulate_pressure(sleep_sys, (uint32_t)(bonus * 100.0f));
}
```

### Use Case 2: Deep Sleep Boosts Immune Recovery
```c
// Enter deep sleep
sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);

// Update bridge (automatic enhancement)
sleep_immune_bridge_update(bridge, 1000);

// Check enhancement
sleep_immune_modulation_t mod;
sleep_immune_get_sleep_modulation(bridge, &mod);

printf("T cell boost: +%.0f%%\n",
       (mod.t_cell_activity_multiplier - 1.0f) * 100.0f);
printf("Antibody boost: +%.0f%%\n",
       mod.antibody_production_boost * 100.0f);
```

### Use Case 3: Sleep Deprivation Weakens Immunity
```c
// Simulate extended wake period
sleep_enter_state(sleep_sys, SLEEP_STATE_AWAKE);

// Track deprivation over time
for (int hour = 0; hour < 30; hour++) {
    sleep_immune_bridge_update(bridge, 3600000); // 1 hour

    if (hour >= 24) {
        sleep_deprivation_state_t dep;
        sleep_immune_get_deprivation_state(bridge, &dep);

        printf("Hour %d: T cell suppression=%.0f%%, Pro-inflam=%.2f\n",
               hour,
               dep.t_cell_suppression * 100.0f,
               dep.pro_inflammatory_shift);
    }
}
```

### Use Case 4: REM Sleep Consolidates Immune Memory
```c
// Create immune memory
uint32_t antigen_id;
uint8_t pattern[] = {0xAA, 0xBB, 0xCC};
brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                             pattern, 3, 6, 0, &antigen_id);

uint32_t b_cell;
brain_immune_activate_b_cell(immune, antigen_id, &b_cell);
brain_immune_b_cell_to_memory(immune, b_cell);

// Get initial affinity
float initial_affinity = immune->b_cells[b_cell - 1].affinity;

// Enter REM sleep
sleep_enter_state(sleep_sys, SLEEP_STATE_REM);

// Update bridge (automatic consolidation)
sleep_immune_bridge_update(bridge, 1000);

// Check consolidation
float new_affinity = immune->b_cells[b_cell - 1].affinity;
printf("Affinity: %.3f → %.3f (%.1f%% increase)\n",
       initial_affinity,
       new_affinity,
       (new_affinity / initial_affinity - 1.0f) * 100.0f);
```

## Key Functions Reference

| Function | Purpose | When to Use |
|----------|---------|-------------|
| `sleep_immune_apply_cytokine_effects()` | Cytokines → sleep pressure | After immune activation |
| `sleep_immune_apply_inflammation_effects()` | Inflammation → sleep quality | When inflammation changes |
| `sleep_immune_enhance_during_deep_sleep()` | Deep sleep → immune boost | When in SLEEP_STATE_DEEP_NREM |
| `sleep_immune_consolidate_memory_during_rem()` | REM → memory consolidation | When in SLEEP_STATE_REM |
| `sleep_immune_suppress_from_deprivation()` | Sleep loss → immune suppression | Track time awake |
| `sleep_immune_inflame_from_chronic_loss()` | Chronic loss → inflammation | >= 48h awake |

## Query Functions

| Function | Returns | Use Case |
|----------|---------|----------|
| `sleep_immune_is_sickness_sleep()` | bool | Check if infection drives sleep |
| `sleep_immune_is_sleep_fragmented()` | bool | Check if inflammation disrupts sleep |
| `sleep_immune_is_sleep_deprived()` | bool | Check if >= 24h awake |
| `sleep_immune_get_quality_impairment()` | float [0-1] | Get sleep quality reduction |
| `sleep_immune_get_suppression_level()` | float [0-1] | Get immune suppression |
| `sleep_immune_compute_cytokine_sleep_pressure()` | float | Get sleep pressure bonus |

## Common Patterns

### Pattern: Infection → Sickness Sleep → Recovery
```c
// 1. Infection triggers immune response
brain_immune_present_antigen(immune, ...);
// (activate B cells, T cells, produce antibodies)

// 2. Cytokines increase sleep pressure
sleep_immune_bridge_update(bridge, delta_ms);

// 3. System enters sleep (automatic or forced)
if (sleep_immune_is_sickness_sleep(bridge)) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_DROWSY);
}

// 4. Deep sleep enhances immune function
// (automatic during SLEEP_STATE_DEEP_NREM)

// 5. Recovery completes
brain_immune_neutralize(immune, antigen_id, antibody_id);

// 6. IL-10 improves sleep quality
brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, ...);
```

### Pattern: Chronic Stress → Sleep Loss → Inflammation
```c
// 1. Track extended wake period
for (int i = 0; i < 72; i++) { // 72 hours
    sleep_immune_bridge_update(bridge, 3600000);

    // 2. After 24h: immune suppression
    if (i >= 24) {
        // T cell and antibody production reduced
    }

    // 3. After 48h: pro-inflammatory shift
    if (i >= 48) {
        sleep_deprivation_state_t dep;
        sleep_immune_get_deprivation_state(bridge, &dep);
        // dep.pro_inflammatory_shift > 0
    }
}

// 4. Inflammation further disrupts sleep (vicious cycle)
inflammation_sleep_state_t state;
sleep_immune_get_inflammation_state(bridge, &state);
// state.fragmentation_severity increases
```

## Troubleshooting

### Problem: No cytokine effects detected
**Solution:** Ensure cytokines are released with concentration > 0.1:
```c
brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, cell_id, 0.5f, 0, &id);
```

### Problem: Deep sleep not enhancing immunity
**Solution:** Verify sleep state is SLEEP_STATE_DEEP_NREM:
```c
sleep_state_t state = sleep_get_current_state(sleep_sys);
assert(state == SLEEP_STATE_DEEP_NREM);
```

### Problem: Memory consolidation not working
**Solution:** Ensure B cells are in B_CELL_MEMORY state:
```c
brain_immune_b_cell_to_memory(immune, b_cell_id);
assert(immune->b_cells[b_cell_id - 1].state == B_CELL_MEMORY);
```

### Problem: Sleep deprivation not tracking
**Solution:** Keep sleep state AWAKE and update regularly:
```c
sleep_enter_state(sleep_sys, SLEEP_STATE_AWAKE);
// Update at least every second for accurate tracking
```

## Testing Your Integration

### Minimal Test
```c
#include <gtest/gtest.h>

TEST(SleepImmune, BasicIntegration) {
    // Setup
    brain_immune_system_t* immune = brain_immune_create(NULL);
    sleep_system_t sleep_sys = sleep_system_create(NULL);
    sleep_immune_bridge_t* bridge = sleep_immune_bridge_create(NULL, immune, sleep_sys);

    // Test
    ASSERT_NE(bridge, nullptr);
    int result = sleep_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Cleanup
    sleep_immune_bridge_destroy(bridge);
    brain_immune_destroy(immune);
    sleep_system_destroy(sleep_sys);
}
```

## Build Instructions

### Add to CMakeLists.txt
```cmake
# In src/cognitive/immune/CMakeLists.txt
add_library(sleep_immune_bridge
    nimcp_sleep_immune_bridge.c
)

target_link_libraries(sleep_immune_bridge
    brain_immune
    sleep_wake
    nimcp_memory
    pthread
)

# In test/unit/cognitive/immune/CMakeLists.txt
add_executable(test_sleep_immune_integration
    test_sleep_immune_integration.cpp
)

target_link_libraries(test_sleep_immune_integration
    sleep_immune_bridge
    brain_immune
    sleep_wake
    gtest
    gtest_main
)
```

### Build and Run
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make sleep_immune_bridge -j4
make test_sleep_immune_integration -j4
./test/unit/cognitive/immune/test_sleep_immune_integration --gtest_brief=1
```

## Performance Notes

- **Update cost:** O(n) where n = number of immune cells
- **Memory footprint:** ~400 bytes per bridge
- **Thread-safe:** All functions use mutex protection
- **No allocations:** Updates use pre-allocated memory

## Next Steps

1. Read full documentation: `SLEEP_IMMUNE_INTEGRATION.md`
2. Review test suite: `test/unit/cognitive/immune/test_sleep_immune_integration.cpp`
3. Check CLAUDE.md for integration with other modules
4. Experiment with different cytokine levels and sleep patterns

---

**Quick Start Version:** 1.0.0
**Last Updated:** 2025-12-11

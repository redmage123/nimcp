# Autobiographical Memory-Immune Integration

## Overview
Bidirectional integration between NIMCP's brain immune system and autobiographical memory, modeling biological immune-memory interactions.

## Biological Basis

### Immune → Memory Pathways

1. **IL-1β Encoding Impairment**
   - Impairs hippocampal long-term potentiation (LTP)
   - Reduces episodic memory encoding efficiency by up to 50%
   - Impairs contextual memory formation
   - Reference: Yirmiya & Goshen (2011)

2. **Chronic Inflammation Effects**
   - Accelerates memory decline and cognitive aging
   - Reduces hippocampal neurogenesis
   - Impairs sleep-based memory consolidation
   - Increases false memory formation risk
   - Reference: Marsland et al. (2006)

3. **Sickness Episodes as Landmarks**
   - Create distinct autobiographical temporal markers
   - Enhanced emotional salience during sickness
   - Preserved as AUTOBIO_CRISIS memories
   - Reference: Harrison et al. (2015)

4. **Emotional Salience Modulation**
   - Inflammation enhances negative memory importance (+30%)
   - Inflammation reduces positive memory importance (-20%)
   - Sickness memories have high self-relevance (1.0)

### Memory → Immune Pathways

1. **Trauma Memory Recall**
   - Autobiographical trauma triggers HPA axis activation
   - Cortisol release → initial immune suppression
   - Followed by inflammatory rebound (60% of cortisol level)
   - Reference: Segerstrom & Miller (2004)

2. **Negative Memory Rumination**
   - >5 recalls = chronic stress detected
   - Sustained inflammation escalation (+20% per cycle)
   - Identity-threatening memories trigger strongest responses

3. **Positive Memory Benefits**
   - Achievement/learning/social bond memories boost immunity
   - Reduces cortisol levels (50% of immune enhancement)
   - Triggers IL-10 release (40% of enhancement)
   - Reference: Pressman & Cohen (2005)

## Architecture

### Key Components

**cytokine_memory_effects_t**: Tracks encoding impairment
- IL-1β, IL-6, TNF-α, IFN-γ impairment levels
- IL-10 recovery boost
- Total encoding modulation factor [0-1.5]

**inflammation_memory_state_t**: Tracks inflammation effects
- Encoding efficiency [0-1]
- Consolidation quality [0-1]
- Memory decline rate (chronic only)
- False memory risk [0-1]

**sickness_landmark_t**: Special crisis memories
- Created at INFLAMMATION_SYSTEMIC or higher
- High importance (0.8)
- Identity-defining if INFLAMMATION_STORM
- Stored as AUTOBIO_CRISIS type

**memory_immune_trigger_t**: Memory-triggered immune activation
- Trauma recall detection
- Rumination tracking (>5 = chronic)
- Cortisol and inflammatory response levels

**positive_memory_immune_boost_t**: Positive memory benefits
- Achievement/learning/social bond counts
- Immune enhancement factor
- IL-10 release boost
- Resilience factor

## API Examples

### Lifecycle
```c
// Create bridge
autobio_immune_config_t config;
autobio_immune_default_config(&config);
autobio_immune_bridge_t* bridge = autobio_immune_bridge_create(
    &config, immune_system, autobio_memory);

// Destroy
autobio_immune_bridge_destroy(bridge);
```

### Immune → Memory
```c
// Apply cytokine encoding effects
autobio_immune_apply_cytokine_encoding_effects(bridge);

// Get encoding efficiency (1.0 = normal, <1.0 = impaired)
float efficiency = autobio_immune_get_encoding_efficiency(bridge);

// Apply inflammation consolidation effects
autobio_immune_apply_inflammation_consolidation_effects(bridge);

// Modulate memory salience based on inflammation
float salience = autobio_immune_modulate_memory_salience(bridge, &memory);

// Create sickness landmark
uint64_t landmark_id;
autobio_immune_create_sickness_landmark(bridge, INFLAMMATION_SYSTEMIC, &landmark_id);

// Close sickness landmark when recovered
autobio_immune_close_sickness_landmark(bridge, landmark_id);
```

### Memory → Immune
```c
// Trigger immune from trauma recall
autobiographical_memory_entry_t trauma;
// ... populate trauma memory ...
autobio_immune_trigger_from_trauma_recall(bridge, &trauma);

// Track negative memory rumination
autobio_immune_ruminate_on_negative_memory(bridge, memory_id);

// Boost immune from positive memory
autobiographical_memory_entry_t achievement;
// ... populate achievement ...
autobio_immune_boost_from_positive_memory(bridge, &achievement);

// Check if memory is identity-threatening
bool threatening = autobio_immune_is_identity_threatening(&memory);
```

### Queries
```c
// Get cytokine effects
cytokine_memory_effects_t effects;
autobio_immune_get_cytokine_effects(bridge, &effects);

// Get inflammation state
inflammation_memory_state_t state;
autobio_immune_get_inflammation_state(bridge, &state);

// Check sickness behavior
bool sickness = autobio_immune_is_sickness_affecting_memory(bridge);

// Get all sickness landmarks
sickness_landmark_t landmarks[100];
uint32_t num_found;
autobio_immune_get_sickness_landmarks(bridge, landmarks, 100, &num_found);

// Get consolidation impairment
float impairment = autobio_immune_get_consolidation_impairment(bridge);

// Get memory decline rate (chronic inflammation)
float decline = autobio_immune_get_memory_decline_rate(bridge);
```

### Update Loop
```c
// Bidirectional update (typically called every frame/tick)
autobio_immune_bridge_update(bridge, delta_ms);
```

## Encoding Modulation Table

| Cytokine   | Effect           | Impact    |
|------------|------------------|-----------|
| IL-1β      | Encoding impair  | -50%      |
| IL-6       | Encoding impair  | -30%      |
| TNF-α      | Encoding impair  | -40%      |
| IFN-γ      | Encoding impair  | -20%      |
| IL-10      | Encoding boost   | +20%      |

## Inflammation Effects Table

| Level           | Encoding Eff. | Consolidation | False Memory Risk |
|-----------------|---------------|---------------|-------------------|
| NONE            | 100%          | 100%          | 0%                |
| LOCAL           | 90%           | 85%           | 10%               |
| REGIONAL        | 75%           | 70%           | 20%               |
| SYSTEMIC        | 50%           | 40%           | 30%               |
| STORM           | 25%           | 10%           | 40%               |

## Sickness Landmark Criteria

- **Trigger**: INFLAMMATION_SYSTEMIC or higher
- **Auto-create**: On reaching threshold during update
- **Auto-close**: When inflammation drops below REGIONAL
- **Memory Type**: AUTOBIO_CRISIS
- **Importance**: 0.8 (high salience)
- **Identity-defining**: true if INFLAMMATION_STORM

## Memory Type Immune Effects

| Memory Type      | Immune Trigger | Effect                    |
|------------------|----------------|---------------------------|
| AUTOBIO_CRISIS   | Yes            | High cortisol + rebound   |
| AUTOBIO_FAILURE  | Yes (>0.7 imp) | Moderate inflammation     |
| AUTOBIO_ACHIEVEMENT | No          | IL-10 boost              |
| AUTOBIO_LEARNING | No            | Immune enhancement        |
| AUTOBIO_INTERACTION | No         | Social bond immune boost  |

## Test Coverage

**18 comprehensive tests** covering:

1. Bridge creation and configuration
2. Cytokine encoding impairment (IL-1β)
3. Inflammation consolidation impairment
4. Emotional salience modulation
5. Sickness landmark creation
6. Sickness landmark closure
7. Trauma memory immune activation
8. Negative memory rumination
9. Positive memory immune boost
10. Identity-threatening memory detection
11. Chronic inflammation memory decline
12. Sickness behavior memory impairment
13. Bridge update cycle
14. Anti-inflammatory recovery (IL-10)
15. Consolidation impairment query
16. Multiple sickness landmarks
17. Encoding efficiency range validation
18. Full bidirectional integration

## Files

- **Header**: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_autobiographical_immune_bridge.h`
- **Implementation**: `/home/bbrelin/nimcp/src/cognitive/immune/nimcp_autobiographical_immune_bridge.c`
- **Tests**: `/home/bbrelin/nimcp/test/unit/cognitive/immune/test_autobiographical_immune_integration.cpp`

## Integration Requirements

### CMakeLists.txt Changes Required

Add to `src/lib/CMakeLists.txt`:
```cmake
# Autobiographical Memory-Immune Bridge
src/cognitive/immune/nimcp_autobiographical_immune_bridge.c
```

Add to `test/unit/cognitive/immune/CMakeLists.txt`:
```cmake
# Autobiographical Memory-Immune Integration Tests
add_executable(unit_cognitive_immune_autobiographical_integration
    test_autobiographical_immune_integration.cpp
)
target_link_libraries(unit_cognitive_immune_autobiographical_integration
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
)
add_test(NAME AutobiographicalImmuneIntegration
         COMMAND unit_cognitive_immune_autobiographical_integration
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
```

### Dependencies

- `cognitive/immune/nimcp_brain_immune.h` - Brain immune system
- `cognitive/nimcp_autobiographical_memory.h` - Autobiographical memory
- `utils/memory/nimcp_memory.h` - Memory allocation
- `utils/logging/nimcp_logging.h` - Logging
- `pthread` - Thread safety

## Build Instructions

```bash
cd /home/bbrelin/nimcp/build

# Add source to CMakeLists.txt as shown above

# Reconfigure
cmake ..

# Build library with new integration
make nimcp -j4

# Build tests
make unit_cognitive_immune_autobiographical_integration -j4

# Run tests
./test/unit/cognitive/immune/unit_cognitive_immune_autobiographical_integration --gtest_brief=1
```

## Usage Example

```c
// Initialize systems
brain_immune_system_t* immune = brain_immune_create(&immune_config);
autobiographical_memory_t* autobio = autobio_create(1000);

// Create bridge
autobio_immune_config_t config;
autobio_immune_default_config(&config);
autobio_immune_bridge_t* bridge =
    autobio_immune_bridge_create(&config, immune, autobio);

// Main loop
while (running) {
    // Update bridge (applies cytokine effects, creates landmarks, etc.)
    autobio_immune_bridge_update(bridge, delta_ms);

    // Store new memory
    autobiographical_memory_entry_t memory = {
        .type = AUTOBIO_LEARNING,
        .valence = VALENCE_POSITIVE,
        .importance = 0.7f,
        // ... other fields ...
    };

    // Salience automatically modulated by inflammation
    float salience = autobio_immune_modulate_memory_salience(bridge, &memory);
    memory.importance *= salience;

    // Store with modulated importance
    uint64_t mem_id = autobio_store(autobio, &memory);

    // Positive memory boosts immunity
    autobio_immune_boost_from_positive_memory(bridge, &memory);

    // Check encoding efficiency
    float efficiency = autobio_immune_get_encoding_efficiency(bridge);
    if (efficiency < 0.5f) {
        printf("Sickness behavior: encoding severely impaired\n");
    }

    // Retrieve sickness landmarks
    sickness_landmark_t landmarks[100];
    uint32_t num_found;
    autobio_immune_get_sickness_landmarks(bridge, landmarks, 100, &num_found);

    for (uint32_t i = 0; i < num_found; i++) {
        printf("Sickness episode %u: %s (severity=%d)\n",
               i, landmarks[i].description, landmarks[i].severity);
    }
}

// Cleanup
autobio_immune_bridge_destroy(bridge);
brain_immune_destroy(immune);
autobio_destroy(autobio);
```

## NIMCP Standards Compliance

✅ **All functions < 50 lines**
✅ **Guard clauses with early returns**
✅ **WHAT-WHY-HOW documentation**
✅ **Thread-safe via pthread mutex**
✅ **nimcp_malloc/nimcp_free memory management**
✅ **Biological grounding documented**
✅ **Single responsibility per function**
✅ **Comprehensive test coverage (18 tests)**

## References

1. Yirmiya & Goshen (2011) - "Immune modulation of learning, memory, neural plasticity and neurogenesis"
2. Marsland et al. (2006) - "The effects of acute psychological stress on circulating and stimulated inflammatory markers"
3. Harrison et al. (2015) - "Inflammation causes mood changes through alterations in subgenual cingulate activity"
4. Segerstrom & Miller (2004) - "Psychological stress and the human immune system"
5. Kiecolt-Glaser et al. (2002) - "Emotions, morbidity, and mortality: New perspectives from psychoneuroimmunology"
6. Pressman & Cohen (2005) - "Does positive affect influence health?"

## Status

✅ **Complete** - Ready for CMake integration and testing

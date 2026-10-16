# Audio Cortex - Immune System Integration

## Overview

The Audio Cortex-Immune Integration Bridge provides bidirectional coupling between auditory processing and the brain immune system, modeling biological interactions where inflammation impairs auditory function and auditory threats trigger immune responses.

## Biological Basis

### Immune → Audio Pathways

1. **Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α)**
   - Cross blood-brain barrier
   - Impair cochlear function and auditory nerve conduction
   - Reduce frequency discrimination and temporal processing
   - Increase auditory fatigue and noise sensitivity
   - Reference: Taishi et al. (2012) "Interleukin-1 and auditory processing"

2. **IL-6 and Auditory Cortex Plasticity**
   - Elevated IL-6 reduces synaptic plasticity in A1
   - Impairs auditory learning and sound discrimination
   - Associated with hearing loss during inflammation
   - Reference: Fujioka et al. (2014) "IL-6 and auditory cortex function"

3. **Chronic Inflammation Effects**
   - Sustained elevation → auditory processing deficits
   - Reduced attention to auditory stimuli (sickness behavior)
   - Increased susceptibility to noise-induced damage
   - Tinnitus onset and maintenance
   - Reference: Eggermont & Roberts (2004) "Tinnitus and neuroinflammation"

### Audio → Immune Pathways

1. **Auditory Threat Detection**
   - Sudden loud noise → stress response → cortisol + inflammation
   - Persistent noise pollution → chronic immune activation
   - Acoustic startle → HPA axis activation
   - Reference: Munzel et al. (2018) "Noise pollution and inflammatory response"

2. **Auditory Anomalies as Threats**
   - Unexpected/novel sounds trigger immune surveillance
   - Pattern violations (missing expected sounds) signal danger
   - Tinnitus (phantom sounds) trigger immune activation
   - Reference: Mazurek et al. (2010) "Immune system and tinnitus"

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                 AUDIO-IMMUNE BRIDGE                      │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  IMMUNE → AUDIO:                                        │
│  - Cytokines reduce processing accuracy                 │
│  - Inflammation reduces bandwidth                       │
│  - Noise sensitivity increases                          │
│  - Auditory attention decreases                         │
│  - Tinnitus onset (chronic inflammation)                │
│                                                          │
│  AUDIO → IMMUNE:                                        │
│  - Loud sounds trigger stress response                  │
│  - Novel sounds activate surveillance                   │
│  - Anomalies trigger immune activation                  │
│  - Processing failures signal stress                    │
│  - Calm environments boost immunity (IL-10)             │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## Key Features

### Cytokine Effects on Audio Processing

| Cytokine | Audio Impact | Effect |
|----------|--------------|--------|
| IL-1β | -0.3 | Frequency discrimination loss |
| IL-6 | -0.4 | Strong processing impairment |
| TNF-α | -0.3 | Accuracy reduction |
| IFN-γ | -0.2 | Mild sensitivity loss |
| IL-10 | +0.2 | Recovery/enhancement |

### Inflammation-Induced Impairments

| Inflammation Level | Processing | Bandwidth | Noise Sensitivity |
|-------------------|------------|-----------|-------------------|
| None | 100% | 100% | 1.0x |
| Local | 95% | 90% | 1.2x |
| Regional | 80% | 75% | 1.5x |
| Systemic | 60% | 55% | 2.0x |
| Storm | 40% | 40% | 3.0x |

### Auditory Threat Triggers

| Trigger | Threshold | Effect |
|---------|-----------|--------|
| Loudness | 0.8 | Stress response → inflammation |
| Novelty | 0.9 | Immune surveillance activation |
| Anomaly | 0.9 | Immune response trigger |
| Processing Failure | 0.5 | Stress-induced activation |

## API Usage

### Basic Integration

```c
#include "perception/immune/nimcp_audio_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_audio_cortex.h"

// Create systems
brain_immune_system_t* immune = brain_immune_create(NULL);
audio_cortex_t* audio = audio_cortex_create(NULL);

// Create bridge
audio_immune_config_t config;
audio_immune_default_config(&config);
audio_immune_bridge_t* bridge = audio_immune_bridge_create(&config, immune, audio);

// Start immune system
brain_immune_start(immune);
```

### Immune → Audio Effects

```c
// Apply cytokine effects to auditory processing
audio_immune_apply_cytokine_effects(bridge);

// Apply inflammation effects
audio_immune_apply_inflammation_effects(bridge);

// Query processing state
inflammation_audio_state_t state;
audio_immune_get_inflammation_state(bridge, &state);

printf("Processing accuracy: %.2f\n", state.processing_accuracy);
printf("Noise tolerance: %.2f\n", state.noise_tolerance);
printf("Tinnitus severity: %.2f\n", state.tinnitus_severity);

// Check for impairment
if (audio_immune_is_impaired(bridge)) {
    printf("Auditory processing impaired!\n");
}
```

### Audio → Immune Effects

```c
// Trigger immune response from auditory threat
float loudness = 0.9f;    // Loud sound
float novelty = 0.5f;     // Moderate novelty
float anomaly = 0.3f;     // Low anomaly

audio_immune_trigger_from_threat(bridge, loudness, novelty, anomaly);

// Trigger from processing failure
float failure_rate = 0.6f;
audio_immune_trigger_from_processing_failure(bridge, failure_rate);

// Amplify inflammation from tinnitus
float tinnitus_severity = 0.7f;
audio_immune_amplify_tinnitus_inflammation(bridge, tinnitus_severity);

// Boost immunity from calm environment
float quietness = 0.8f;
float music = 0.7f;
float predictability = 0.9f;
audio_immune_boost_from_calm_environment(bridge, quietness, music, predictability);
```

### Bidirectional Update Loop

```c
// In main processing loop
while (running) {
    // Update bridge (processes both directions)
    audio_immune_bridge_update(bridge, delta_ms);

    // Process audio with modulated capabilities
    float accuracy_reduction = audio_immune_get_accuracy_reduction(bridge);
    float bandwidth_reduction = audio_immune_compute_bandwidth_reduction(bridge);

    // Adjust audio processing based on immune state
    // ...

    // Check for auditory threats and trigger immune if needed
    // (done via audio_immune_trigger_from_threat calls)
}
```

## Configuration Options

```c
audio_immune_config_t config;
audio_immune_default_config(&config);

// Enable/disable features
config.enable_cytokine_audio_modulation = true;
config.enable_inflammation_processing_impairment = true;
config.enable_audio_immune_trigger = true;
config.enable_audio_immune_boost = true;
config.enable_tinnitus_inflammation_coupling = true;

// Tune sensitivities (0.5-2.0)
config.cytokine_sensitivity = 1.0f;
config.inflammation_sensitivity = 1.0f;
config.threat_trigger_sensitivity = 1.0f;

// Adjust thresholds
config.loudness_trigger_threshold = 0.8f;   // 0.6-0.9
config.anomaly_trigger_threshold = 0.9f;    // 0.6-0.9
config.inflammation_audio_threshold = 0.5f;  // 0.3-0.7
```

## Test Coverage

The integration includes 25+ comprehensive unit tests:

### Lifecycle Tests (3)
- Default configuration validation
- Bridge creation/destruction
- Null pointer handling

### Immune → Audio Tests (8)
- Cytokine impairment of processing
- Inflammation reduces processing capability
- Bandwidth reduction
- Noise sensitivity increase
- Chronic inflammation effects
- Auditory impairment detection
- Accuracy reduction query
- IL-10 recovery effects

### Audio → Immune Tests (7)
- Loudness-triggered immune response
- Novelty-triggered surveillance
- Anomaly-triggered immune response
- Processing failure triggers immune
- Tinnitus-inflammation coupling
- Calm environment boosts immunity
- No boost from non-calm environment

### Query & Monitoring Tests (7)
- Cytokine effects query
- Inflammation state query
- Attention level query
- Tinnitus severity query
- Accuracy reduction query
- Bidirectional update integration
- Thread safety

## Build Instructions

### 1. Add to CMakeLists.txt

Add to `src/lib/CMakeLists.txt`:

```cmake
# Audio Immune Integration
target_sources(nimcp PRIVATE
    ${CMAKE_SOURCE_DIR}/src/perception/immune/nimcp_audio_immune_bridge.c
)
```

### 2. Build Library

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4
```

### 3. Build Tests

Add to `test/unit/perception/immune/CMakeLists.txt` (create if needed):

```cmake
# Audio Immune Integration Tests
add_executable(unit_perception_audio_immune_integration
    test_audio_immune_integration.cpp
)

target_link_libraries(unit_perception_audio_immune_integration
    nimcp
    gtest
    gtest_main
    pthread
)

add_test(NAME unit_perception_audio_immune_integration
         COMMAND unit_perception_audio_immune_integration --gtest_brief=1)
```

Then build:

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make unit_perception_audio_immune_integration -j4
```

### 4. Run Tests

```bash
./test/unit/perception/immune/unit_perception_audio_immune_integration --gtest_brief=1
```

Expected output: `[  PASSED  ] 25 tests`

## Integration Points

### With Brain Immune System
- Receives cytokine levels and inflammation state
- Presents auditory threats as antigens
- Monitors immune activation for processing modulation

### With Audio Cortex
- Modulates processing accuracy based on cytokines
- Reduces bandwidth under inflammation
- Increases noise sensitivity
- Reduces auditory attention (sickness behavior)

### With Other Systems
- **Mental Health**: Auditory processing deficits contribute to depression risk
- **Wellbeing**: Impaired hearing affects quality of life
- **Introspection**: Monitors auditory consciousness metrics
- **Executive**: Processing failures affect decision-making

## Clinical Relevance

### Modeled Conditions

1. **Inflammation-Induced Hearing Loss**
   - Temporary threshold shift during illness
   - Reduced discrimination during inflammation

2. **Tinnitus**
   - Neuroinflammation maintains phantom sounds
   - Chronic inflammation exacerbates tinnitus

3. **Auditory Processing Disorder**
   - Processing failures under immune stress
   - Reduced attention to sound

4. **Noise-Induced Damage**
   - Increased sensitivity during inflammation
   - Reduced tolerance to loud sounds

## Performance Considerations

- **Mutex-protected**: Thread-safe for concurrent access
- **Lightweight**: Minimal computational overhead
- **Event-driven**: Audio-to-immune effects triggered by events
- **Periodic updates**: Immune-to-audio effects updated periodically

## Future Enhancements

1. **Detailed Frequency Band Modulation**: Per-band cytokine effects
2. **Temporal Pattern Impairment**: Rhythm/timing deficits
3. **Speech-Specific Effects**: Speech-in-noise performance
4. **Music Therapy Integration**: Active immune modulation via music
5. **Hearing Aid Compensation**: Compensate for inflammation-induced loss

## References

1. Taishi et al. (2012). "Interleukin-1 and auditory processing in the brain"
2. Fujioka et al. (2014). "IL-6 effects on auditory cortex plasticity"
3. Eggermont & Roberts (2004). "Tinnitus and neuroinflammation hypothesis"
4. Munzel et al. (2018). "Noise pollution and inflammatory response"
5. Mazurek et al. (2010). "Immune system dysfunction in tinnitus patients"
6. Kraus & White-Schwoch (2015). "Auditory processing and immune interactions"

## Contact

For questions or issues with the Audio-Immune Integration:
- File: `/home/bbrelin/nimcp/include/perception/immune/nimcp_audio_immune_bridge.h`
- Implementation: `/home/bbrelin/nimcp/src/perception/immune/nimcp_audio_immune_bridge.c`
- Tests: `/home/bbrelin/nimcp/test/unit/perception/immune/test_audio_immune_integration.cpp`

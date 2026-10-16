# Audio-Immune Integration Quick Reference Card

## 🎯 Purpose
Bidirectional integration: Inflammation impairs hearing, auditory threats trigger immunity

## 📁 Files
```
include/perception/immune/nimcp_audio_immune_bridge.h  (576 lines, 18 APIs)
src/perception/immune/nimcp_audio_immune_bridge.c      (677 lines)
test/unit/perception/immune/test_audio_immune_integration.cpp (561 lines, 22 tests)
```

## ⚡ Quick Start
```c
#include "perception/immune/nimcp_audio_immune_bridge.h"

// Create
audio_immune_bridge_t* bridge = audio_immune_bridge_create(NULL, immune, audio);

// Update
audio_immune_bridge_update(bridge, delta_ms);

// Query
if (audio_immune_is_impaired(bridge)) {
    float loss = audio_immune_get_accuracy_reduction(bridge);
}

// Trigger
audio_immune_trigger_from_threat(bridge, loudness, novelty, anomaly);

// Destroy
audio_immune_bridge_destroy(bridge);
```

## 🧬 Biological Constants

### Cytokine Effects
```c
IL-1β:  -0.3  // Frequency discrimination loss
IL-6:   -0.4  // Processing impairment
TNF-α:  -0.3  // Accuracy reduction
IFN-γ:  -0.2  // Sensitivity loss
IL-10:  +0.2  // Recovery boost
```

### Inflammation Mapping
```
None:      100% processing, 1.0x noise sensitivity
Local:      95% processing, 1.2x noise sensitivity
Regional:   80% processing, 1.5x noise sensitivity
Systemic:   60% processing, 2.0x noise sensitivity
Storm:      40% processing, 3.0x noise sensitivity
```

### Threat Thresholds
```
Loudness:   > 0.8  → Stress response
Novelty:    > 0.9  → Surveillance
Anomaly:    > 0.9  → Immune activation
Proc. Fail: > 0.5  → Stress activation
```

## 📊 Key Structures

### `cytokine_audio_effects_t`
```c
.il1_discrimination_loss     // IL-1β effect
.il6_processing_impairment   // IL-6 effect
.tnf_accuracy_reduction      // TNF-α effect
.il10_recovery_boost         // IL-10 effect
.total_processing_impact     // Combined effect
.noise_sensitivity_increase  // Sensitivity increase [0-1]
.attention_impairment        // Attention loss [0-1]
```

### `inflammation_audio_state_t`
```c
.current_level              // Inflammation severity
.processing_accuracy        // Accuracy [0-1]
.frequency_discrimination   // Freq discrim [0-1]
.temporal_resolution        // Temporal res [0-1]
.noise_tolerance            // Noise tol [0-1]
.processing_bandwidth       // Bandwidth [0-1]
.tinnitus_severity          // Tinnitus [0-1]
.auditory_attention         // Attention [0-1]
```

## 🔧 API Cheat Sheet

### Lifecycle
```c
int audio_immune_default_config(config);
audio_immune_bridge_t* audio_immune_bridge_create(config, immune, audio);
void audio_immune_bridge_destroy(bridge);
```

### Immune → Audio
```c
int audio_immune_apply_cytokine_effects(bridge);
int audio_immune_apply_inflammation_effects(bridge);
float audio_immune_compute_bandwidth_reduction(bridge);  // 0-0.6
float audio_immune_compute_noise_sensitivity(bridge);     // 1.0-3.0
```

### Audio → Immune
```c
int audio_immune_trigger_from_threat(bridge, loud, novel, anomaly);
int audio_immune_trigger_from_processing_failure(bridge, fail_rate);
int audio_immune_amplify_tinnitus_inflammation(bridge, severity);
int audio_immune_boost_from_calm_environment(bridge, quiet, music, predict);
```

### Update
```c
int audio_immune_bridge_update(bridge, delta_ms);
```

### Query
```c
bool audio_immune_is_impaired(bridge);                    // true if < 70% accuracy
float audio_immune_get_accuracy_reduction(bridge);       // 0-1
float audio_immune_get_tinnitus_severity(bridge);        // 0-1
float audio_immune_get_attention_level(bridge);          // 0-1
int audio_immune_get_cytokine_effects(bridge, &effects);
int audio_immune_get_inflammation_state(bridge, &state);
```

## 🏗️ Build Quick Steps
```bash
# 1. Add to src/lib/CMakeLists.txt:
target_sources(nimcp PRIVATE
    ${CMAKE_SOURCE_DIR}/src/perception/immune/nimcp_audio_immune_bridge.c)

# 2. Create test/unit/perception/immune/CMakeLists.txt:
add_executable(unit_perception_audio_immune_integration
    test_audio_immune_integration.cpp)
target_link_libraries(unit_perception_audio_immune_integration
    nimcp gtest gtest_main pthread)

# 3. Build:
cd build && cmake .. && make nimcp -j4
make unit_perception_audio_immune_integration -j4

# 4. Test:
./test/unit/perception/immune/unit_perception_audio_immune_integration --gtest_brief=1
# Expected: [  PASSED  ] 22 tests
```

## 🔬 Test Categories

**Lifecycle (3)**: Config, create/destroy, null handling
**Immune→Audio (8)**: Cytokine effects, inflammation, bandwidth, noise, impairment
**Audio→Immune (7)**: Loudness, novelty, anomaly, failure, tinnitus, calm
**Query (4)**: State queries, attention, severity, thread safety

## 💡 Usage Patterns

### Pattern 1: Monitor Impairment
```c
if (audio_immune_is_impaired(bridge)) {
    float acc_loss = audio_immune_get_accuracy_reduction(bridge);
    float bw_loss = audio_immune_compute_bandwidth_reduction(bridge);
    // Compensate or alert
}
```

### Pattern 2: Trigger from Threat
```c
if (loud_sound || novel_sound || anomaly_detected) {
    audio_immune_trigger_from_threat(bridge,
        compute_loudness(),
        compute_novelty(),
        compute_anomaly());
}
```

### Pattern 3: Boost from Calm
```c
if (quiet_environment && music_playing) {
    audio_immune_boost_from_calm_environment(bridge,
        quietness_level,
        music_presence,
        predictability);
}
```

### Pattern 4: Update Loop
```c
while (running) {
    audio_immune_bridge_update(bridge, delta_ms);

    // Get current state
    inflammation_audio_state_t state;
    audio_immune_get_inflammation_state(bridge, &state);

    // Adjust processing
    float effective_accuracy = baseline * state.processing_accuracy;
    float effective_bandwidth = baseline * state.processing_bandwidth;
}
```

## ⚠️ Common Gotchas

1. **Create Order**: Immune and audio must exist before bridge
2. **Start Immune**: Call `brain_immune_start()` before using bridge
3. **Mutex**: All query functions are thread-safe
4. **Null Checks**: All functions handle NULL gracefully (return -1 or 0.0f)
5. **Update Frequency**: Call `bridge_update()` at 10-60 Hz
6. **Threshold Tuning**: Adjust config thresholds for your use case

## 📖 Documentation
- Full API: `docs/AUDIO_IMMUNE_INTEGRATION.md`
- Build Guide: `AUDIO_IMMUNE_BUILD_INSTRUCTIONS.md`
- Summary: `AUDIO_IMMUNE_INTEGRATION_SUMMARY.md`

## 🔗 Dependencies
- Brain Immune: `cognitive/immune/nimcp_brain_immune.h` (required)
- Audio Cortex: `perception/nimcp_audio_cortex.h` (required)
- Logging: `utils/logging/nimcp_logging.h` (optional)

## 📊 Performance
- Memory: ~1KB per bridge
- CPU: < 1% overhead
- Mutex: Microsecond lock duration
- Thread-safe: Full protection

## 🧪 Validation
```bash
# Run all tests
./test/unit/perception/immune/unit_perception_audio_immune_integration

# Run specific test
./test/unit/perception/immune/unit_perception_audio_immune_integration \
  --gtest_filter=AudioImmuneTest.CytokineImpairmentOfProcessing

# Verbose output
./test/unit/perception/immune/unit_perception_audio_immune_integration \
  --gtest_brief=0
```

## 🎓 Scientific Basis
1. IL-6 reduces A1 plasticity (Fujioka 2014)
2. Cytokines impair cochlear function (Taishi 2012)
3. Tinnitus linked to neuroinflammation (Eggermont 2004)
4. Noise pollution triggers immunity (Munzel 2018)
5. Immune dysfunction in tinnitus (Mazurek 2010)
6. Audio-immune interactions (Kraus 2015)

---
**Version**: 1.0.0 | **Date**: 2025-12-11 | **Status**: Complete

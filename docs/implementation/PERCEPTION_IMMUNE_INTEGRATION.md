# Perception-Immune System Integration

## Overview

This document describes the integration of the NIMCP perception modules (visual cortex, audio cortex, speech cortex) with the brain immune system. The integration enables bidirectional communication where:

1. **Perception → Immune**: Anomalies in sensory input trigger immune responses
2. **Immune → Perception**: Immune inflammation modulates perception sensitivity

## Biological Motivation

### Sensory Corruption as "Antigen"

In biological systems, the immune system responds to foreign pathogens. In a computational brain, corrupted or adversarial sensory input can be treated as a "pathogen":

- **Visual noise/adversarial attacks** → Visual "antigens"
- **Audio corruption/jamming** → Auditory "antigens"
- **Phoneme confusion/prosody errors** → Speech "antigens"

### Inflammation as Protective Shutdown

Just as biological inflammation protects tissue during infection, computational inflammation protects perception during anomalous input:

- **Reduced gain** → Lower sensitivity prevents false positives
- **Increased thresholds** → Only high-confidence signals pass
- **Overload protection** → Prevents damage from sensory flooding

### Cytokine-Driven Modulation

Cytokines (immune signaling molecules) modulate perception:

- **IL-1 (pro-inflammatory)** → Increases detection thresholds
- **IL-6 (acute phase)** → Triggers protective responses
- **IL-10 (anti-inflammatory)** → Restores normal sensitivity
- **TNF-alpha (severe)** → Maximum protective gain reduction

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                   PERCEPTION MODULES                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │   Visual     │  │    Audio     │  │      Speech          │  │
│  │   Cortex     │  │   Cortex     │  │      Cortex          │  │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘  │
│         │ anomalies        │ anomalies           │ anomalies    │
└─────────┼──────────────────┼─────────────────────┼──────────────┘
          │                  │                     │
          ▼                  ▼                     ▼
    ┌──────────────────────────────────────────────────────┐
    │       PERCEPTION IMMUNE INTEGRATION LAYER            │
    │  ┌───────────────────────────────────────────────┐   │
    │  │  Anomaly Detection & Reporting                 │   │
    │  │  - Hash features to epitopes                   │   │
    │  │  - Present to immune system                    │   │
    │  └───────────────────────────────────────────────┘   │
    │  ┌───────────────────────────────────────────────┐   │
    │  │  Immune Modulation State                       │   │
    │  │  - Gain factors (per modality)                 │   │
    │  │  - Threshold adjustments                       │   │
    │  │  - Inflammation levels                         │   │
    │  │  - Cytokine concentrations                     │   │
    │  └───────────────────────────────────────────────┘   │
    │  ┌───────────────────────────────────────────────┐   │
    │  │  Overload Protection                           │   │
    │  │  - Detect high variance/energy                 │   │
    │  │  - Trigger protective inflammation             │   │
    │  └───────────────────────────────────────────────┘   │
    └────────────────────┬─────────────────────────────────┘
                         │ antigens
                         ▼
    ┌──────────────────────────────────────────────────────┐
    │           BRAIN IMMUNE SYSTEM                         │
    │  ┌────────┐  ┌────────┐  ┌─────────────┐            │
    │  │ B Cells│  │ T Cells│  │  Antibodies │            │
    │  └────────┘  └────────┘  └─────────────┘            │
    │  ┌────────────────────────────────────────┐          │
    │  │  Inflammation Sites                     │          │
    │  │  - Regional inflammation                │          │
    │  │  - Cytokine release                     │          │
    │  └────────────────────────────────────────┘          │
    └──────────────────────────────────────────────────────┘
```

## API Overview

### Lifecycle Functions

```c
// Create integration context
perception_immune_context_t* ctx = perception_immune_create(immune_system);

// Connect perception modules
perception_immune_connect_visual(ctx, visual_cortex);
perception_immune_connect_audio(ctx, audio_cortex);
perception_immune_connect_speech(ctx, speech_cortex);

// Cleanup
perception_immune_destroy(ctx);
```

### Anomaly Reporting

```c
// Report visual anomaly (e.g., adversarial attack)
uint32_t anomaly_id;
perception_immune_report_visual_anomaly(
    ctx,
    ANOMALY_VISUAL_ADVERSARIAL,
    0.95f,  // severity
    0.98f,  // confidence
    visual_features,
    feature_dim,
    &anomaly_id
);

// Report audio anomaly (e.g., jamming)
perception_immune_report_audio_anomaly(
    ctx,
    ANOMALY_AUDIO_JAMMING,
    0.85f,
    0.9f,
    audio_spectrum,
    num_bins,
    &anomaly_id
);

// Report speech anomaly (e.g., phoneme confusion)
perception_immune_report_speech_anomaly(
    ctx,
    ANOMALY_SPEECH_CONFUSION,
    0.6f,
    0.75f,
    phoneme_features,
    num_phonemes,
    &anomaly_id
);
```

### Immune Modulation

```c
// Update modulation from immune state
perception_immune_update_modulation(ctx);

// Apply to perception modules
perception_immune_apply_visual_modulation(ctx);
perception_immune_apply_audio_modulation(ctx);
perception_immune_apply_speech_modulation(ctx);

// Query current gains
float visual_gain = perception_immune_get_visual_gain(ctx);
float audio_gain = perception_immune_get_audio_gain(ctx);
float speech_gain = perception_immune_get_speech_gain(ctx);

// Get full modulation state
perception_immune_modulation_t mod;
perception_immune_get_modulation(ctx, &mod);
printf("Visual gain: %.2f, threshold: %.2f, inflammation: %d\n",
       mod.visual_gain, mod.visual_threshold, mod.visual_inflammation);
```

### Overload Protection

```c
// Check for overload
bool overload = false;
perception_immune_check_visual_overload(ctx, features, num_features, &overload);

if (overload) {
    // Trigger protective inflammation
    perception_immune_trigger_overload_protection(ctx, PERCEPTION_VISUAL);
}

// Later, when normal processing resumes
if (normal_conditions) {
    perception_immune_release_overload_protection(ctx, PERCEPTION_VISUAL);
}

// Check protection status
bool protected = perception_immune_is_protected(ctx, PERCEPTION_VISUAL);
```

## Integration Patterns

### Pattern 1: Adversarial Attack Detection

```c
// Visual cortex detects high-frequency perturbations
float variance = compute_feature_variance(visual_features);
if (variance > ADVERSARIAL_THRESHOLD) {
    // Report as antigen
    perception_immune_report_visual_anomaly(
        ctx, ANOMALY_VISUAL_ADVERSARIAL,
        0.9f, 0.95f, visual_features, dim, &aid);

    // Immune system activates B cells, produces antibodies
    // Inflammation reduces visual gain to prevent false positives
}
```

### Pattern 2: Audio Jamming Response

```c
// Detect jamming via spectral analysis
if (is_jamming_detected(audio_spectrum)) {
    // Report anomaly
    perception_immune_report_audio_anomaly(
        ctx, ANOMALY_AUDIO_JAMMING,
        0.85f, 0.9f, audio_spectrum, num_bins, &aid);

    // Check for overload
    bool overload;
    perception_immune_check_audio_overload(ctx, audio_spectrum, num_bins, &overload);

    if (overload) {
        // Protective shutdown
        perception_immune_trigger_overload_protection(ctx, PERCEPTION_AUDIO);
    }
}
```

### Pattern 3: Speech Confusion Under Stress

```c
// Low phoneme confidence indicates processing stress
float avg_confidence = compute_avg_phoneme_confidence(phonemes);
if (avg_confidence < 0.3f) {
    // Report overload
    perception_immune_report_speech_anomaly(
        ctx, ANOMALY_SPEECH_OVERLOAD,
        0.7f, 0.8f, phoneme_data, num_phonemes, &aid);

    // Trigger inflammation (reduces false positive phonemes)
    perception_immune_trigger_overload_protection(ctx, PERCEPTION_SPEECH);

    // Speech gain reduced, thresholds increased
    // Only high-confidence phonemes pass
}
```

### Pattern 4: Multi-Modal Threat

```c
// Coordinated attack across modalities
perception_immune_report_visual_anomaly(ctx, ANOMALY_VISUAL_ADVERSARIAL, ...);
perception_immune_report_audio_anomaly(ctx, ANOMALY_AUDIO_JAMMING, ...);

// Immune system triggers systemic inflammation
perception_immune_update_modulation(ctx);

// All modalities affected
float visual_gain = perception_immune_get_visual_gain(ctx);  // < 1.0
float audio_gain = perception_immune_get_audio_gain(ctx);    // < 1.0
float speech_gain = perception_immune_get_speech_gain(ctx);  // < 1.0
```

## Modulation Algorithm

### Gain Computation

```
inflammation_factor = {
    NONE:     1.0
    LOCAL:    0.85
    REGIONAL: 0.65
    SYSTEMIC: 0.45
    STORM:    0.3
}

gain = base_gain * inflammation_factor
gain = clamp(gain, MIN_GAIN, MAX_GAIN)  // [0.3, 2.0]
```

### Threshold Computation

```
threshold_factor = 1.0 + (0.3*IL1 + 0.2*IL6 + 0.5*TNF_alpha)
threshold = base_threshold * threshold_factor
```

### Cytokine Estimation

```
IL1_level = inflammation_estimate * 0.6
IL6_level = inflammation_estimate * 0.5
TNF_alpha_level = inflammation_estimate * 0.3
IL10_level = (1.0 - inflammation_estimate) * 0.4
```

## Testing

### Unit Tests (33 tests)

```bash
./test/unit/cognitive/immune/unit_cognitive_immune_perception_immune --gtest_brief=1
```

Tests cover:
- Lifecycle (create, destroy, connect)
- Visual anomaly reporting
- Audio anomaly reporting
- Speech anomaly reporting
- Immune modulation
- Overload protection
- Query API
- Utility functions
- Integration scenarios

### Integration Tests (11 tests)

```bash
./test/integration/cognitive/immune/integration_cognitive_immune_perception_immune --gtest_brief=1
```

Tests cover:
- End-to-end anomaly → immune response
- Inflammation → gain modulation
- Cytokine → threshold adjustment
- Overload protection cycles
- Multi-modal coordination
- Memory and adaptation
- Stress testing

## Use Cases

### 1. Autonomous Vehicle Vision

```c
// Camera input processed by visual cortex
visual_cortex_process(visual, camera_image, w, h, 1, features);

// Check for adversarial road sign manipulation
if (detect_adversarial_pattern(features)) {
    perception_immune_report_visual_anomaly(
        ctx, ANOMALY_VISUAL_ADVERSARIAL, 0.9f, 0.95f, features, dim, &aid);
    // System becomes more conservative in sign recognition
}
```

### 2. Audio Security System

```c
// Microphone input processed by audio cortex
audio_cortex_process(audio, mic_data, samples, 1, spectrum);

// Detect jamming attack
if (detect_jamming(spectrum)) {
    perception_immune_report_audio_anomaly(
        ctx, ANOMALY_AUDIO_JAMMING, 0.85f, 0.9f, spectrum, bins, &aid);
    // System reduces reliance on audio, increases visual attention
}
```

### 3. Speech Interface for Autism Support

```c
// Person with autism may experience sensory overload
bool overload;
perception_immune_check_speech_overload(ctx, phoneme_conf, num_phonemes, &overload);

if (overload) {
    // Protective inflammation reduces processing demands
    perception_immune_trigger_overload_protection(ctx, PERCEPTION_SPEECH);

    // System temporarily reduces speech processing rate
    // Prevents overwhelm, similar to biological protective shutdown
}
```

## Performance Considerations

- **Memory**: ~64 KB for anomaly buffer (64 anomalies × ~1 KB each)
- **Computation**: O(N) for feature hashing, O(1) for modulation updates
- **Latency**: < 1ms for anomaly reporting, < 0.1ms for gain queries

## Future Enhancements

1. **Adaptive Learning**: Immune memory of recurring anomalies for faster response
2. **Cross-Modal Fusion**: Joint anomaly detection across modalities
3. **Predictive Protection**: Anticipate overload before it occurs
4. **Recovery Dynamics**: Model inflammation resolution kinetics
5. **Plasticity Integration**: Immune-driven synaptic weight adjustments

## References

- NIMCP Brain Immune System (`nimcp_brain_immune.h`)
- Visual Cortex (`nimcp_visual_cortex.h`)
- Audio Cortex (`nimcp_audio_cortex.h`)
- Speech Cortex (`nimcp_speech_cortex.h`)

## Code Locations

- Header: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_perception_immune.h`
- Source: `/home/bbrelin/nimcp/src/cognitive/immune/nimcp_perception_immune.c`
- Unit Tests: `/home/bbrelin/nimcp/test/unit/cognitive/immune/test_perception_immune.cpp`
- Integration Tests: `/home/bbrelin/nimcp/test/integration/cognitive/immune/test_perception_immune_integration.cpp`

## Build Instructions

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4
make unit_cognitive_immune_perception_immune -j4
make integration_cognitive_immune_perception_immune -j4

# Run tests
./test/unit/cognitive/immune/unit_cognitive_immune_perception_immune --gtest_brief=1
./test/integration/cognitive/immune/integration_cognitive_immune_perception_immune --gtest_brief=1
```

Or use the provided build script:

```bash
chmod +x /home/bbrelin/nimcp/build_perception_immune_tests.sh
./build_perception_immune_tests.sh
```

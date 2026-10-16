# NIMCP Phasor Integration - Quick Start Guide

## Overview

This guide shows how to use the new phasor-enhanced oscillation detection and phase-coded buffers in NIMCP middleware.

---

## Enhanced Oscillation Detection

### Basic Usage (Phasor Method - Default)

```c
#include "middleware/patterns/nimcp_oscillation_detector.h"

// Create detector with phasor detection enabled (default)
oscillation_detector_config_t config = oscillation_detector_default_config();
config.use_phasor_detection = true;  // Already true by default
oscillation_detector_t* detector = oscillation_detector_create(&config);

// Add samples from your neural simulation
for (int i = 0; i < 1000; i++) {
    float signal = get_neural_signal();  // Your signal source
    oscillation_detector_add_sample(detector, signal, (double)i);
}

// Detect oscillations
oscillation_result_t result;
oscillation_detector_detect(detector, &result);

// Check results
printf("Dominant band: %s\n", oscillation_band_name(result.dominant_band));
printf("Theta power: %.3f (relative: %.1f%%)\n",
       result.bands[OSC_BAND_THETA].power,
       result.bands[OSC_BAND_THETA].relative_power * 100);
printf("Has gamma: %s\n", result.has_gamma ? "Yes" : "No");

oscillation_detector_destroy(detector);
```

### Backward Compatible (Traditional DFT Method)

```c
// Disable phasor detection for compatibility
oscillation_detector_config_t config = oscillation_detector_default_config();
config.use_phasor_detection = false;  // Use traditional DFT
oscillation_detector_t* detector = oscillation_detector_create(&config);

// Rest is identical - API unchanged
```

---

## Phase-Amplitude Coupling (PAC) Detection

### Detect Theta-Gamma Coupling (Phasor Method)

```c
#include "middleware/patterns/nimcp_oscillation_detector.h"

oscillation_detector_config_t config = oscillation_detector_default_config();
config.use_phasor_detection = true;   // Use fast phasor PAC
config.enable_pac = true;              // Enable PAC detection

oscillation_detector_t* detector = oscillation_detector_create(&config);

// Add samples...
for (int i = 0; i < 2048; i++) {
    oscillation_detector_add_sample(detector, signal[i], (double)i);
}

// Detect PAC
cross_freq_coupling_t couplings[10];
uint32_t num_found = 0;
oscillation_detector_detect_pac(detector, couplings, 10, &num_found);

// Check for theta-gamma coupling
if (num_found > 0) {
    printf("Found %u couplings:\n", num_found);
    for (uint32_t i = 0; i < num_found; i++) {
        printf("  %s-%s: strength=%.3f, preferred_phase=%.2f rad\n",
               oscillation_band_name(couplings[i].phase_band),
               oscillation_band_name(couplings[i].amp_band),
               couplings[i].coupling_strength,
               couplings[i].preferred_phase);
    }
}

oscillation_detector_destroy(detector);
```

**Performance:** 2-5x faster than traditional PAC methods using `phasor_pac_modulation_index()`

---

## Phase-Coded Working Memory Buffers

### Basic Sequence Storage and Retrieval

```c
#include "middleware/buffering/nimcp_phase_coded_buffer.h"

// Create buffer with auto-phase-increment
phase_buffer_config_t config = phase_buffer_default_config();
config.auto_phase_increment = true;      // Auto-assign phases
config.phase_increment = M_PI / 4.0f;    // 45° per item (8 items/cycle)
phase_coded_buffer_t* buffer = phase_buffer_create(&config);

// Store sequence (e.g., phone number: 555-1212)
float sequence[] = {5, 5, 5, 1, 2, 1, 2};
for (int i = 0; i < 7; i++) {
    phase_buffer_store(buffer, sequence[i], 1.0f, (double)i * 100.0);
}

// Retrieve in phase order (temporal order)
phase_coded_item_t items[10];
uint32_t num_retrieved;
phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved);

printf("Retrieved sequence: ");
for (uint32_t i = 0; i < num_retrieved; i++) {
    printf("%.0f ", items[i].data);
}
printf("\n");  // Output: 5 5 5 1 2 1 2

phase_buffer_destroy(buffer);
```

### Explicit Phase Tagging

```c
// Store items with specific phases
phase_buffer_config_t config = phase_buffer_default_config();
phase_coded_buffer_t* buffer = phase_buffer_create(&config);

// Tag items at specific phases
phase_buffer_store_with_phase(buffer, 10.0f, 0.0f,      1.0f, 0.0);   // 0°
phase_buffer_store_with_phase(buffer, 20.0f, M_PI/2,    1.0f, 10.0);  // 90°
phase_buffer_store_with_phase(buffer, 30.0f, M_PI,      1.0f, 20.0);  // 180°
phase_buffer_store_with_phase(buffer, 40.0f, 3*M_PI/2,  1.0f, 30.0);  // 270°

// Items retrieved in phase order: 10, 20, 30, 40
```

### Pattern Matching by Phase Coherence

```c
phase_coded_buffer_t* buffer = phase_buffer_create(&phase_buffer_default_config());

// Store items with known phases
phase_buffer_store_with_phase(buffer, 1.0f, 0.0f,    1.0f, 0.0);
phase_buffer_store_with_phase(buffer, 2.0f, M_PI/4,  1.0f, 10.0);
phase_buffer_store_with_phase(buffer, 3.0f, M_PI/2,  1.0f, 20.0);
phase_buffer_store_with_phase(buffer, 4.0f, M_PI,    1.0f, 30.0);

// Define pattern to match (items near 0 and π/2)
float pattern_phases[] = {0.0f, M_PI/2};
phase_pattern_match_t result;

phase_buffer_pattern_match(buffer, pattern_phases, 2, 0.8f, &result);

printf("Found %u matches with mean coherence %.2f\n",
       result.count, result.mean_coherence);

// Access matched items
for (uint32_t i = 0; i < result.count; i++) {
    uint32_t idx = result.indices[i];
    printf("  Item %u: coherence=%.2f\n", idx, result.coherences[i]);
}

phase_pattern_match_free(&result);
phase_buffer_destroy(buffer);
```

### Coherence Metrics

```c
phase_coded_buffer_t* buffer = ...;  // Filled buffer

// Get overall phase coherence (0-1)
float coherence = phase_buffer_coherence(buffer);
printf("Buffer coherence: %.2f\n", coherence);
// 1.0 = perfect alignment, 0.0 = random phases

// Get mean phase
float mean_phase = phase_buffer_mean_phase(buffer);
printf("Mean phase: %.2f rad (%.0f°)\n",
       mean_phase, mean_phase * 180.0 / M_PI);

// Get buffer stats
uint32_t count, capacity;
float mean_coherence;
phase_buffer_get_stats(buffer, &count, &capacity, &mean_coherence);
printf("Buffer: %u/%u items, coherence=%.2f\n",
       count, capacity, mean_coherence);
```

---

## Neuroscience-Inspired Usage

### Hippocampal Theta Phase Precession

```c
// Simulate place cell firing during spatial navigation
phase_buffer_config_t config = phase_buffer_default_config();
config.theta_frequency_hz = 8.0f;  // Hippocampal theta
phase_coded_buffer_t* buffer = phase_buffer_create(&config);

// Positions along track, encoded at theta phases
float positions[] = {0, 25, 50, 75, 100};  // cm
float phases[] = {0, M_PI/4, M_PI/2, 3*M_PI/4, M_PI};

for (int i = 0; i < 5; i++) {
    // Earlier positions = earlier phases within theta cycle
    phase_buffer_store_with_phase(buffer, positions[i], phases[i],
                                   1.0f, (double)i * 125.0);
}

// Retrieval in phase order = spatial order
phase_coded_item_t items[10];
uint32_t num_retrieved;
phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved);

for (uint32_t i = 0; i < num_retrieved; i++) {
    printf("Position: %.0f cm (phase: %.2f rad)\n",
           items[i].data, items[i].phase);
}

phase_buffer_destroy(buffer);
```

### Working Memory Item Maintenance

```c
// Maintain items in working memory with gamma phase coding
phase_buffer_config_t config = phase_buffer_default_config();
config.phase_increment = (2 * M_PI) / 7.0f;  // 7-item capacity
phase_coded_buffer_t* buffer = phase_buffer_create(&config);

// Add items with varying memory strength (amplitude)
phase_buffer_store(buffer, 42.0f, 1.0f,  0.0);   // Strong memory
phase_buffer_store(buffer, 17.0f, 0.8f,  100.0); // Medium memory
phase_buffer_store(buffer, 99.0f, 0.5f,  200.0); // Weak memory

// Check memory organization
float coherence = phase_buffer_coherence(buffer);
if (coherence > 0.7f) {
    printf("Working memory well-organized (coherence=%.2f)\n", coherence);
} else {
    printf("Working memory degraded (coherence=%.2f)\n", coherence);
}

phase_buffer_destroy(buffer);
```

---

## Configuration Options

### Oscillation Detector

```c
oscillation_detector_config_t config = oscillation_detector_default_config();

// Core parameters
config.sample_rate_hz = 1000.0f;        // Sampling rate
config.window_size = 1024;              // FFT window (power of 2)
config.overlap_fraction = 0.5f;         // Window overlap

// Burst detection
config.enable_burst_detection = true;
config.min_burst_duration_ms = 50.0f;
config.burst_threshold_std = 2.0f;      // Std devs above mean

// Advanced features
config.enable_plv = false;              // Phase locking value (expensive)
config.enable_pac = false;              // PAC detection (expensive)

// Phasor methods
config.use_phasor_detection = true;     // Use complex phasor methods
```

### Phase-Coded Buffer

```c
phase_buffer_config_t config = phase_buffer_default_config();

config.capacity = 64;                   // Max items
config.auto_phase_increment = true;     // Auto-assign phases
config.phase_increment = M_PI / 4;      // Phase step per item
config.theta_frequency_hz = 8.0f;       // Theta frequency for cycling
config.enable_coherence_sort = false;   // Sort by coherence on retrieval
```

---

## Performance Tips

1. **Use Phasor Methods:** ~2-5x faster for PAC, more accurate phase extraction
2. **Adjust Window Size:** Larger windows = better frequency resolution, slower processing
3. **Disable Expensive Features:** PLV and PAC add overhead if not needed
4. **Pre-allocate Buffers:** Reuse detector/buffer objects instead of recreating
5. **Batch Processing:** Add multiple samples before calling detect()

---

## Error Handling

```c
// All functions return bool or NULL on error
oscillation_detector_t* detector = oscillation_detector_create(&config);
if (!detector) {
    fprintf(stderr, "Failed to create oscillation detector\n");
    return -1;
}

// Check return values
if (!oscillation_detector_detect(detector, &result)) {
    fprintf(stderr, "Detection failed (need more samples?)\n");
}

// Cleanup always safe with NULL checks
phase_buffer_destroy(NULL);  // No-op, safe
```

---

## Common Pitfalls

1. **Not Enough Samples:** Need at least `window_size` samples before detection
2. **Wrong Phase Range:** Phases must be in [-π, π], wrap if needed
3. **Buffer Overflow:** Check capacity before storing, or handle failure
4. **Frequency Mismatch:** Ensure signal sample rate matches config
5. **Phase Sorting:** `retrieve_ordered()` sorts by phase, may change original order

---

## Examples in Codebase

See:
- `/test/unit/middleware/patterns/test_oscillation_detector_phasor.cpp` - Full test suite
- `/test/unit/middleware/buffering/test_phase_coded_buffer.cpp` - Usage examples

---

## API Reference

Full API documentation in headers:
- `/include/middleware/patterns/nimcp_oscillation_detector.h`
- `/include/middleware/buffering/nimcp_phase_coded_buffer.h`
- `/include/utils/math/nimcp_complex_math.h` (underlying phasor utilities)

---

*End of Quick Start Guide*

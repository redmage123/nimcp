# Portia Classification & Deception - Quick Reference

## Target Classification

### Initialization
```c
portia_classification_config_t config = {
    .classification_threshold = 0.5f,
    .max_targets = 100,
    .retention_time_ms = 5000,
    .enable_prediction = true,
    .enable_bio_async = false
};
portia_classifier_t classifier = portia_classification_init(&config);
```

### Basic Operations
```c
// Add target
uint32_t id = portia_classification_add_target(classifier, x, y, z, size);

// Update position
portia_classification_update(classifier, id, new_x, new_y, new_z);

// Classify
target_class_t class;
float confidence;
portia_classification_classify(classifier, id, &class, &confidence);

// Get threats
uint32_t threats[10];
uint32_t count = portia_classification_get_threats(classifier, threats, 10);

// Prune stale targets
uint32_t pruned = portia_classification_prune(classifier);

// Cleanup
portia_classification_destroy(classifier);
```

### Target Classes
```c
TARGET_CLASS_UNKNOWN  - Not enough data
TARGET_CLASS_FRIENDLY - Known ally
TARGET_CLASS_NEUTRAL  - No threat, no interest
TARGET_CLASS_THREAT   - Large + fast = danger
TARGET_CLASS_PREY     - Small + fast OR medium speed
TARGET_CLASS_OBSTACLE - Large + stationary
```

---

## Deception System

### Initialization
```c
portia_deception_config_t config = {
    .enable_stealth = true,
    .enable_mimicry = true,
    .enable_jamming = true,
    .default_emission_level = 1.0f,
    .profile_count = 10,
    .enable_bio_async = false
};
portia_deception_t deception = portia_deception_init(&config);
```

### Stealth Operations
```c
// Set stealth mode
portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);

// Control emissions (0.0 = silent, 1.0 = normal)
portia_deception_emit(deception, 0.2f);

// Check effectiveness (0.0-1.0)
float eff = portia_deception_get_effectiveness(deception);

// Enable jamming
portia_deception_jam(deception, true);
```

### Mimicry Operations
```c
// Register profile
mimicry_profile_t profile = {0};
strncpy(profile.name, "prey_spider", sizeof(profile.name)-1);
profile.pattern_length = 4;
profile.signal_pattern[0] = 0.3f;
profile.signal_pattern[1] = 0.5f;
profile.signal_pattern[2] = 0.2f;
profile.signal_pattern[3] = 0.4f;
profile.effectiveness = 0.85f;

uint32_t id = portia_deception_register_profile(deception, &profile);

// Activate mimicry
portia_deception_mimic(deception, id);

// Get all profiles
mimicry_profile_t profiles[10];
uint32_t count = portia_deception_get_profiles(deception, profiles, 10);

// Cleanup
portia_deception_destroy(deception);
```

### Stealth Modes
```c
STEALTH_MODE_NONE    - Normal operation (0% effective)
STEALTH_MODE_PASSIVE - Minimize emissions (1.0 - emission_level)
STEALTH_MODE_ACTIVE  - Active countermeasures (80% base, +15% with jamming)
STEALTH_MODE_MIMICRY - Imitate entity (profile effectiveness)
```

---

## Files Reference

### Headers
- `/home/bbrelin/nimcp/include/portia/nimcp_portia_classification.h`
- `/home/bbrelin/nimcp/include/portia/nimcp_portia_deception.h`

### Implementation
- `/home/bbrelin/nimcp/src/portia/nimcp_portia_classification.c`
- `/home/bbrelin/nimcp/src/portia/nimcp_portia_deception.c`

### Tests
- `/home/bbrelin/nimcp/test/unit/portia/test_portia_classification.cpp` (26 tests)
- `/home/bbrelin/nimcp/test/unit/portia/test_portia_deception.cpp` (34 tests)

### Demo
- `/home/bbrelin/nimcp/examples/portia_demo.c`

---

## Testing

```bash
# Build
cd /home/bbrelin/nimcp/build
cmake --build . --target test_portia_classification
cmake --build . --target test_portia_deception

# Run
ctest -R PortiaClassification -V
ctest -R PortiaDeception -V

# Direct execution
./test/unit/portia/test_portia_classification
./test/unit/portia/test_portia_deception
```

---

## Classification Algorithm

```
if observations < 3:
    → UNKNOWN (confidence: 0.1)
else if speed > 2.0:
    if size > 1.0:
        → THREAT (confidence: 0.7 + speed_factor)
    else:
        → PREY (confidence: 0.6 + speed_factor)
else if speed < 0.5:
    if size < 0.5:
        → NEUTRAL (confidence: 0.8)
    else:
        → OBSTACLE (confidence: 0.9)
else:
    → PREY (confidence: 0.7)

Confidence bonus: +0.02 per observation (max +0.2)
```

---

## Effectiveness Calculation

```
NONE:     0.0
PASSIVE:  1.0 - emission_level
ACTIVE:   0.8 - (emission_level * 0.5) + (jamming ? 0.15 : 0)
MIMICRY:  profile.effectiveness
```

---

## Bio-Async Messages

### Classification Events (outgoing)
- `BIO_MSG_TARGET_CLASSIFIED` via Acetylcholine
  - Payload: target_id, classification, confidence

### Deception Events (outgoing)
- `BIO_MSG_STEALTH_STATE_CHANGED` via Serotonin
  - Payload: old_mode, new_mode, effectiveness

### Inbox Handlers (incoming)
- `BIO_MSG_TARGET_QUERY` - Get target info
- `BIO_MSG_THREAT_ASSESSMENT_REQUEST` - Get threats
- `BIO_MSG_STEALTH_MODE_REQUEST` - Change mode
- `BIO_MSG_EMISSION_CONTROL_REQUEST` - Adjust emissions

---

## Error Codes

```c
NIMCP_SUCCESS              // 0 - Operation succeeded
NIMCP_ERROR_INVALID_PARAM  // Invalid input parameter
NIMCP_ERROR_NOT_FOUND      // Target/profile not found
NIMCP_ERROR_NOT_SUPPORTED  // Feature disabled in config
NIMCP_ERROR_INVALID_STATE  // Operation not valid in current state
```

---

## Thread Safety

✅ All operations thread-safe
✅ Mutex-protected shared state
✅ Concurrent add/update/classify supported
✅ Lock-free reads where possible

---

## Memory Usage

**Classification:**
```
Base: sizeof(classifier) ≈ 128 bytes
Registry: max_targets * sizeof(target_info_t)
        = max_targets * 80 bytes
Total: ~128 + (max_targets * 80)
```

**Deception:**
```
Base: sizeof(deception) ≈ 96 bytes
Profiles: profile_count * sizeof(mimicry_profile_t)
        = profile_count * 128 bytes
Total: ~96 + (profile_count * 128)
```

---

## Common Patterns

### Track and Classify Pattern
```c
// 1. Add target when first detected
uint32_t id = portia_classification_add_target(classifier, x, y, z, size);

// 2. Update position each frame
for (int frame = 0; frame < 100; frame++) {
    detect_target_position(&x, &y, &z);  // Your sensor code
    portia_classification_update(classifier, id, x, y, z);
}

// 3. Classify after enough observations
if (frame >= 10) {
    target_class_t class;
    float confidence;
    portia_classification_classify(classifier, id, &class, &confidence);

    if (confidence > 0.7f) {
        // Act on classification
    }
}

// 4. Periodically prune stale targets
if (frame % 60 == 0) {
    portia_classification_prune(classifier);
}
```

### Adaptive Stealth Pattern
```c
// 1. Start passive
portia_deception_set_mode(deception, STEALTH_MODE_PASSIVE);
portia_deception_emit(deception, 0.3f);

// 2. Monitor threat level
uint32_t threats[10];
uint32_t count = portia_classification_get_threats(classifier, threats, 10);

// 3. Escalate if needed
if (count > 3) {
    // High threat - go active with jamming
    portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    portia_deception_emit(deception, 0.1f);
    portia_deception_jam(deception, true);
}
else if (count > 0) {
    // Moderate threat - active stealth
    portia_deception_set_mode(deception, STEALTH_MODE_ACTIVE);
    portia_deception_emit(deception, 0.2f);
}
else {
    // No threats - conserve power
    portia_deception_set_mode(deception, STEALTH_MODE_NONE);
    portia_deception_emit(deception, 1.0f);
}
```

### Lure and Hunt Pattern
```c
// 1. Identify prey
target_class_t class;
float confidence;
portia_classification_classify(classifier, target_id, &class, &confidence);

if (class == TARGET_CLASS_PREY && confidence > 0.8f) {
    // 2. Activate courtship mimicry
    uint32_t courtship_profile = find_profile_by_name(deception, "courtship");
    portia_deception_mimic(deception, courtship_profile);

    // 3. Wait for prey to approach
    while (distance_to_prey(target_id) > strike_range) {
        // Monitor prey position
        target_info_t info;
        portia_classification_get_target(classifier, target_id, &info);

        // Adjust mimicry intensity based on distance
        float intensity = 1.0f - (distance / max_range);
        portia_deception_emit(deception, intensity);

        usleep(100000);  // 100ms
    }

    // 4. Strike!
    execute_strike(target_id);
}
```

---

## Performance Tips

1. **Batch Updates:** Group position updates when possible
2. **Prune Regularly:** Don't let stale targets accumulate
3. **Classification Threshold:** Higher threshold = fewer false positives
4. **Bio-Async:** Disable for maximum performance (synchronous mode)
5. **Target Capacity:** Size registry appropriately for your workload

---

## Debugging

### Enable Detailed Logging
```c
#define LOG_MODULE "portia_classification"  // or "portia_deception"
```

### Common Issues

**"Target not found"**
- Target pruned due to staleness
- Target ID incorrect
- Target never added

**"Registry full"**
- Increase max_targets in config
- Prune more frequently
- Reduce retention_time_ms

**"Invalid classification"**
- Not enough observations (< 3)
- Wait for more position updates
- Check velocity computation

**"Mimicry not working"**
- Profile not registered
- Mimicry capability disabled
- Wrong profile ID

---

## Example Output (Demo)

```
╔═══════════════════════════════════════════════════════════════╗
║     PORTIA SPIDER - TARGET CLASSIFICATION & DECEPTION         ║
╚═══════════════════════════════════════════════════════════════╝

=== Classification Results ===

Prey target (ID 1):
   - Classification: PREY
   - Confidence: 73.00%
   - Position: (12.00, 8.00, 0.00)
   - Velocity: (1.20, 0.80, 0.00)
   - Speed: 1.44 units/s
   - Observations: 11

Threat target (ID 2):
   - Classification: THREAT
   - Confidence: 85.00%
   - Position: (25.00, 0.00, 0.00)
   - Velocity: (1.50, -1.00, 0.00)
   - Speed: 1.80 units/s
   - Observations: 11

=== Threat Assessment ===

Detected 1 threat(s):
   - Target ID: 2

=== Stealth Demonstration ===

Activating courtship signal mimicry (lure prey):
   - Mode: MIMICRY
   - Active profile: 3 (courtship_signal)
   - Effectiveness: 92.00%
   - Status: Imitating mate signals to attract prey
```

---

**Quick Start:** Run `/home/bbrelin/nimcp/examples/portia_demo.c` for complete walkthrough!

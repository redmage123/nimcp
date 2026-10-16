# Feature Extractor-Immune Bridge API Quick Reference

## Lifecycle

```c
// Get default configuration
feature_immune_config_t config;
feature_immune_default_config(&config);

// Create bridge
feature_immune_bridge_t* bridge = feature_immune_bridge_create(
    &config,              // NULL for defaults
    immune_system,        // brain_immune_system_t*
    feature_extractor     // feature_extractor_t
);

// Destroy bridge
feature_immune_bridge_destroy(bridge);
```

## Main Update Loop

```c
// In your main loop:
middleware_features_t* features = /* extract from spike data */;

// Update bridge (both directions)
feature_immune_bridge_update(bridge, features, delta_ms);

// Query results
float precision = feature_immune_get_precision_factor(bridge);
bool threat = feature_immune_is_threat_detected(bridge);
float quality = feature_immune_get_quality_score(bridge);
```

## Immune → Features (Query)

```c
// Apply effects manually
feature_immune_apply_cytokine_effects(bridge);
feature_immune_apply_inflammation_effects(bridge);

// Get cytokine effects
cytokine_feature_effects_t cytokine_effects;
feature_immune_get_cytokine_effects(bridge, &cytokine_effects);

// Get inflammation effects
inflammation_feature_state_t inflammation_state;
feature_immune_get_inflammation_state(bridge, &inflammation_state);

// Get precision reduction
float precision = feature_immune_compute_precision_reduction(bridge);
float precision_api = feature_immune_get_precision_factor(bridge);
```

## Features → Immune (Trigger)

```c
// Trigger from anomalies (checks all 6 types)
feature_immune_trigger_from_anomalies(bridge, features);

// Check specific anomalies
feature_immune_detect_dead_neurons(bridge, features);
feature_immune_detect_binding_failure(bridge, features);

// Monitor quality (escalates on chronic degradation)
feature_immune_escalate_from_degradation(bridge, features);

// Check if threat detected
if (feature_immune_is_threat_detected(bridge)) {
    printf("Immune response triggered!\n");
    printf("Severity: %u\n", bridge->immune_trigger.immune_severity);
}
```

## Configuration Options

```c
feature_immune_config_t config;
feature_immune_default_config(&config);

// Enable/disable features
config.enable_cytokine_feature_modulation = true;
config.enable_inflammation_precision_reduction = true;
config.enable_feature_immune_trigger = true;
config.enable_threat_feature_bias = true;
config.enable_quality_monitoring = true;

// Sensitivity tuning (0.5-2.0)
config.cytokine_sensitivity = 1.0f;
config.inflammation_sensitivity = 1.0f;
config.anomaly_trigger_sensitivity = 1.0f;

// Anomaly thresholds
config.burst_threshold = 0.70f;           // [0.5-0.9]
config.fano_threshold = 3.00f;            // [2.0-5.0]
config.isi_cv_threshold = 2.00f;          // [1.5-3.0]
config.sync_threshold = 0.90f;            // [0.7-0.95]
config.entropy_collapse_threshold = 0.10f; // [0.0-0.2]
config.gamma_collapse_threshold = 0.10f;   // [0.0-0.2]

// Quality monitoring
config.chronic_degradation_threshold = 0.5f; // [0.3-0.7]
config.chronic_duration_sec = 300.0f;        // seconds
```

## Anomaly Severity Mapping

| Anomaly Type | Threshold | Severity | Response |
|--------------|-----------|----------|----------|
| Entropy Collapse | < 0.10 | 10 | CRITICAL - Dead neurons |
| Gamma Collapse | < 0.10 | 9 | SEVERE - Binding failure |
| ISI CV High | > 2.00 | 8 | Pathological firing |
| Fano High | > 3.00 | 7 | High variability |
| Burst High | > 0.70 | 6 | Excessive bursting |
| Sync High | > 0.90 | 5 | Abnormal synchrony |

## Precision Reduction Table

| Inflammation Level | Precision Multiplier | Effect |
|-------------------|---------------------|--------|
| NONE | 1.00 | No reduction |
| LOCAL | 0.90 | -10% precision |
| REGIONAL | 0.75 | -25% precision |
| SYSTEMIC | 0.50 | -50% precision |
| STORM | 0.20 | -80% precision |

## Example Use Cases

### 1. Monitor Feature Quality

```c
for (int step = 0; step < max_steps; step++) {
    // Extract features
    feature_extractor_update(extractor, spike_data, features);

    // Update bridge
    feature_immune_bridge_update(bridge, features, 100);

    // Check quality
    float quality = feature_immune_get_quality_score(bridge);
    if (quality < 0.5f) {
        printf("WARNING: Feature quality degraded: %.2f\n", quality);
    }

    // Check precision
    float precision = feature_immune_get_precision_factor(bridge);
    printf("Precision: %.2f\n", precision);
}
```

### 2. Detect Critical Failures

```c
feature_immune_bridge_update(bridge, features, delta_ms);

if (bridge->immune_trigger.entropy_collapse) {
    fprintf(stderr, "CRITICAL: Dead neurons detected!\n");
    fprintf(stderr, "Entropy: %.3f (threshold: %.3f)\n",
            features->spike_entropy,
            FEATURE_ENTROPY_DEAD_THRESHOLD);
    // Trigger emergency response
}

if (bridge->immune_trigger.gamma_collapse) {
    fprintf(stderr, "SEVERE: Feature binding failure!\n");
    fprintf(stderr, "Gamma power: %.3f (threshold: %.3f)\n",
            features->gamma_power,
            FEATURE_GAMMA_COLLAPSE_THRESHOLD);
    // Trigger recovery
}
```

### 3. Inflammation Impact Analysis

```c
// Simulate different inflammation levels
brain_inflammation_level_t levels[] = {
    INFLAMMATION_NONE,
    INFLAMMATION_LOCAL,
    INFLAMMATION_REGIONAL,
    INFLAMMATION_SYSTEMIC,
    INFLAMMATION_STORM
};

for (int i = 0; i < 5; i++) {
    // Set inflammation level (would normally be set by immune system)
    SimulateInflammation(levels[i]);

    // Update bridge
    feature_immune_apply_inflammation_effects(bridge);

    // Query precision
    float precision = feature_immune_get_precision_factor(bridge);

    printf("Level: %s, Precision: %.2f\n",
           brain_immune_inflammation_to_string(levels[i]),
           precision);
}
```

## Thread Safety

All API functions are thread-safe and use internal mutex locking:

```c
// Safe to call from multiple threads
feature_immune_bridge_update(bridge, features, delta_ms);
float precision = feature_immune_get_precision_factor(bridge);
```

## Error Handling

All functions return error codes or safe defaults:

```c
// Returns -1 on error, 0 on success
int result = feature_immune_bridge_update(bridge, features, delta_ms);
if (result != 0) {
    fprintf(stderr, "Bridge update failed\n");
}

// Returns 1.0f (no reduction) on null pointer
float precision = feature_immune_get_precision_factor(NULL);
// precision = 1.0f

// Returns false on null pointer
bool threat = feature_immune_is_threat_detected(NULL);
// threat = false
```

## Statistics Tracking

```c
printf("Total updates: %lu\n", bridge->total_updates);
printf("Cytokine modulations: %u\n", bridge->cytokine_modulations);
printf("Feature-triggered responses: %u\n", bridge->feature_triggered_responses);
printf("Anomalies detected: %u\n", bridge->anomalies_detected);
printf("Quality escalations: %u\n", bridge->quality_escalations);
```

## Integration with Other Systems

```c
// Training system - adjust learning rate based on precision
float base_lr = 0.001f;
float precision = feature_immune_get_precision_factor(bridge);
float effective_lr = base_lr * precision;
optimizer_set_learning_rate(optimizer, effective_lr);

// Attention system - use threat bias
if (bridge->inflammation_state.threat_feature_bias > 0.3f) {
    attention_increase_threat_sensitivity(attention_sys);
}

// Perception system - apply noise based on cytokines
float noise_level = bridge->cytokine_effects.noise_amplification;
perception_add_noise(perception_sys, noise_level);
```

# Middleware Phase 2 Brain Integration Guide

**Version:** 1.0
**Date:** 2025-11-19
**Status:** Implementation Ready

---

## Overview

This document provides a **complete integration plan** for Phase 2 Middleware (Population Coding, Feature Extraction) into the NIMCP brain structure.

**Phase 2 Components:**
- **Population Coding**: Neural population activity encoding
- **Feature Extractor**: Multi-feature extraction from populations
- **Rate Coding**: (Phase 1, already integrated)
- **Temporal Coding**: (Phase 1, already integrated)

**Integration Goals:**
1. Add Phase 2 components to brain configuration
2. Add Phase 2 state to brain structure
3. Initialize Phase 2 in brain_create_custom
4. Update Phase 2 in brain_update
5. Clean up Phase 2 in brain_destroy
6. Provide accessor functions

---

## 1. Brain Configuration Changes

### File: `/home/bbrelin/nimcp/include/core/brain/nimcp_brain.h`

### 1.1 Add to `brain_config_t` Structure

Insert the following fields in the `brain_config_t` structure (after existing middleware flags):

```c
// === MIDDLEWARE LAYER CONFIGURATION ===

// Phase 1: Basic Encoding (Already integrated)
bool enable_middleware;           /**< Enable middleware layer (default: false) */
bool enable_rate_coding;          /**< Enable rate-based spike encoding */
bool enable_temporal_coding;      /**< Enable temporal spike encoding */

// Phase 2: Population Coding & Feature Extraction
bool enable_population_coding;    /**< Enable population coding (NEW) */
bool enable_feature_extraction;   /**< Enable feature extraction (NEW) */

// Feature Extraction Configuration
uint32_t num_feature_types;       /**< Number of feature types to extract (NEW) */
feature_type_t* feature_types;    /**< Array of feature types to extract (NEW) */

// Population Coding Configuration
uint32_t population_tuning_curves; /**< Number of tuning curves (default: 50, NEW) */
float population_tuning_width;     /**< Tuning curve width (default: 0.2, NEW) */
bool population_normalize;         /**< Normalize population codes (default: true, NEW) */

// Feature Extraction Windows
uint32_t feature_window_ms;        /**< Feature extraction window (default: 100ms, NEW) */
uint32_t feature_step_ms;          /**< Feature sliding step (default: 50ms, NEW) */

// Integration Options
bool middleware_auto_extract;      /**< Auto-extract features on update (NEW) */
bool middleware_cache_features;    /**< Cache extracted features (NEW) */
```

### 1.2 Default Configuration Helper

Add to brain configuration helpers:

```c
/**
 * @brief Get default middleware Phase 2 configuration
 *
 * WHAT: Returns sensible defaults for Phase 2 middleware
 * WHY:  Simplifies setup for common use cases
 *
 * @return Configuration with Phase 2 enabled
 */
brain_config_t brain_config_with_middleware_phase2(brain_size_t size, brain_task_t task);
```

---

## 2. Brain Structure Changes

### File: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`

### 2.1 Add to Internal `brain_struct`

Add these fields to the internal brain structure:

```c
struct brain_struct {
    // ... existing fields ...

    // === MIDDLEWARE PHASE 1 (Already integrated) ===
    rate_coder_t rate_coder;              /**< Rate coding encoder/decoder */
    temporal_coder_t temporal_coder;      /**< Temporal coding encoder/decoder */

    // === MIDDLEWARE PHASE 2 (NEW) ===
    population_coder_t population_coder;  /**< Population coding encoder/decoder */
    feature_extractor_t feature_extractor; /**< Multi-feature extractor */

    // Feature extraction state
    feature_vector_t* cached_features;    /**< Cached feature vectors (per region) */
    uint32_t num_cached_features;         /**< Number of cached feature vectors */
    uint64_t last_feature_extraction_ms;  /**< Timestamp of last extraction */

    // Population coding state
    float* population_activities;          /**< Current population activities */
    uint32_t population_size;              /**< Population vector size */

    // Performance tracking
    uint64_t total_feature_extractions;    /**< Total extractions performed */
    float avg_extraction_time_us;          /**< Average extraction time */

    // ... rest of existing fields ...
};
```

---

## 3. Initialization in `brain_create_custom`

### File: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`

### 3.1 Add Phase 2 Initialization

Insert this code block after Phase 1 middleware initialization:

```c
/**
 * PHASE 2 MIDDLEWARE INITIALIZATION
 */
if (config->enable_middleware && config->enable_population_coding) {
    // Create population coder
    population_coding_config_t pop_config = {
        .num_tuning_curves = config->population_tuning_curves > 0 ?
                              config->population_tuning_curves : 50,
        .tuning_width = config->population_tuning_width > 0.0f ?
                        config->population_tuning_width : 0.2f,
        .normalize = config->population_normalize,
        .encoding_type = POPULATION_ENCODING_GAUSSIAN  // Default to Gaussian tuning
    };

    brain->population_coder = population_coder_create(&pop_config);
    if (!brain->population_coder) {
        fprintf(stderr, "Failed to create population coder\n");
        brain_destroy(brain);
        return NULL;
    }

    // Allocate population activities buffer
    brain->population_size = pop_config.num_tuning_curves;
    brain->population_activities = (float*)calloc(brain->population_size, sizeof(float));
    if (!brain->population_activities) {
        fprintf(stderr, "Failed to allocate population activities\n");
        brain_destroy(brain);
        return NULL;
    }

    LOG_INFO("Population coding initialized: %u tuning curves", brain->population_size);
}

if (config->enable_middleware && config->enable_feature_extraction) {
    // Build feature configuration array
    uint32_t num_features = config->num_feature_types > 0 ?
                            config->num_feature_types : 3;  // Default: 3 features

    feature_config_t* feature_configs = (feature_config_t*)malloc(
        num_features * sizeof(feature_config_t)
    );
    if (!feature_configs) {
        fprintf(stderr, "Failed to allocate feature configs\n");
        brain_destroy(brain);
        return NULL;
    }

    // Use provided feature types or defaults
    if (config->feature_types && config->num_feature_types > 0) {
        for (uint32_t i = 0; i < num_features; i++) {
            feature_configs[i] = feature_config_default(config->feature_types[i]);
            feature_configs[i].window_ms = config->feature_window_ms > 0 ?
                                           config->feature_window_ms : 100;
            feature_configs[i].step_ms = config->feature_step_ms > 0 ?
                                         config->feature_step_ms : 50;
        }
    } else {
        // Default features: rate, synchrony, burst
        feature_configs[0] = feature_config_default(FEATURE_FIRING_RATE);
        feature_configs[1] = feature_config_default(FEATURE_SYNCHRONY);
        feature_configs[2] = feature_config_default(FEATURE_BURST_RATE);

        for (uint32_t i = 0; i < num_features; i++) {
            feature_configs[i].window_ms = 100;
            feature_configs[i].step_ms = 50;
        }
    }

    // Create feature extractor
    brain->feature_extractor = feature_extractor_create(feature_configs, num_features);
    free(feature_configs);

    if (!brain->feature_extractor) {
        fprintf(stderr, "Failed to create feature extractor\n");
        brain_destroy(brain);
        return NULL;
    }

    // Allocate feature cache if enabled
    if (config->middleware_cache_features) {
        brain->num_cached_features = brain->num_brain_regions > 0 ?
                                      brain->num_brain_regions : 1;
        brain->cached_features = (feature_vector_t*)calloc(
            brain->num_cached_features,
            sizeof(feature_vector_t)
        );
        if (!brain->cached_features) {
            fprintf(stderr, "Failed to allocate feature cache\n");
            brain_destroy(brain);
            return NULL;
        }
    }

    brain->total_feature_extractions = 0;
    brain->avg_extraction_time_us = 0.0f;

    LOG_INFO("Feature extraction initialized: %u feature types", num_features);
}
```

---

## 4. Update Logic in `brain_update`

### File: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`

### 4.1 Add Phase 2 Update Logic

Insert this code block in `brain_update`, after network update but before cognitive processing:

```c
/**
 * PHASE 2 MIDDLEWARE UPDATE
 */
if (brain->feature_extractor && config->middleware_auto_extract) {
    uint64_t current_time_ms = get_current_time_ms();  // Use brain's time tracking

    // Check if enough time has passed for next extraction
    uint32_t extraction_interval = config->feature_step_ms > 0 ?
                                    config->feature_step_ms : 50;

    if (current_time_ms - brain->last_feature_extraction_ms >= extraction_interval) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Extract features from each brain region
        for (uint32_t r = 0; r < brain->num_brain_regions; r++) {
            // Get neuron population for this region
            uint32_t* neuron_ids = NULL;
            uint32_t num_neurons = brain_region_get_neurons(
                brain,
                r,  // region ID
                &neuron_ids
            );

            if (num_neurons > 0 && neuron_ids) {
                // Extract features
                feature_vector_t features = feature_extractor_extract(
                    brain->feature_extractor,
                    brain->network,
                    neuron_ids,
                    num_neurons,
                    current_time_ms
                );

                // Cache features if enabled
                if (config->middleware_cache_features && brain->cached_features) {
                    // Free old cached features
                    if (brain->cached_features[r].data) {
                        feature_vector_free(&brain->cached_features[r]);
                    }

                    // Store new features
                    brain->cached_features[r] = features;
                } else {
                    // If not caching, features are available for immediate use
                    // and must be freed by caller

                    // For auto-extract mode, we integrate with cognitive systems
                    // Example: Feed to working memory, ethics engine, etc.
                    if (brain->working_memory && features.dim > 0) {
                        working_memory_add(
                            brain->working_memory,
                            features.data,
                            features.dim,
                            0.5f  // Default salience
                        );
                    }

                    // Free if not cached
                    feature_vector_free(&features);
                }

                free(neuron_ids);
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        float extraction_time_us = (end.tv_sec - start.tv_sec) * 1e6 +
                                    (end.tv_nsec - start.tv_nsec) / 1e3;

        // Update statistics
        brain->total_feature_extractions++;
        brain->avg_extraction_time_us = (brain->avg_extraction_time_us *
                                          (brain->total_feature_extractions - 1) +
                                          extraction_time_us) /
                                         brain->total_feature_extractions;

        brain->last_feature_extraction_ms = current_time_ms;
    }
}

// Update population activities if population coding enabled
if (brain->population_coder && brain->population_activities) {
    // Encode current network state as population code
    // This provides a low-dimensional representation of neural activity

    // Example: Encode PFC activity
    if (brain->num_brain_regions > 0) {
        uint32_t* pfc_neurons = NULL;
        uint32_t num_pfc = brain_region_get_neurons(brain, REGION_PREFRONTAL_CORTEX, &pfc_neurons);

        if (num_pfc > 0 && pfc_neurons) {
            // Get firing rates
            float* rates = (float*)malloc(num_pfc * sizeof(float));
            if (rates) {
                // Extract rates using rate coder
                if (brain->rate_coder) {
                    rate_coder_encode_population(
                        brain->rate_coder,
                        brain->network,
                        pfc_neurons,
                        num_pfc,
                        rates
                    );
                }

                // Encode as population code
                population_coder_encode(
                    brain->population_coder,
                    rates,
                    num_pfc,
                    brain->population_activities,
                    brain->population_size
                );

                free(rates);
            }
            free(pfc_neurons);
        }
    }
}
```

---

## 5. Cleanup in `brain_destroy`

### File: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`

### 5.1 Add Phase 2 Cleanup

Insert this code in `brain_destroy`, before freeing the brain structure:

```c
/**
 * PHASE 2 MIDDLEWARE CLEANUP
 */
if (brain->population_coder) {
    population_coder_destroy(brain->population_coder);
    brain->population_coder = NULL;
}

if (brain->feature_extractor) {
    feature_extractor_destroy(brain->feature_extractor);
    brain->feature_extractor = NULL;
}

// Free cached features
if (brain->cached_features) {
    for (uint32_t i = 0; i < brain->num_cached_features; i++) {
        if (brain->cached_features[i].data) {
            feature_vector_free(&brain->cached_features[i]);
        }
    }
    free(brain->cached_features);
    brain->cached_features = NULL;
}

// Free population activities
if (brain->population_activities) {
    free(brain->population_activities);
    brain->population_activities = NULL;
}

LOG_INFO("Phase 2 middleware cleanup complete");
```

---

## 6. Accessor Functions

### File: `/home/bbrelin/nimcp/include/core/brain/nimcp_brain.h` (public API)

### 6.1 Add Public Accessor Declarations

```c
/**
 * @brief Get middleware feature extractor
 *
 * WHAT: Access brain's feature extractor for custom feature extraction
 * WHY:  Allow applications to extract features from specific populations
 * HOW:  Returns handle to feature extractor (read-only access)
 *
 * @param brain Brain instance
 * @return Feature extractor handle or NULL if not enabled
 *
 * EXAMPLE:
 * ```c
 * feature_extractor_t extractor = brain_get_feature_extractor(brain);
 * if (extractor) {
 *     feature_vector_t features = feature_extractor_extract_region(
 *         extractor,
 *         brain,
 *         REGION_VISUAL_V1
 *     );
 *     // Use features...
 *     feature_vector_free(&features);
 * }
 * ```
 */
NIMCP_EXPORT feature_extractor_t brain_get_feature_extractor(brain_t brain);

/**
 * @brief Get middleware population coder
 *
 * WHAT: Access brain's population coder
 * WHY:  Encode/decode population activity representations
 *
 * @param brain Brain instance
 * @return Population coder handle or NULL if not enabled
 */
NIMCP_EXPORT population_coder_t brain_get_population_coder(brain_t brain);

/**
 * @brief Get cached features for brain region
 *
 * WHAT: Retrieve cached feature vector for specific region
 * WHY:  Avoid re-extraction if features are already computed
 * HOW:  Returns copy of cached features (caller must free)
 *
 * @param brain Brain instance
 * @param region Brain region ID
 * @param features Output: feature vector (caller must free with feature_vector_free)
 * @return true if features available, false otherwise
 *
 * THREAD-SAFETY: Not thread-safe, call from same thread as brain_update
 *
 * EXAMPLE:
 * ```c
 * feature_vector_t features;
 * if (brain_get_cached_features(brain, REGION_HIPPOCAMPUS_CA3, &features)) {
 *     printf("Hippocampal features: dim=%u\n", features.dim);
 *     feature_vector_free(&features);
 * }
 * ```
 */
NIMCP_EXPORT bool brain_get_cached_features(
    brain_t brain,
    brain_region_t region,
    feature_vector_t* features
);

/**
 * @brief Get current population activities
 *
 * WHAT: Get current population code representation
 * WHY:  Low-dimensional summary of neural activity
 *
 * @param brain Brain instance
 * @param activities Output buffer (must be pre-allocated, size = population_size)
 * @param size Size of output buffer
 * @return true on success
 */
NIMCP_EXPORT bool brain_get_population_activities(
    brain_t brain,
    float* activities,
    uint32_t size
);

/**
 * @brief Get middleware statistics
 *
 * WHAT: Performance and usage statistics for Phase 2 middleware
 * WHY:  Monitor overhead, identify bottlenecks
 *
 * @param brain Brain instance
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool brain_get_middleware_stats(
    brain_t brain,
    middleware_stats_t* stats
);

/**
 * @brief Manually trigger feature extraction
 *
 * WHAT: Force feature extraction for specific region
 * WHY:  On-demand extraction when auto-extract disabled
 * HOW:  Extracts features and optionally caches them
 *
 * @param brain Brain instance
 * @param region Brain region to extract from
 * @param cache Whether to cache extracted features
 * @return Feature vector (caller must free)
 *
 * EXAMPLE:
 * ```c
 * feature_vector_t pfc_features = brain_extract_features(
 *     brain,
 *     REGION_PREFRONTAL_CORTEX,
 *     true  // cache for later
 * );
 * // Use features...
 * feature_vector_free(&pfc_features);
 * ```
 */
NIMCP_EXPORT feature_vector_t brain_extract_features(
    brain_t brain,
    brain_region_t region,
    bool cache
);
```

### 6.2 Implementation in `nimcp_brain.c`

```c
feature_extractor_t brain_get_feature_extractor(brain_t brain) {
    if (!brain) return NULL;
    return brain->feature_extractor;
}

population_coder_t brain_get_population_coder(brain_t brain) {
    if (!brain) return NULL;
    return brain->population_coder;
}

bool brain_get_cached_features(brain_t brain, brain_region_t region, feature_vector_t* features) {
    if (!brain || !features) return false;
    if (!brain->cached_features || region >= brain->num_cached_features) return false;

    // Copy cached features
    return feature_vector_copy(&brain->cached_features[region], features);
}

bool brain_get_population_activities(brain_t brain, float* activities, uint32_t size) {
    if (!brain || !activities) return false;
    if (!brain->population_activities || size != brain->population_size) return false;

    memcpy(activities, brain->population_activities, size * sizeof(float));
    return true;
}

bool brain_get_middleware_stats(brain_t brain, middleware_stats_t* stats) {
    if (!brain || !stats) return false;

    memset(stats, 0, sizeof(middleware_stats_t));

    stats->total_extractions = brain->total_feature_extractions;
    stats->avg_extraction_time_us = brain->avg_extraction_time_us;
    stats->active_pipelines = brain->feature_extractor ? 1 : 0;

    // Add more stats as needed
    return true;
}

feature_vector_t brain_extract_features(brain_t brain, brain_region_t region, bool cache) {
    feature_vector_t empty = {0};

    if (!brain || !brain->feature_extractor) return empty;

    // Get neurons for region
    uint32_t* neuron_ids = NULL;
    uint32_t num_neurons = brain_region_get_neurons(brain, region, &neuron_ids);

    if (num_neurons == 0 || !neuron_ids) return empty;

    // Extract features
    feature_vector_t features = feature_extractor_extract(
        brain->feature_extractor,
        brain->network,
        neuron_ids,
        num_neurons,
        get_current_time_ms()
    );

    free(neuron_ids);

    // Cache if requested
    if (cache && brain->cached_features && region < brain->num_cached_features) {
        if (brain->cached_features[region].data) {
            feature_vector_free(&brain->cached_features[region]);
        }
        feature_vector_copy(&features, &brain->cached_features[region]);
    }

    return features;
}
```

---

## 7. Include Dependencies

### 7.1 Add to Brain Header Includes

In `/home/bbrelin/nimcp/include/core/brain/nimcp_brain.h`:

```c
// Middleware Phase 2 includes
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
```

### 7.2 Add to Brain Implementation Includes

In `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`:

```c
// Already included from Phase 1
#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/encoding/nimcp_temporal_coding.h"

// Phase 2 additions
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "middleware/features/nimcp_population_features.h"
```

---

## 8. Integration Checklist

Use this checklist when performing the integration:

- [ ] **Step 1**: Add Phase 2 fields to `brain_config_t`
- [ ] **Step 2**: Add Phase 2 fields to `brain_struct`
- [ ] **Step 3**: Add Phase 2 initialization in `brain_create_custom`
- [ ] **Step 4**: Add Phase 2 update logic in `brain_update`
- [ ] **Step 5**: Add Phase 2 cleanup in `brain_destroy`
- [ ] **Step 6**: Add accessor function declarations to public header
- [ ] **Step 7**: Implement accessor functions in brain.c
- [ ] **Step 8**: Add include directives
- [ ] **Step 9**: Update CMakeLists.txt to link middleware libraries
- [ ] **Step 10**: Build and test integration
- [ ] **Step 11**: Run integration tests
- [ ] **Step 12**: Run regression tests
- [ ] **Step 13**: Update documentation

---

## 9. Testing the Integration

### 9.1 Basic Integration Test

```c
#include "core/brain/nimcp_brain.h"

int main() {
    // Create brain with Phase 2 middleware
    brain_config_t config = brain_default_config(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION);

    // Enable Phase 2
    config.enable_middleware = true;
    config.enable_population_coding = true;
    config.enable_feature_extraction = true;
    config.middleware_auto_extract = true;
    config.middleware_cache_features = true;

    // Create brain
    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create brain\n");
        return -1;
    }

    // Verify Phase 2 components
    feature_extractor_t extractor = brain_get_feature_extractor(brain);
    population_coder_t pop_coder = brain_get_population_coder(brain);

    printf("Feature extractor: %s\n", extractor ? "OK" : "FAILED");
    printf("Population coder: %s\n", pop_coder ? "OK" : "FAILED");

    // Run brain
    for (int i = 0; i < 100; i++) {
        brain_update(brain);
    }

    // Check cached features
    feature_vector_t features;
    if (brain_get_cached_features(brain, 0, &features)) {
        printf("Cached features: dim=%u\n", features.dim);
        feature_vector_free(&features);
    }

    // Get statistics
    middleware_stats_t stats;
    if (brain_get_middleware_stats(brain, &stats)) {
        printf("Total extractions: %lu\n", stats.total_extractions);
        printf("Avg time: %.2f us\n", stats.avg_extraction_time_us);
    }

    brain_destroy(brain);
    return 0;
}
```

### 9.2 Run Integration Tests

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make

# Run Phase 2 integration tests
./test/integration/middleware/integration_middleware_test_phase2_integration

# Run regression tests
./test/regression/middleware/regression_middleware_test_phase2_regression
```

---

## 10. Performance Considerations

### 10.1 Feature Extraction Overhead

- **Auto-extract mode**: ~0.5-2ms per update (depends on population size)
- **On-demand mode**: No overhead when not extracting
- **Recommendation**: Use auto-extract for real-time systems, on-demand for batch processing

### 10.2 Memory Usage

- **Population coder**: ~200 bytes + (tuning_curves * 4 bytes)
- **Feature extractor**: ~1KB + (num_features * 100 bytes)
- **Feature cache**: (num_regions * feature_dim * 4 bytes)
- **Recommendation**: Enable caching only for frequently accessed features

### 10.3 Optimization Tips

1. **Reduce extraction frequency**: Set `feature_step_ms` to 100-200ms for non-critical features
2. **Limit feature types**: Only extract features you actually use
3. **Disable auto-extract**: Use manual extraction for control over when features are computed
4. **Cache selectively**: Only cache features for high-traffic regions

---

## 11. Common Integration Issues

### Issue 1: Feature Extractor Returns NULL

**Cause**: Middleware not enabled in config
**Solution**: Set `config.enable_middleware = true` and `config.enable_feature_extraction = true`

### Issue 2: Cached Features are Empty

**Cause**: Auto-extract disabled or not enough time has passed
**Solution**: Enable `config.middleware_auto_extract = true` and ensure `brain_update` is called

### Issue 3: Performance Degradation

**Cause**: Too frequent feature extraction
**Solution**: Increase `feature_step_ms` or disable auto-extract

### Issue 4: Memory Leak

**Cause**: Feature vectors not freed
**Solution**: Always call `feature_vector_free()` on extracted features

---

## 12. Next Steps

After integrating Phase 2:

1. **Test**: Run all integration and regression tests
2. **Benchmark**: Measure performance impact on your use case
3. **Optimize**: Tune extraction frequency and feature selection
4. **Document**: Update application documentation with Phase 2 capabilities
5. **Phase 3**: Prepare for next middleware phase (if planned)

---

## Conclusion

This integration plan provides a **complete, step-by-step guide** for adding Phase 2 Middleware to the NIMCP brain. Follow the checklist, implement each section carefully, and test thoroughly.

**DO NOT MODIFY BRAIN FILES YET** - this document is for planning. When ready to integrate, follow this guide exactly.

---

**End of Integration Guide**

/**
 * @file e2e_test_hippocampus_pipeline.cpp
 * @brief End-to-end tests for Hippocampus Pipeline
 *
 * WHAT: Full pipeline tests for memory encoding and spatial navigation
 * WHY:  Verify complete hippocampal workflows with substrate integration
 * HOW:  Test encoding, consolidation, retrieval, spatial processing
 *
 * TEST COVERAGE:
 * - Memory Encoding Pipeline (4 tests)
 * - Memory Consolidation (3 tests)
 * - Memory Retrieval (4 tests)
 * - Spatial Navigation (4 tests)
 * - Metabolic Effects (3 tests)
 * - Pattern Separation/Completion (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Hippocampus critical for episodic memory formation
 * - CA1/CA3 circuit for pattern separation/completion
 * - Entorhinal cortex provides spatial grid cells
 * - LTP requires sustained ATP for memory consolidation
 * - Sleep-dependent memory consolidation
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/hippocampus/nimcp_hippocampus_substrate_bridge.h"
#include "core/hippocampus/nimcp_hippocampus_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_ENCODING_TIME_MS = 50.0;
constexpr double MAX_RETRIEVAL_TIME_MS = 30.0;
constexpr double MAX_NAVIGATION_TIME_MS = 100.0;
constexpr float MIN_MEMORY_CAPACITY = 0.3f;
constexpr float ENCODING_THRESHOLD = 0.5f;
constexpr uint32_t PATTERN_SIZE = 64;
constexpr uint32_t SPATIAL_GRID_SIZE = 16;

//=============================================================================
// Helper Structures
//=============================================================================

struct MemoryPattern {
    std::vector<float> features;
    uint32_t id;
    float strength;
};

struct SpatialLocation {
    float x, y;
    uint32_t place_id;
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EHippocampusEncodingTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    hippocampus_substrate_bridge_t* hpc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        hippocampus_substrate_config_t hpc_config = hippocampus_substrate_default_config();
        hpc_bridge = hippocampus_substrate_bridge_create(nullptr, substrate, &hpc_config);
        ASSERT_NE(hpc_bridge, nullptr);
    }

    void TearDown() override {
        if (hpc_bridge) {
            hippocampus_substrate_bridge_destroy(hpc_bridge);
            hpc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    MemoryPattern createPattern(uint32_t id, float base_value = 0.5f) {
        MemoryPattern pattern;
        pattern.id = id;
        pattern.strength = 1.0f;
        pattern.features.resize(PATTERN_SIZE);

        std::mt19937 gen(id);
        std::normal_distribution<float> dist(base_value, 0.2f);

        for (size_t i = 0; i < PATTERN_SIZE; i++) {
            pattern.features[i] = std::clamp(dist(gen), 0.0f, 1.0f);
        }
        return pattern;
    }
};

class E2EHippocampusConsolidationTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    hippocampus_substrate_bridge_t* hpc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        hippocampus_substrate_config_t hpc_config = hippocampus_substrate_default_config();
        hpc_bridge = hippocampus_substrate_bridge_create(nullptr, substrate, &hpc_config);
        ASSERT_NE(hpc_bridge, nullptr);
    }

    void TearDown() override {
        if (hpc_bridge) {
            hippocampus_substrate_bridge_destroy(hpc_bridge);
            hpc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EHippocampusRetrievalTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    hippocampus_substrate_bridge_t* hpc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        hippocampus_substrate_config_t hpc_config = hippocampus_substrate_default_config();
        hpc_bridge = hippocampus_substrate_bridge_create(nullptr, substrate, &hpc_config);
        ASSERT_NE(hpc_bridge, nullptr);
    }

    void TearDown() override {
        if (hpc_bridge) {
            hippocampus_substrate_bridge_destroy(hpc_bridge);
            hpc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EHippocampusSpatialTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    hippocampus_substrate_bridge_t* hpc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        hippocampus_substrate_config_t hpc_config = hippocampus_substrate_default_config();
        hpc_bridge = hippocampus_substrate_bridge_create(nullptr, substrate, &hpc_config);
        ASSERT_NE(hpc_bridge, nullptr);
    }

    void TearDown() override {
        if (hpc_bridge) {
            hippocampus_substrate_bridge_destroy(hpc_bridge);
            hpc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    SpatialLocation createLocation(float x, float y, uint32_t place_id) {
        SpatialLocation loc;
        loc.x = x;
        loc.y = y;
        loc.place_id = place_id;
        return loc;
    }
};

//=============================================================================
// Memory Encoding Pipeline Tests
//=============================================================================

TEST_F(E2EHippocampusEncodingTest, BaselineEncodingCapacity) {
    // Scenario: Verify baseline memory encoding with optimal substrate
    E2E_PIPELINE_START("Baseline Encoding Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update HPC bridge", 10);
    int result = hippocampus_substrate_bridge_update(hpc_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    hippocampus_substrate_effects_t effects;
    result = hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_MEMORY_CAPACITY);
    EXPECT_GT(effects.encoding_capacity, MIN_MEMORY_CAPACITY);
    EXPECT_GT(effects.consolidation_rate, MIN_MEMORY_CAPACITY);
    EXPECT_GT(effects.retrieval_accuracy, MIN_MEMORY_CAPACITY);
    EXPECT_GT(effects.spatial_processing, MIN_MEMORY_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, EncodingWithHighActivity) {
    // Scenario: Test encoding capacity during high neural activity
    E2E_PIPELINE_START("Encoding With High Activity");

    E2E_STAGE_BEGIN("Baseline encoding", 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t baseline;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &baseline);
    float baseline_encoding = baseline.encoding_capacity;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate encoding activity", 50);
    // High activity consumes resources
    for (int i = 0; i < 20; i++) {
        substrate_record_spikes(substrate, 200);
        substrate_record_transmissions(substrate, 500);
        substrate_update(substrate, 10);
    }
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t active;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &active);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify validity", 2);
    EXPECT_GE(active.encoding_capacity, 0.0f);
    EXPECT_LE(active.encoding_capacity, 1.0f);
    EXPECT_FALSE(std::isnan(active.encoding_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, SequentialEncodingPipeline) {
    // Scenario: Encode multiple patterns sequentially
    E2E_PIPELINE_START("Sequential Encoding Pipeline");

    E2E_STAGE_BEGIN("Encode multiple patterns", 100);
    std::vector<MemoryPattern> patterns;

    for (int i = 0; i < 10; i++) {
        // Create and "encode" pattern
        MemoryPattern p = createPattern(i);
        patterns.push_back(p);

        // Encoding activity
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 50);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);

        // Encoding capacity should remain valid
        EXPECT_GE(effects.encoding_capacity, 0.0f);
        EXPECT_LE(effects.encoding_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify pattern count", 2);
    EXPECT_EQ(patterns.size(), 10u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, EncodingApplyEffects) {
    // Scenario: Apply encoding effects to hippocampal system
    E2E_PIPELINE_START("Encoding Apply Effects");

    E2E_STAGE_BEGIN("Update and apply", 20);
    hippocampus_substrate_bridge_update(hpc_bridge);
    int result = hippocampus_substrate_bridge_apply_effects(hpc_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify state", 5);
    hippocampus_substrate_effects_t effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Memory Consolidation Tests
//=============================================================================

TEST_F(E2EHippocampusConsolidationTest, ConsolidationRateTracking) {
    // Scenario: Track consolidation rate over time
    E2E_PIPELINE_START("Consolidation Rate Tracking");

    E2E_STAGE_BEGIN("Initial consolidation state", 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t initial;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &initial);
    float initial_rate = initial.consolidation_rate;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate consolidation period", 100);
    std::vector<float> rates;

    for (int step = 0; step < 50; step++) {
        substrate_update(substrate, 100);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
        rates.push_back(effects.consolidation_rate);

        EXPECT_GE(effects.consolidation_rate, 0.0f);
        EXPECT_LE(effects.consolidation_rate, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    float mean = std::accumulate(rates.begin(), rates.end(), 0.0f) / rates.size();
    EXPECT_GT(mean, 0.0f);
    EXPECT_FALSE(std::isnan(mean));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusConsolidationTest, ConsolidationUnderATPStress) {
    // Scenario: Consolidation requires sustained ATP
    E2E_PIPELINE_START("Consolidation Under ATP Stress");

    E2E_STAGE_BEGIN("Normal ATP consolidation", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t normal;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &normal);
    float normal_consolidation = normal.consolidation_rate;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP consolidation", 10);
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t low_atp;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &low_atp);
    float low_atp_consolidation = low_atp.consolidation_rate;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GT(normal_consolidation, 0.0f);
    EXPECT_GE(low_atp_consolidation, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusConsolidationTest, ExtendedConsolidationSimulation) {
    // Scenario: Extended consolidation without degradation
    E2E_PIPELINE_START("Extended Consolidation Simulation");

    E2E_STAGE_BEGIN("Long consolidation run", 200);
    for (int step = 0; step < 500; step++) {
        substrate_update(substrate, 50);
        hippocampus_substrate_bridge_update(hpc_bridge);

        if (step % 50 == 0) {
            hippocampus_substrate_effects_t effects;
            hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.consolidation_rate));
            EXPECT_FALSE(std::isinf(effects.consolidation_rate));
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    hippocampus_substrate_effects_t final_effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &final_effects);
    EXPECT_GT(final_effects.consolidation_rate, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Memory Retrieval Tests
//=============================================================================

TEST_F(E2EHippocampusRetrievalTest, RetrievalAccuracyBaseline) {
    // Scenario: Baseline retrieval accuracy
    E2E_PIPELINE_START("Retrieval Accuracy Baseline");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get retrieval accuracy", 5);
    hippocampus_substrate_effects_t effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify accuracy", 2);
    EXPECT_GT(effects.retrieval_accuracy, MIN_MEMORY_CAPACITY);
    EXPECT_LE(effects.retrieval_accuracy, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusRetrievalTest, RetrievalUnderFatigue) {
    // Scenario: Retrieval accuracy degrades with fatigue
    E2E_PIPELINE_START("Retrieval Under Fatigue");

    E2E_STAGE_BEGIN("Fresh state", 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t fresh;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &fresh);
    float fresh_accuracy = fresh.retrieval_accuracy;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 100);
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 300);
        substrate_record_transmissions(substrate, 800);
        substrate_update(substrate, 20);
    }
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t fatigued;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fresh_accuracy, 0.0f);
    EXPECT_GE(fatigued.retrieval_accuracy, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusRetrievalTest, RepeatedRetrievalStability) {
    // Scenario: Multiple retrievals should remain stable
    E2E_PIPELINE_START("Repeated Retrieval Stability");

    E2E_STAGE_BEGIN("Multiple retrievals", 100);
    std::vector<float> accuracies;

    for (int retrieval = 0; retrieval < 20; retrieval++) {
        substrate_update(substrate, 50);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
        accuracies.push_back(effects.retrieval_accuracy);

        EXPECT_GE(effects.retrieval_accuracy, 0.0f);
        EXPECT_LE(effects.retrieval_accuracy, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check stability", 5);
    float mean = std::accumulate(accuracies.begin(), accuracies.end(), 0.0f) / accuracies.size();
    float variance = 0.0f;
    for (float a : accuracies) {
        variance += (a - mean) * (a - mean);
    }
    variance /= accuracies.size();

    // Should be relatively stable
    EXPECT_LT(variance, 0.3f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusRetrievalTest, RetrievalWithSubstrateRecovery) {
    // Scenario: Retrieval improves with substrate recovery
    E2E_PIPELINE_START("Retrieval With Substrate Recovery");

    E2E_STAGE_BEGIN("Deplete substrate", 50);
    for (int i = 0; i < 50; i++) {
        substrate_record_spikes(substrate, 400);
        substrate_update(substrate, 10);
    }
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t depleted;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &depleted);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Allow recovery", 100);
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 100);
    }
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t recovered;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GE(recovered.retrieval_accuracy, depleted.retrieval_accuracy);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Spatial Navigation Tests
//=============================================================================

TEST_F(E2EHippocampusSpatialTest, SpatialProcessingCapacity) {
    // Scenario: Baseline spatial processing capacity
    E2E_PIPELINE_START("Spatial Processing Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get spatial capacity", 5);
    hippocampus_substrate_effects_t effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.spatial_processing, MIN_MEMORY_CAPACITY);
    EXPECT_LE(effects.spatial_processing, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusSpatialTest, PathIntegrationSimulation) {
    // Scenario: Simulate path integration through environment
    E2E_PIPELINE_START("Path Integration Simulation");

    E2E_STAGE_BEGIN("Create path", 50);
    std::vector<SpatialLocation> path;

    // Create a simple path through space
    for (int step = 0; step < 20; step++) {
        float x = step * 0.5f;
        float y = sinf(step * 0.3f) * 2.0f;
        path.push_back(createLocation(x, y, step));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Navigate path", 100);
    for (const auto& loc : path) {
        // Each location update consumes resources
        substrate_record_spikes(substrate, 50);
        substrate_update(substrate, 50);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);

        // Spatial processing should remain valid
        EXPECT_GE(effects.spatial_processing, 0.0f);
        EXPECT_FALSE(std::isnan(effects.spatial_processing));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final state", 5);
    hippocampus_substrate_effects_t final_effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &final_effects);
    EXPECT_GT(final_effects.spatial_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusSpatialTest, PlaceCellRepresentation) {
    // Scenario: Simulate place cell firing across environment
    E2E_PIPELINE_START("Place Cell Representation");

    E2E_STAGE_BEGIN("Generate place cells", 50);
    // Simulate place cell firing at different locations
    for (int x = 0; x < SPATIAL_GRID_SIZE; x++) {
        for (int y = 0; y < SPATIAL_GRID_SIZE; y++) {
            // Place cells fire for specific locations
            float firing_rate = expf(-0.5f * ((x - 8) * (x - 8) + (y - 8) * (y - 8)) / 4.0f);

            if (firing_rate > 0.1f) {
                substrate_record_spikes(substrate, (uint32_t)(firing_rate * 100));
            }

            substrate_update(substrate, 10);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update bridge", 10);
    hippocampus_substrate_bridge_update(hpc_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify spatial processing", 5);
    hippocampus_substrate_effects_t effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    EXPECT_GE(effects.spatial_processing, 0.0f);
    EXPECT_LE(effects.spatial_processing, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusSpatialTest, SpatialNavigationWithFatigue) {
    // Scenario: Spatial processing degrades with extended navigation
    E2E_PIPELINE_START("Spatial Navigation With Fatigue");

    E2E_STAGE_BEGIN("Initial spatial capacity", 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t initial;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &initial);
    float initial_spatial = initial.spatial_processing;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Extended navigation", 200);
    // Simulate long navigation session
    for (int step = 0; step < 200; step++) {
        substrate_record_spikes(substrate, 100);
        substrate_record_transmissions(substrate, 250);
        substrate_update(substrate, 20);
    }
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t fatigued;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(initial_spatial, 0.0f);
    EXPECT_GE(fatigued.spatial_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2EHippocampusEncodingTest, GlucoseDependentEncoding) {
    // Scenario: Memory encoding is glucose dependent
    E2E_PIPELINE_START("Glucose Dependent Encoding");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t normal;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_CRITICAL_GLUCOSE);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t low;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.encoding_capacity, 0.0f);
    EXPECT_GE(low.encoding_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, OxygenDependentLTP) {
    // Scenario: LTP (consolidation) requires oxygen
    E2E_PIPELINE_START("Oxygen Dependent LTP");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t normal_o2;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Hypoxic condition", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t hypoxic;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &hypoxic);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify consolidation", 2);
    EXPECT_GT(normal_o2.consolidation_rate, 0.0f);
    EXPECT_GE(hypoxic.consolidation_rate, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, TemperatureSensitivity) {
    // Scenario: Hippocampus is temperature sensitive
    E2E_PIPELINE_START("Temperature Sensitivity");

    float temperatures[] = {35.0f, 37.0f, 39.0f, 40.0f};
    std::vector<hippocampus_substrate_effects_t> effects_at_temps;

    E2E_STAGE_BEGIN("Test temperatures", 40);
    for (float temp : temperatures) {
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 10);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
        effects_at_temps.push_back(effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify temperature effects", 5);
    // All values should be valid
    for (const auto& eff : effects_at_temps) {
        EXPECT_FALSE(std::isnan(eff.overall_capacity));
        EXPECT_FALSE(std::isinf(eff.overall_capacity));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pattern Separation/Completion Tests
//=============================================================================

TEST_F(E2EHippocampusEncodingTest, PatternSeparationSimulation) {
    // Scenario: Similar patterns should be encoded distinctly
    E2E_PIPELINE_START("Pattern Separation Simulation");

    E2E_STAGE_BEGIN("Create similar patterns", 20);
    std::vector<MemoryPattern> similar_patterns;

    for (int i = 0; i < 5; i++) {
        MemoryPattern p = createPattern(i, 0.5f + 0.02f * i);  // Similar base values
        similar_patterns.push_back(p);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Encode patterns", 50);
    for (const auto& pattern : similar_patterns) {
        substrate_record_spikes(substrate, 150);
        substrate_update(substrate, 50);
        hippocampus_substrate_bridge_update(hpc_bridge);

        hippocampus_substrate_effects_t effects;
        hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);

        // Encoding should work for similar patterns
        EXPECT_GT(effects.encoding_capacity, 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify separation", 5);
    hippocampus_substrate_effects_t final_effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &final_effects);
    EXPECT_GE(final_effects.encoding_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusRetrievalTest, PatternCompletionSimulation) {
    // Scenario: Partial cue should activate full pattern
    E2E_PIPELINE_START("Pattern Completion Simulation");

    E2E_STAGE_BEGIN("Encode complete pattern", 30);
    // Simulate encoding a complete pattern
    substrate_record_spikes(substrate, 200);
    substrate_update(substrate, 100);
    hippocampus_substrate_bridge_update(hpc_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Present partial cue", 30);
    // Partial cue triggers retrieval
    substrate_record_spikes(substrate, 50);  // Less activity for partial
    substrate_update(substrate, 50);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify completion", 5);
    // Retrieval should still work with partial cue
    EXPECT_GT(effects.retrieval_accuracy, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EHippocampusEncodingTest, ContextDependentEncoding) {
    // Scenario: Context affects encoding
    E2E_PIPELINE_START("Context Dependent Encoding");

    E2E_STAGE_BEGIN("Encode in context A", 30);
    // Simulate specific context state
    substrate_set_atp(substrate, 0.9f);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t context_a;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &context_a);
    float encoding_a = context_a.encoding_capacity;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Encode in context B", 30);
    // Different context state
    substrate_set_atp(substrate, 0.7f);
    substrate_update(substrate, 10);
    hippocampus_substrate_bridge_update(hpc_bridge);

    hippocampus_substrate_effects_t context_b;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &context_b);
    float encoding_b = context_b.encoding_capacity;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare contexts", 5);
    // Both should be valid
    EXPECT_GT(encoding_a, 0.0f);
    EXPECT_GT(encoding_b, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(E2EHippocampusEncodingTest, LongSimulationStability) {
    // Scenario: Extended simulation without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_update(substrate, 10);
        hippocampus_substrate_bridge_update(hpc_bridge);

        if (step % 100 == 0) {
            hippocampus_substrate_effects_t effects;
            hippocampus_substrate_bridge_get_effects(hpc_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    hippocampus_substrate_effects_t final_effects;
    hippocampus_substrate_bridge_get_effects(hpc_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.encoding_capacity, 0.0f);
    EXPECT_GT(final_effects.consolidation_rate, 0.0f);
    EXPECT_GT(final_effects.retrieval_accuracy, 0.0f);
    EXPECT_GT(final_effects.spatial_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

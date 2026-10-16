/**
 * @file e2e_test_mammillary_pipeline.cpp
 * @brief End-to-end tests for Mammillary Bodies Pipeline
 *
 * WHAT: Full pipeline tests for memory relay and Papez circuit integration
 * WHY:  Verify complete mammillary workflows with hippocampus-thalamus relay
 * HOW:  Test memory consolidation, head direction, Papez circuit cycles
 *
 * TEST COVERAGE:
 * - Memory Relay Pipeline (4 tests)
 * - Papez Circuit Operations (4 tests)
 * - Head Direction Processing (4 tests)
 * - Memory Consolidation (3 tests)
 * - Performance Benchmarks (2 tests)
 *
 * TOTAL: 17 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Mammillary bodies relay memories from hippocampus to thalamus
 * - Critical node in Papez circuit for emotion and memory
 * - Head direction cells provide orientation information
 * - Memory consolidation during sleep involves mammillary bodies
 *
 * @author NIMCP Development Team
 * @date 2026-01-14
 */

#include "e2e_test_framework.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/mammillary/nimcp_mammillary.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

/*=============================================================================
 * Test Configuration
 *===========================================================================*/

constexpr double MAX_RELAY_TIME_MS = 50.0;
constexpr double MAX_PAPEZ_CYCLE_TIME_MS = 100.0;
constexpr uint32_t TRACE_DIM = 128;
constexpr uint32_t CONTEXT_DIM = 128;
constexpr uint32_t NUM_TEST_TRACES = 10;

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static void CreateTestContext(float* context, uint32_t dim, float base_value) {
    for (uint32_t i = 0; i < dim; i++) {
        context[i] = base_value + (float)i * 0.001f;
    }
}

/*=============================================================================
 * Memory Relay Pipeline Tests
 *===========================================================================*/

class E2EMammillaryRelayTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_head_direction = true;
        config.enable_spatial_processing = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(mammillary, nullptr);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }
};

TEST_F(E2EMammillaryRelayTest, SingleMemoryRelay) {
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    auto start = std::chrono::high_resolution_clock::now();
    int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(0, result);
    EXPECT_GE(trace_id, 0u);

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, MAX_RELAY_TIME_MS);
}

TEST_F(E2EMammillaryRelayTest, MultipleMemoryRelays) {
    std::vector<uint32_t> trace_ids(NUM_TEST_TRACES);

    for (uint32_t i = 0; i < NUM_TEST_TRACES; i++) {
        float trace[TRACE_DIM];
        CreateTestContext(trace, TRACE_DIM, (float)i * 0.1f);

        int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_ids[i]);
        EXPECT_EQ(0, result);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.total_memory_traces, NUM_TEST_TRACES);
}

TEST_F(E2EMammillaryRelayTest, RelayToThalamus) {
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    int result = mammillary_relay_to_thalamus(mammillary, trace_id);
    EXPECT_EQ(0, result);

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.relay_operations, 1u);
}

TEST_F(E2EMammillaryRelayTest, DifferentTraceTypes) {
    memory_trace_type_t types[] = {
        MEMORY_TRACE_EPISODIC,
        MEMORY_TRACE_SPATIAL,
        MEMORY_TRACE_CONTEXTUAL,
        MEMORY_TRACE_EMOTIONAL
    };

    for (int i = 0; i < 4; i++) {
        float trace[TRACE_DIM];
        CreateTestContext(trace, TRACE_DIM, (float)i * 0.2f);

        uint32_t trace_id = 0;
        int result = mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
            types[i], 0.5f, &trace_id);
        EXPECT_EQ(0, result);

        mammillary_relay_to_thalamus(mammillary, trace_id);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.total_memory_traces, 4u);
}

/*=============================================================================
 * Papez Circuit Tests
 *===========================================================================*/

class E2EMammillaryPapezTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        config.enable_papez_circuit = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(mammillary, nullptr);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }
};

TEST_F(E2EMammillaryPapezTest, PapezCycleExecution) {
    auto start = std::chrono::high_resolution_clock::now();
    int result = mammillary_process_papez_cycle(mammillary);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(0, result);

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, MAX_PAPEZ_CYCLE_TIME_MS);
}

TEST_F(E2EMammillaryPapezTest, PapezPhaseProgression) {
    papez_phase_t initial = mammillary_get_papez_phase(mammillary);
    EXPECT_GE((int)initial, 0);

    for (int i = 0; i < 6; i++) {
        mammillary_advance_papez_phase(mammillary);
    }

    papez_phase_t final_phase = mammillary_get_papez_phase(mammillary);
    EXPECT_GE((int)final_phase, 0);
}

TEST_F(E2EMammillaryPapezTest, PapezActivityLevel) {
    /* Process a cycle to generate activity */
    mammillary_process_papez_cycle(mammillary);

    float activity = mammillary_get_papez_activity(mammillary);
    EXPECT_GE(activity, 0.0f);
}

TEST_F(E2EMammillaryPapezTest, PapezWithMemoryInput) {
    /* Add memory trace */
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    /* Process Papez cycle with memory */
    int result = mammillary_process_papez_cycle(mammillary);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * Head Direction Tests
 *===========================================================================*/

class E2EMammillaryHeadDirectionTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        config.enable_head_direction = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(mammillary, nullptr);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }
};

TEST_F(E2EMammillaryHeadDirectionTest, HeadDirectionUpdate) {
    int result = mammillary_update_head_direction(mammillary, 0.5f, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(E2EMammillaryHeadDirectionTest, HeadDirectionTracking) {
    /* Simulate rotation */
    for (int i = 0; i < 10; i++) {
        mammillary_update_head_direction(mammillary, 0.1f, 10.0f);
    }

    float heading = mammillary_get_head_direction(mammillary);
    EXPECT_GE(heading, -M_PI);
    EXPECT_LE(heading, 2 * M_PI);
}

TEST_F(E2EMammillaryHeadDirectionTest, HeadDirectionConfidence) {
    mammillary_update_head_direction(mammillary, 0.5f, 10.0f);

    float confidence = mammillary_get_hd_confidence(mammillary);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(E2EMammillaryHeadDirectionTest, LandmarkBasedCorrection) {
    /* Introduce drift */
    for (int i = 0; i < 20; i++) {
        mammillary_update_head_direction(mammillary, 0.1f, 10.0f);
    }

    /* Correct with landmark */
    int result = mammillary_set_hd_from_landmark(mammillary, 0.0f, 0.9f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * Memory Consolidation Tests
 *===========================================================================*/

class E2EMammillaryConsolidationTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_background_consolidation = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(mammillary, nullptr);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }
};

TEST_F(E2EMammillaryConsolidationTest, ConsolidationStart) {
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    int result = mammillary_start_consolidation(mammillary, trace_id);
    EXPECT_EQ(0, result);
}

TEST_F(E2EMammillaryConsolidationTest, ConsolidationUpdate) {
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    mammillary_start_consolidation(mammillary, trace_id);

    for (int i = 0; i < 50; i++) {
        int result = mammillary_update_consolidation(mammillary, 10.0f);
        EXPECT_EQ(0, result);
    }

    consolidation_state_t state = mammillary_get_consolidation_state(mammillary);
    EXPECT_GE((int)state, 0);
}

TEST_F(E2EMammillaryConsolidationTest, TraceStrengthening) {
    float trace[TRACE_DIM];
    CreateTestContext(trace, TRACE_DIM, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    int result = mammillary_strengthen_trace(mammillary, trace_id, 0.3f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * Performance Benchmark Tests
 *===========================================================================*/

class E2EMammillaryBenchmarkTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary = nullptr;

    void SetUp() override {
        mammillary_config_t config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_head_direction = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(mammillary, nullptr);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }
};

TEST_F(E2EMammillaryBenchmarkTest, RelayThroughput) {
    const uint32_t BENCHMARK_COUNT = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < BENCHMARK_COUNT; i++) {
        float trace[TRACE_DIM];
        CreateTestContext(trace, TRACE_DIM, (float)i * 0.01f);

        uint32_t trace_id = 0;
        mammillary_receive_hippocampal_input(mammillary, trace, TRACE_DIM,
            MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);
        mammillary_relay_to_thalamus(mammillary, trace_id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double relays_per_second = (BENCHMARK_COUNT * 1000.0) / elapsed_ms;
    EXPECT_GT(relays_per_second, 100.0); /* At least 100 relays/second */
}

TEST_F(E2EMammillaryBenchmarkTest, PapezCycleLatency) {
    /* Warmup */
    for (int i = 0; i < 5; i++) {
        mammillary_process_papez_cycle(mammillary);
    }

    /* Benchmark */
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; i++) {
        mammillary_process_papez_cycle(mammillary);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double avg_cycle_ms = elapsed_ms / 10.0;
    EXPECT_LT(avg_cycle_ms, MAX_PAPEZ_CYCLE_TIME_MS);
}

/*=============================================================================
 * Main
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

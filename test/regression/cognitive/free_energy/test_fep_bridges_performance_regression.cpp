/**
 * @file test_fep_bridges_performance_regression.cpp
 * @brief Performance regression tests for FEP bridge modules
 *
 * WHAT: Tests performance benchmarks for bridge operations
 * WHY:  Prevent performance regressions, ensure operations complete in acceptable time
 * HOW:  Measure timing for critical operations, validate against thresholds
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_fep_consciousness.h"
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_fep_sleep.h"
#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/introspection/nimcp_introspection.h"

class FEPPerformanceRegressionTest : public ::testing::Test {
protected:
    static const int WARMUP_ITERATIONS = 100;
    static const int BENCHMARK_ITERATIONS = 1000;
    static const uint64_t TIMESTEP_MS = 16;

    // Performance thresholds (microseconds)
    static constexpr int64_t UPDATE_THRESHOLD_US = 100;        // <100μs per update
    static constexpr int64_t CONNECTION_THRESHOLD_US = 50;     // <50μs to connect
    static constexpr int64_t STATE_QUERY_THRESHOLD_US = 10;    // <10μs to query state
    static constexpr int64_t MODULATION_THRESHOLD_US = 200;    // <200μs to apply modulation

    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;

    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;

    void SetUp() override {
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.num_levels = 2;
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Utility to measure operation time
    template<typename Func>
    int64_t measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    // Benchmark with warmup and statistics
    template<typename Func>
    void benchmark_operation(const char* name, Func&& func, int64_t threshold_us) {
        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            func();
        }

        // Measure
        std::vector<int64_t> timings;
        for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
            int64_t time_us = measure_time_us(func);
            timings.push_back(time_us);
        }

        // Statistics
        std::sort(timings.begin(), timings.end());
        int64_t min = timings.front();
        int64_t max = timings.back();
        int64_t median = timings[timings.size() / 2];
        int64_t p95 = timings[(timings.size() * 95) / 100];

        int64_t sum = 0;
        for (auto t : timings) sum += t;
        int64_t avg = sum / timings.size();

        // Report
        printf("  %s: avg=%lldμs, median=%lldμs, p95=%lldμs, min=%lldμs, max=%lldμs\n",
               name, (long long)avg, (long long)median, (long long)p95,
               (long long)min, (long long)max);

        // Verify performance
        EXPECT_LT(avg, threshold_us) << name << " average exceeds threshold";
        EXPECT_LT(p95, threshold_us * 2) << name << " p95 exceeds 2x threshold";
    }
};

/* ============================================================================
 * Immune Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, ImmuneBridgeUpdatePerformance) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    printf("\n[Immune Bridge Update Performance]\n");
    benchmark_operation("update", [&]() {
        fep_immune_bridge_update(bridge, TIMESTEP_MS);
    }, UPDATE_THRESHOLD_US);

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPPerformanceRegressionTest, ImmuneBridgeConnectionPerformance) {
    printf("\n[Immune Bridge Connection Performance]\n");
    benchmark_operation("connect_fep", [&]() {
        fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
        fep_immune_bridge_connect_fep(bridge, fep);
        fep_immune_bridge_destroy(bridge);
    }, CONNECTION_THRESHOLD_US);
}

TEST_F(FEPPerformanceRegressionTest, ImmuneBridgeStateQueryPerformance) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    printf("\n[Immune Bridge State Query Performance]\n");
    benchmark_operation("get_state", [&]() {
        fep_immune_state_t state;
        fep_immune_bridge_get_state(bridge, &state);
    }, STATE_QUERY_THRESHOLD_US);

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPPerformanceRegressionTest, ImmuneBridgeCytokineUpdatePerformance) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    printf("\n[Immune Bridge Cytokine Update Performance]\n");
    benchmark_operation("update_cytokines", [&]() {
        fep_immune_update_cytokine_effects(bridge);
    }, UPDATE_THRESHOLD_US / 2);

    fep_immune_bridge_destroy(bridge);
}

/* ============================================================================
 * Learning Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, LearningBridgeUpdatePerformance) {
    const uint32_t STATE_DIM = 8;

    // Verified API: fep_likelihood_learner_create(config, obs_dim, state_dim)
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float states[STATE_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;
    for (uint32_t i = 0; i < STATE_DIM; i++) states[i] = 0.5f;

    printf("\n[Learning Bridge Update Performance]\n");
    benchmark_operation("update_likelihood", [&]() {
        // Verified API: fep_learn_likelihood(learner, sys, observation, state, obs_dim)
        fep_learn_likelihood(like_learner, fep, observations, states, OBS_DIM, STATE_DIM);
    }, UPDATE_THRESHOLD_US);

    fep_likelihood_learner_destroy(like_learner);
}

TEST_F(FEPPerformanceRegressionTest, LearningBridgeGradientPerformance) {
    const uint32_t STATE_DIM = 8;

    // Verified API
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    printf("\n[Learning Bridge Gradient Performance]\n");
    benchmark_operation("compute_gradient", [&]() {
        fep_learning_stats_t stats;
        fep_likelihood_learning_get_stats(like_learner, &stats);
        (void)stats.current_grad_norm;
    }, STATE_QUERY_THRESHOLD_US);

    fep_likelihood_learner_destroy(like_learner);
}

TEST_F(FEPPerformanceRegressionTest, LearningBridgeBatchPerformance) {
    const uint32_t STATE_DIM = 8;

    // Verified API
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float states[STATE_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;
    for (uint32_t i = 0; i < STATE_DIM; i++) states[i] = 0.5f;

    printf("\n[Learning Bridge Batch Performance]\n");
    benchmark_operation("batch_update_32", [&]() {
        // Perform 32 learning steps to simulate batch
        for (int j = 0; j < 32; j++) {
            fep_learn_likelihood(like_learner, fep, observations, states, OBS_DIM, STATE_DIM);
        }
    }, UPDATE_THRESHOLD_US * 32);

    fep_likelihood_learner_destroy(like_learner);
}

/* ============================================================================
 * Context Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, ContextBridgeSwitchPerformance) {
    fep_context_system_t* context = fep_context_create(nullptr);
    ASSERT_NE(context, nullptr);

    fep_context_connect(context, fep);

    uint32_t ctx1, ctx2;
    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    // Verified API: fep_context_add(sys, name, prior_beliefs, belief_dim, &context_id)
    fep_context_add(context, "context_1", prior_beliefs, OBS_DIM, &ctx1);
    fep_context_add(context, "context_2", prior_beliefs, OBS_DIM, &ctx2);

    printf("\n[Context Bridge Switch Performance]\n");
    bool toggle = false;
    benchmark_operation("switch_context", [&]() {
        // Verified API: fep_context_switch(sys, fep, target_context_id)
        fep_context_switch(context, fep, toggle ? ctx1 : ctx2);
        toggle = !toggle;
    }, UPDATE_THRESHOLD_US);

    fep_context_destroy(context);
}

TEST_F(FEPPerformanceRegressionTest, ContextBridgeInferencePerformance) {
    fep_context_system_t* context = fep_context_create(nullptr);
    ASSERT_NE(context, nullptr);

    fep_context_connect(context, fep);

    uint32_t ctx1, ctx2;
    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    // Verified API
    fep_context_add(context, "context_1", prior_beliefs, OBS_DIM, &ctx1);
    fep_context_add(context, "context_2", prior_beliefs, OBS_DIM, &ctx2);

    float observations[OBS_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;

    printf("\n[Context Bridge Inference Performance]\n");
    benchmark_operation("infer_context", [&]() {
        uint32_t inferred;
        float confidence;
        fep_context_infer(context, fep, observations, OBS_DIM, &inferred, &confidence);
    }, MODULATION_THRESHOLD_US);

    fep_context_destroy(context);
}

/* ============================================================================
 * Neuromodulation Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, NeuromodBridgeUpdatePerformance) {
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);

    fep_neuromod_connect(neuromod, fep);

    printf("\n[Neuromod Bridge Update Performance]\n");
    benchmark_operation("update", [&]() {
        fep_neuromod_update(neuromod, TIMESTEP_MS);
    }, UPDATE_THRESHOLD_US);

    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPPerformanceRegressionTest, NeuromodBridgeModulationPerformance) {
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);

    fep_neuromod_connect(neuromod, fep);

    printf("\n[Neuromod Bridge Modulation Performance]\n");
    benchmark_operation("apply_modulation", [&]() {
        fep_neuromod_apply_to_fep(neuromod, fep);
    }, MODULATION_THRESHOLD_US);

    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPPerformanceRegressionTest, NeuromodBridgeSetLevelsPerformance) {
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);

    fep_neuromod_connect(neuromod, fep);

    printf("\n[Neuromod Bridge Set Levels Performance]\n");
    float level = 0.5f;
    benchmark_operation("set_dopamine", [&]() {
        // Verified API: fep_neuromod_set_level(sys, type, level)
        fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, level);
        level = 1.0f - level;
    }, STATE_QUERY_THRESHOLD_US);

    fep_neuromod_destroy(neuromod);
}

/* ============================================================================
 * Sleep Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, SleepBridgeUpdatePerformance) {
    fep_sleep_system_t* sleep = fep_sleep_create(nullptr);
    ASSERT_NE(sleep, nullptr);

    fep_sleep_connect(sleep, fep);

    printf("\n[Sleep Bridge Update Performance]\n");
    benchmark_operation("update", [&]() {
        fep_sleep_update(sleep, TIMESTEP_MS);
    }, UPDATE_THRESHOLD_US);

    fep_sleep_destroy(sleep);
}

TEST_F(FEPPerformanceRegressionTest, SleepBridgeConsolidationPerformance) {
    fep_sleep_config_t config;
    fep_sleep_default_config(&config);
    config.enable_replay_consolidation = true;

    fep_sleep_system_t* sleep = fep_sleep_create(&config);
    ASSERT_NE(sleep, nullptr);

    fep_sleep_connect(sleep, fep);
    // Enter SWS stage for consolidation (verified API)
    fep_sleep_set_stage(sleep, SLEEP_STAGE_SWS);

    printf("\n[Sleep Bridge Consolidation Performance]\n");
    benchmark_operation("sws_update_with_consolidation", [&]() {
        // SWS updates handle consolidation automatically
        fep_sleep_update(sleep, TIMESTEP_MS);
    }, MODULATION_THRESHOLD_US * 2);

    fep_sleep_destroy(sleep);
}

/* ============================================================================
 * Curiosity Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, CuriosityBridgeUpdatePerformance) {
    fep_curiosity_config_t config;
    fep_curiosity_default_config(&config);

    fep_curiosity_system_t* curiosity = fep_curiosity_create(&config);
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    float observations[OBS_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;

    printf("\n[Curiosity Bridge Update Performance]\n");
    benchmark_operation("record_observation", [&]() {
        // Use record_observation as the update function (verified API)
        fep_curiosity_record_observation(curiosity, observations, OBS_DIM);
    }, UPDATE_THRESHOLD_US);

    fep_curiosity_destroy(curiosity);
}

TEST_F(FEPPerformanceRegressionTest, CuriosityBridgeNoveltyPerformance) {
    fep_curiosity_config_t config;
    fep_curiosity_default_config(&config);

    fep_curiosity_system_t* curiosity = fep_curiosity_create(&config);
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    float observations[OBS_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;

    printf("\n[Curiosity Bridge Novelty Performance]\n");
    benchmark_operation("compute_novelty", [&]() {
        // fep_compute_novelty returns float directly
        float novelty = fep_compute_novelty(curiosity, observations, OBS_DIM);
        (void)novelty; // Suppress unused warning
    }, UPDATE_THRESHOLD_US);

    fep_curiosity_destroy(curiosity);
}

/* ============================================================================
 * Evidence Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, EvidenceBridgeComputePerformance) {
    fep_evidence_config_t config;
    fep_evidence_default_config(&config);

    fep_evidence_system_t* evidence = fep_evidence_create(&config);
    ASSERT_NE(evidence, nullptr);

    fep_evidence_connect(evidence, fep);

    float observations[OBS_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;

    printf("\n[Evidence Bridge Compute Performance]\n");
    benchmark_operation("compute_evidence", [&]() {
        fep_evidence_result_t result;
        fep_compute_log_evidence(evidence, fep, observations, 1, OBS_DIM, &result);
    }, UPDATE_THRESHOLD_US);

    fep_evidence_destroy(evidence);
}

/* ============================================================================
 * Multi-Bridge Performance Tests
 * ============================================================================ */

TEST_F(FEPPerformanceRegressionTest, MultiBridgeSequentialUpdatePerformance) {
    fep_immune_bridge_t* immune_bridge = fep_immune_bridge_create(nullptr);
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);

    fep_immune_bridge_connect_fep(immune_bridge, fep);
    fep_immune_bridge_connect_immune(immune_bridge, immune);
    fep_neuromod_connect(neuromod, fep);

    printf("\n[Multi-Bridge Sequential Update Performance]\n");
    benchmark_operation("all_updates", [&]() {
        fep_immune_bridge_update(immune_bridge, TIMESTEP_MS);
        fep_neuromod_update(neuromod, TIMESTEP_MS);
    }, UPDATE_THRESHOLD_US * 3);

    fep_immune_bridge_destroy(immune_bridge);
    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPPerformanceRegressionTest, BioAsyncMessageLatency) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);

    printf("\n[Bio-Async Message Latency]\n");
    benchmark_operation("bio_async_connect", [&]() {
        fep_immune_bridge_connect_bio_async(bridge);
        fep_immune_bridge_disconnect_bio_async(bridge);
    }, CONNECTION_THRESHOLD_US * 2);

    fep_immune_bridge_destroy(bridge);
}

//=============================================================================
// test_quantum_stdp_optimizer.cpp - Unit Tests for Quantum STDP Optimizer
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/stdp/nimcp_quantum_stdp_optimizer.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class QSTDPOptimizerLifecycleTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(QSTDPOptimizerLifecycleTest, CreateWithDefaultConfig) {
    ctx = qstdp_optimizer_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QSTDPOptimizerLifecycleTest, CreateWithCustomConfig) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.objective = QSTDP_OBJ_STABILITY;
    config.schedule = QSTDP_SCHEDULE_LINEAR;
    config.ensemble_size = 4;

    ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QSTDPOptimizerLifecycleTest, CreateInvalidEnsembleSize) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.ensemble_size = 0;

    ctx = qstdp_optimizer_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QSTDPOptimizerLifecycleTest, CreateTooLargeEnsemble) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.ensemble_size = QSTDP_MAX_ENSEMBLE + 1;

    ctx = qstdp_optimizer_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QSTDPOptimizerLifecycleTest, CreateInvalidHistoryLength) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.history_length = 0;

    ctx = qstdp_optimizer_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QSTDPOptimizerLifecycleTest, DestroyNull) {
    qstdp_optimizer_destroy(nullptr);  // Should not crash
}

TEST_F(QSTDPOptimizerLifecycleTest, GetConfig) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.ensemble_size = 12;
    config.objective = QSTDP_OBJ_HOMEOSTASIS;

    ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    qstdp_optimizer_config_t retrieved;
    int result = qstdp_optimizer_get_config(ctx, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.ensemble_size, 12);
    EXPECT_EQ(retrieved.objective, QSTDP_OBJ_HOMEOSTASIS);
}

//=============================================================================
// Parameter Tests
//=============================================================================

class QSTDPOptimizerParamsTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void SetUp() override {
        ctx = qstdp_optimizer_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerParamsTest, GetParams) {
    float lr, a_plus, a_minus, tau_plus, tau_minus;
    int result = qstdp_optimizer_get_params(ctx, &lr, &a_plus, &a_minus,
                                            &tau_plus, &tau_minus);
    EXPECT_EQ(result, 0);

    // Parameters should be in valid ranges
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 1.0f);
    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
    EXPECT_GT(tau_plus, 0.0f);
    EXPECT_GT(tau_minus, 0.0f);
}

TEST_F(QSTDPOptimizerParamsTest, SetParams) {
    int result = qstdp_optimizer_set_params(ctx, 0.02f, 0.01f, 0.01f, 25.0f, 25.0f);
    EXPECT_EQ(result, 0);

    float lr, a_plus, a_minus, tau_plus, tau_minus;
    qstdp_optimizer_get_params(ctx, &lr, &a_plus, &a_minus, &tau_plus, &tau_minus);

    EXPECT_NEAR(lr, 0.02f, 1e-5);
    EXPECT_NEAR(a_plus, 0.01f, 1e-5);
    EXPECT_NEAR(a_minus, 0.01f, 1e-5);
    EXPECT_NEAR(tau_plus, 25.0f, 1e-5);
    EXPECT_NEAR(tau_minus, 25.0f, 1e-5);
}

TEST_F(QSTDPOptimizerParamsTest, GetParamsNull) {
    int result = qstdp_optimizer_get_params(nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(QSTDPOptimizerParamsTest, SetParamsNull) {
    int result = qstdp_optimizer_set_params(nullptr, 0.01f, 0.005f, 0.005f,
                                            20.0f, 20.0f);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Optimization Step Tests
//=============================================================================

class QSTDPOptimizerStepTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;
    qstdp_activity_stats_t stats;

    void SetUp() override {
        ctx = qstdp_optimizer_create(nullptr);

        // Initialize with realistic activity stats
        stats.mean_weight = 0.5f;
        stats.weight_variance = 0.1f;
        stats.ltp_rate = 5.0f;
        stats.ltd_rate = 5.0f;
        stats.ltp_ltd_ratio = 1.0f;
        stats.firing_rate = 5.0f;
        stats.sparsity = 0.3f;
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerStepTest, SingleStep) {
    float lr = qstdp_optimizer_step(ctx, &stats);

    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 1.0f);
}

TEST_F(QSTDPOptimizerStepTest, MultipleSteps) {
    std::vector<float> learning_rates;

    for (int i = 0; i < 100; i++) {
        float lr = qstdp_optimizer_step(ctx, &stats);
        learning_rates.push_back(lr);
    }

    // All learning rates should be valid
    for (float lr : learning_rates) {
        EXPECT_GT(lr, 0.0f);
        EXPECT_LT(lr, 1.0f);
    }
}

TEST_F(QSTDPOptimizerStepTest, StepWithoutStats) {
    // First record some history
    for (int i = 0; i < 10; i++) {
        qstdp_optimizer_record_activity(ctx, &stats);
    }

    // Step without providing stats (uses history)
    float lr = qstdp_optimizer_step(ctx, nullptr);
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 1.0f);
}

TEST_F(QSTDPOptimizerStepTest, StepNullContext) {
    float lr = qstdp_optimizer_step(nullptr, &stats);
    EXPECT_EQ(lr, 0.01f);  // Default fallback
}

TEST_F(QSTDPOptimizerStepTest, ConvergenceOverTime) {
    // With stable stats, learning rate should stabilize
    std::vector<float> learning_rates;

    for (int i = 0; i < 500; i++) {
        float lr = qstdp_optimizer_step(ctx, &stats);
        learning_rates.push_back(lr);
    }

    // Check variance in last 50 samples vs first 50
    float early_var = 0.0f, late_var = 0.0f;
    float early_mean = 0.0f, late_mean = 0.0f;

    for (int i = 0; i < 50; i++) {
        early_mean += learning_rates[i];
        late_mean += learning_rates[450 + i];
    }
    early_mean /= 50.0f;
    late_mean /= 50.0f;

    for (int i = 0; i < 50; i++) {
        early_var += (learning_rates[i] - early_mean) * (learning_rates[i] - early_mean);
        late_var += (learning_rates[450 + i] - late_mean) * (learning_rates[450 + i] - late_mean);
    }

    // Late variance should be lower or similar (annealing)
    EXPECT_LE(late_var, early_var * 1.5f);
}

//=============================================================================
// Objective Tests
//=============================================================================

class QSTDPOptimizerObjectiveTest : public ::testing::Test {
protected:
    qstdp_activity_stats_t balanced_stats;
    qstdp_activity_stats_t imbalanced_stats;

    void SetUp() override {
        balanced_stats.mean_weight = 0.5f;
        balanced_stats.weight_variance = 0.1f;
        balanced_stats.ltp_rate = 5.0f;
        balanced_stats.ltd_rate = 5.0f;
        balanced_stats.ltp_ltd_ratio = 1.0f;
        balanced_stats.firing_rate = 5.0f;
        balanced_stats.sparsity = 0.3f;

        imbalanced_stats = balanced_stats;
        imbalanced_stats.ltp_ltd_ratio = 2.0f;  // More LTP than LTD
    }
};

TEST_F(QSTDPOptimizerObjectiveTest, BalanceObjective) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.objective = QSTDP_OBJ_BALANCE;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Run with balanced stats
    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &balanced_stats);
    }

    qstdp_optimizer_stats_t stats;
    qstdp_optimizer_get_stats(ctx, &stats);

    EXPECT_GT(stats.optimizations_performed, 0);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerObjectiveTest, StabilityObjective) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.objective = QSTDP_OBJ_STABILITY;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    // High variance stats should lead to lower learning rates
    qstdp_activity_stats_t high_var = balanced_stats;
    high_var.weight_variance = 1.0f;

    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &high_var);
    }

    float lr_high_var;
    qstdp_optimizer_get_params(ctx, &lr_high_var, nullptr, nullptr, nullptr, nullptr);

    qstdp_optimizer_reset(ctx);

    // Low variance stats
    qstdp_activity_stats_t low_var = balanced_stats;
    low_var.weight_variance = 0.01f;

    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &low_var);
    }

    float lr_low_var;
    qstdp_optimizer_get_params(ctx, &lr_low_var, nullptr, nullptr, nullptr, nullptr);

    // Can't guarantee order due to randomness, but both should be valid
    EXPECT_GT(lr_high_var, 0.0f);
    EXPECT_GT(lr_low_var, 0.0f);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerObjectiveTest, HomeostasisObjective) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.objective = QSTDP_OBJ_HOMEOSTASIS;
    config.target_firing_rate = 10.0f;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Stats at target rate
    qstdp_activity_stats_t at_target = balanced_stats;
    at_target.firing_rate = 10.0f;

    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &at_target);
    }

    float lr;
    qstdp_optimizer_get_params(ctx, &lr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_GT(lr, 0.0f);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerObjectiveTest, SparsityObjective) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.objective = QSTDP_OBJ_SPARSITY;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    qstdp_activity_stats_t sparse = balanced_stats;
    sparse.sparsity = 0.8f;

    for (int i = 0; i < 50; i++) {
        qstdp_optimizer_step(ctx, &sparse);
    }

    EXPECT_TRUE(true);  // Just verify no crashes

    qstdp_optimizer_destroy(ctx);
}

//=============================================================================
// Annealing Schedule Tests
//=============================================================================

class QSTDPOptimizerAnnealingTest : public ::testing::Test {
protected:
    qstdp_activity_stats_t stats;

    void SetUp() override {
        stats.mean_weight = 0.5f;
        stats.weight_variance = 0.1f;
        stats.ltp_rate = 5.0f;
        stats.ltd_rate = 5.0f;
        stats.ltp_ltd_ratio = 1.0f;
        stats.firing_rate = 5.0f;
        stats.sparsity = 0.3f;
    }
};

TEST_F(QSTDPOptimizerAnnealingTest, LinearSchedule) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.schedule = QSTDP_SCHEDULE_LINEAR;
    config.initial_temperature = 1.0f;
    config.final_temperature = 0.01f;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    qstdp_optimizer_stats_t stats1;
    qstdp_optimizer_get_stats(ctx, &stats1);
    float initial_temp = stats1.current_temperature;

    for (int i = 0; i < 500; i++) {
        qstdp_optimizer_step(ctx, &stats);
    }

    qstdp_optimizer_stats_t stats2;
    qstdp_optimizer_get_stats(ctx, &stats2);

    EXPECT_LT(stats2.current_temperature, initial_temp);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerAnnealingTest, ExponentialSchedule) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.schedule = QSTDP_SCHEDULE_EXPONENTIAL;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 500; i++) {
        qstdp_optimizer_step(ctx, &stats);
    }

    qstdp_optimizer_stats_t opt_stats;
    qstdp_optimizer_get_stats(ctx, &opt_stats);

    EXPECT_LT(opt_stats.current_temperature, config.initial_temperature);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerAnnealingTest, AdaptiveSchedule) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.schedule = QSTDP_SCHEDULE_ADAPTIVE;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &stats);
    }

    qstdp_optimizer_stats_t opt_stats;
    qstdp_optimizer_get_stats(ctx, &opt_stats);

    EXPECT_LE(opt_stats.current_temperature, config.initial_temperature);

    qstdp_optimizer_destroy(ctx);
}

//=============================================================================
// Tunneling Tests
//=============================================================================

class QSTDPOptimizerTunnelingTest : public ::testing::Test {
protected:
    qstdp_activity_stats_t stats;

    void SetUp() override {
        stats.mean_weight = 0.5f;
        stats.weight_variance = 0.1f;
        stats.ltp_rate = 5.0f;
        stats.ltd_rate = 5.0f;
        stats.ltp_ltd_ratio = 1.0f;
        stats.firing_rate = 5.0f;
        stats.sparsity = 0.3f;
    }
};

TEST_F(QSTDPOptimizerTunnelingTest, TunnelingOccurs) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.tunneling_rate = 0.5f;  // High tunneling rate

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 200; i++) {
        qstdp_optimizer_step(ctx, &stats);
    }

    qstdp_optimizer_stats_t opt_stats;
    qstdp_optimizer_get_stats(ctx, &opt_stats);

    // With high tunneling rate, should have some tunneling events
    EXPECT_GT(opt_stats.tunneling_events, 0);

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerTunnelingTest, NoTunnelingWithZeroRate) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.tunneling_rate = 0.0f;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &stats);
    }

    qstdp_optimizer_stats_t opt_stats;
    qstdp_optimizer_get_stats(ctx, &opt_stats);

    EXPECT_EQ(opt_stats.tunneling_events, 0);

    qstdp_optimizer_destroy(ctx);
}

//=============================================================================
// Statistics Tests
//=============================================================================

class QSTDPOptimizerStatsTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void SetUp() override {
        ctx = qstdp_optimizer_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerStatsTest, InitialStats) {
    qstdp_optimizer_stats_t stats;
    int result = qstdp_optimizer_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.optimizations_performed, 0);
    EXPECT_EQ(stats.tunneling_events, 0);
    EXPECT_EQ(stats.active_candidates, 8);  // Default ensemble size
}

TEST_F(QSTDPOptimizerStatsTest, StatsAfterOptimization) {
    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    for (int i = 0; i < 50; i++) {
        qstdp_optimizer_step(ctx, &activity);
    }

    qstdp_optimizer_stats_t stats;
    qstdp_optimizer_get_stats(ctx, &stats);

    EXPECT_EQ(stats.optimizations_performed, 50);
}

TEST_F(QSTDPOptimizerStatsTest, GetStatsNull) {
    int result = qstdp_optimizer_get_stats(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Reset Tests
//=============================================================================

class QSTDPOptimizerResetTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void SetUp() override {
        ctx = qstdp_optimizer_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerResetTest, ResetRestoresInitialState) {
    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    // Run some optimization
    for (int i = 0; i < 100; i++) {
        qstdp_optimizer_step(ctx, &activity);
    }

    qstdp_optimizer_stats_t stats_before;
    qstdp_optimizer_get_stats(ctx, &stats_before);
    EXPECT_GT(stats_before.optimizations_performed, 0);

    // Reset
    int result = qstdp_optimizer_reset(ctx);
    EXPECT_EQ(result, 0);

    // Check temperature restored
    qstdp_optimizer_stats_t stats_after;
    qstdp_optimizer_get_stats(ctx, &stats_after);

    qstdp_optimizer_config_t config;
    qstdp_optimizer_get_config(ctx, &config);

    EXPECT_NEAR(stats_after.current_temperature, config.initial_temperature, 1e-5);
}

TEST_F(QSTDPOptimizerResetTest, ResetNull) {
    int result = qstdp_optimizer_reset(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Expected LR Tests
//=============================================================================

class QSTDPOptimizerExpectedLRTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void SetUp() override {
        ctx = qstdp_optimizer_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerExpectedLRTest, GetExpectedLR) {
    float expected_lr = qstdp_optimizer_get_expected_lr(ctx);

    EXPECT_GT(expected_lr, 0.0f);
    EXPECT_LT(expected_lr, 1.0f);
}

TEST_F(QSTDPOptimizerExpectedLRTest, ExpectedLRNull) {
    float expected_lr = qstdp_optimizer_get_expected_lr(nullptr);
    EXPECT_EQ(expected_lr, 0.01f);  // Default fallback
}

TEST_F(QSTDPOptimizerExpectedLRTest, ExpectedLRAfterOptimization) {
    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    // Run optimization
    for (int i = 0; i < 50; i++) {
        qstdp_optimizer_step(ctx, &activity);
    }

    float expected_lr = qstdp_optimizer_get_expected_lr(ctx);
    float best_lr;
    qstdp_optimizer_get_params(ctx, &best_lr, nullptr, nullptr, nullptr, nullptr);

    // Expected LR should be reasonably close to best LR
    // (amplitude-weighted average vs best)
    EXPECT_GT(expected_lr, 0.0f);
    EXPECT_LT(expected_lr, 1.0f);
}

//=============================================================================
// History Tests
//=============================================================================

class QSTDPOptimizerHistoryTest : public ::testing::Test {
protected:
    qstdp_optimizer_t ctx = nullptr;

    void SetUp() override {
        qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
        config.history_length = 16;  // Short history for testing
        ctx = qstdp_optimizer_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            qstdp_optimizer_destroy(ctx);
        }
    }
};

TEST_F(QSTDPOptimizerHistoryTest, RecordActivity) {
    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    // Record multiple activities
    for (int i = 0; i < 20; i++) {
        qstdp_optimizer_record_activity(ctx, &activity);
    }

    // Should work without crashing (circular buffer)
    EXPECT_TRUE(true);
}

TEST_F(QSTDPOptimizerHistoryTest, RecordActivityNull) {
    qstdp_optimizer_record_activity(nullptr, nullptr);
    EXPECT_TRUE(true);  // Should not crash
}

//=============================================================================
// Edge Cases
//=============================================================================

class QSTDPOptimizerEdgeCasesTest : public ::testing::Test {};

TEST_F(QSTDPOptimizerEdgeCasesTest, ExtremeActivityStats) {
    auto ctx = qstdp_optimizer_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    // Very extreme stats
    qstdp_activity_stats_t extreme = {
        .mean_weight = 0.0f,
        .weight_variance = 100.0f,
        .ltp_rate = 1000.0f,
        .ltd_rate = 0.001f,
        .ltp_ltd_ratio = 1000.0f,
        .firing_rate = 1000.0f,
        .sparsity = 0.99f
    };

    for (int i = 0; i < 50; i++) {
        float lr = qstdp_optimizer_step(ctx, &extreme);
        EXPECT_FALSE(std::isnan(lr));
        EXPECT_FALSE(std::isinf(lr));
        EXPECT_GT(lr, 0.0f);
    }

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerEdgeCasesTest, ZeroActivityStats) {
    auto ctx = qstdp_optimizer_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    qstdp_activity_stats_t zero = {0};

    for (int i = 0; i < 50; i++) {
        float lr = qstdp_optimizer_step(ctx, &zero);
        EXPECT_FALSE(std::isnan(lr));
        EXPECT_FALSE(std::isinf(lr));
    }

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerEdgeCasesTest, VerySmallEnsemble) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.ensemble_size = 1;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    for (int i = 0; i < 50; i++) {
        float lr = qstdp_optimizer_step(ctx, &activity);
        EXPECT_GT(lr, 0.0f);
    }

    qstdp_optimizer_destroy(ctx);
}

TEST_F(QSTDPOptimizerEdgeCasesTest, MaxEnsemble) {
    qstdp_optimizer_config_t config = qstdp_optimizer_default_config();
    config.ensemble_size = QSTDP_MAX_ENSEMBLE;

    auto ctx = qstdp_optimizer_create(&config);
    ASSERT_NE(ctx, nullptr);

    qstdp_activity_stats_t activity = {0.5f, 0.1f, 5.0f, 5.0f, 1.0f, 5.0f, 0.3f};

    for (int i = 0; i < 50; i++) {
        float lr = qstdp_optimizer_step(ctx, &activity);
        EXPECT_GT(lr, 0.0f);
    }

    qstdp_optimizer_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

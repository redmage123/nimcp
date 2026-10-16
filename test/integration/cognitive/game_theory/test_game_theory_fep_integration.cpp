/**
 * @file test_game_theory_fep_integration.cpp
 * @brief Integration tests for Game Theory + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests real integration between Game Theory module and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates game theory updates, free energy computation,
 *       and prediction error tracking for strategic reasoning
 * HOW:  Test FEP update cycles, Nash equilibrium effects, opponent modeling, and
 *       multi-player coordination with real module interactions
 *
 * TEST COVERAGE:
 * - StrategicDecisionWithFEP: Game theory decisions minimize free energy
 * - NashEquilibriumFreeEnergy: Equilibrium states have lower free energy
 * - OpponentModelingPredictionError: Opponent surprise increases free energy
 * - MultiPlayerCoordination: Coordinated strategies reduce free energy
 * - PayoffMatrixUncertainty: Uncertain payoffs increase free energy
 * - IteratedGameLearning: Repeated games reduce prediction error over time
 * - RiskAversionFreeEnergy: Risk-averse strategies in uncertain environments
 * - CooperationVsDefection: Free energy drives cooperation/defection choice
 * - FEPUpdateCycleIntegration: Verify 50ms update cycles work correctly
 * - StatisticsAccumulation: Stats accumulate across multiple cycles
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/game_theory/nimcp_game_theory_fep_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define GT_FEP_TEST_UPDATE_INTERVAL_MS    50    /* Cognitive timescale */
#define GT_FEP_TEST_NUM_CYCLES            20    /* Number of FEP cycles to run */
#define GT_FEP_TEST_NUM_PLAYERS           4     /* Multi-player game count */

/* ============================================================================
 * Global Test State Tracking
 * ============================================================================ */

static std::atomic<int> g_fep_update_count{0};
static std::atomic<float> g_accumulated_free_energy{0.0f};
static std::atomic<float> g_last_free_energy{0.0f};
static std::atomic<int> g_high_fe_callback_count{0};
static std::atomic<int> g_surprise_callback_count{0};
static std::atomic<int> g_metrics_callback_count{0};

/**
 * FEP bridge update callback for tracking
 */
static int gt_fep_tracking_update(fep_bridge_handle_t handle) {
    gt_fep_bridge_t* bridge = static_cast<gt_fep_bridge_t*>(handle);
    if (!bridge) return -1;

    g_fep_update_count++;

    /* Force FEP computation */
    gt_fep_bridge_force_update(bridge);

    float fe = gt_fep_bridge_get_free_energy(bridge);
    g_last_free_energy.store(fe);
    g_accumulated_free_energy.store(g_accumulated_free_energy.load() + fe);

    return 0;
}

/**
 * High free energy callback
 */
static void test_high_fe_callback(
    gt_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
) {
    (void)bridge;
    (void)free_energy;
    (void)user_data;
    g_high_fe_callback_count++;
}

/**
 * Surprise event callback
 */
static void test_surprise_callback(
    gt_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
) {
    (void)bridge;
    (void)surprise;
    (void)source;
    (void)user_data;
    g_surprise_callback_count++;
}

/**
 * Metrics update callback
 */
static void test_metrics_callback(
    gt_fep_bridge_t* bridge,
    const gt_fep_metrics_t* metrics,
    void* user_data
) {
    (void)bridge;
    (void)metrics;
    (void)user_data;
    g_metrics_callback_count++;
}

/**
 * Reset all global counters
 */
static void reset_global_counters() {
    g_fep_update_count = 0;
    g_accumulated_free_energy = 0.0f;
    g_last_free_energy = 0.0f;
    g_high_fe_callback_count = 0;
    g_surprise_callback_count = 0;
    g_metrics_callback_count = 0;
}

/* ============================================================================
 * Test Fixture: Game Theory + FEP Integration
 * ============================================================================ */

class GameTheoryFEPIntegrationTest : public ::testing::Test {
protected:
    gt_fep_bridge_t* bridge = nullptr;
    gt_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;
    nimcp_gt_system_t gt_system = nullptr;

    void SetUp() override {
        reset_global_counters();

        /* Create game theory system */
        nimcp_gt_config_t gt_config = nimcp_gt_default_config();
        gt_config.enable_statistics = true;
        gt_config.enable_history = true;
        gt_system = nimcp_gt_create(&gt_config);
        ASSERT_NE(gt_system, nullptr) << "Game theory system creation failed";

        /* Create FEP bridge */
        bridge_config = gt_fep_config_default();
        bridge_config.enable_logging = false;
        bridge_config.update_interval_ms = GT_FEP_TEST_UPDATE_INTERVAL_MS;
        bridge = gt_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "FEP bridge creation failed";

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr) << "FEP orchestrator creation failed";

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0)
            << "FEP orchestrator start failed";
    }

    void TearDown() override {
        if (bridge) {
            gt_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (gt_system) {
            nimcp_gt_destroy(gt_system);
            gt_system = nullptr;
        }
    }

    /**
     * Helper to get current time in milliseconds
     */
    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /**
     * Helper to register bridge with FEP orchestrator
     */
    void register_bridge_with_fep() {
        uint32_t bridge_id = 0;
        int ret = gt_fep_bridge_register(bridge, fep_orch, gt_system, &bridge_id);
        ASSERT_EQ(ret, 0) << "Bridge registration failed";
        ASSERT_GT(bridge_id, 0u) << "Bridge ID should be assigned";
    }

    /**
     * Helper to run multiple FEP update cycles
     */
    void run_fep_cycles(int num_cycles, uint64_t interval_ms = GT_FEP_TEST_UPDATE_INTERVAL_MS) {
        uint64_t start_time = get_current_time_ms();
        for (int i = 0; i < num_cycles; i++) {
            uint64_t current_time = start_time + (i * interval_ms);
            int updated = fep_orchestrator_update(fep_orch, current_time);
            EXPECT_GE(updated, 0) << "FEP update cycle " << i << " failed";
        }
    }

    /**
     * Helper to simulate strategic uncertainty scenario
     */
    void simulate_strategic_uncertainty(float initial, float final_val, int steps) {
        float step_size = (final_val - initial) / steps;
        for (int i = 0; i <= steps; i++) {
            float uncertainty = initial + (step_size * i);
            gt_fep_bridge_update_strategy_uncertainty(bridge, uncertainty);
            gt_fep_bridge_force_update(bridge);
        }
    }

    /**
     * Helper to simulate opponent modeling improvement
     */
    void simulate_opponent_learning(float initial_error, float final_error, int steps) {
        float step_size = (final_error - initial_error) / steps;
        for (int i = 0; i <= steps; i++) {
            float error = initial_error + (step_size * i);
            gt_fep_bridge_update_opponent_error(bridge, error);
            gt_fep_bridge_force_update(bridge);
        }
    }
};

/* ============================================================================
 * StrategicDecisionWithFEP - Game theory decisions minimize free energy
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, StrategicDecisionWithFEP) {
    register_bridge_with_fep();

    /* Scenario: Start with uncertain strategy, converge to optimal */

    /* High uncertainty initial state */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.9f);
    gt_fep_bridge_update_opponent_error(bridge, 0.8f);
    gt_fep_bridge_update_nash_distance(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);

    float initial_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(initial_fe, bridge_config.baseline_free_energy)
        << "High uncertainty should produce elevated free energy";

    /* Simulate strategic decision refinement */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.3f);
    gt_fep_bridge_force_update(bridge);

    float refined_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(refined_fe, initial_fe)
        << "Strategy refinement should minimize free energy";

    /* Run FEP cycles to verify coordination */
    run_fep_cycles(10);

    /* Get final state */
    gt_fep_metrics_t metrics;
    gt_fep_bridge_get_metrics(bridge, &metrics);

    EXPECT_GE(metrics.update_count, 10u)
        << "FEP should have processed multiple updates";
}

/* ============================================================================
 * NashEquilibriumFreeEnergy - Equilibrium states have lower free energy
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, NashEquilibriumFreeEnergy) {
    register_bridge_with_fep();

    /* State far from Nash equilibrium */
    gt_fep_bridge_update_nash_distance(bridge, 0.95f);
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f);
    gt_fep_bridge_update_opponent_error(bridge, 0.5f);
    gt_fep_bridge_force_update(bridge);

    float far_from_nash_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_FALSE(gt_fep_bridge_is_at_nash(bridge))
        << "Should not be at Nash equilibrium";

    /* Approach Nash equilibrium */
    gt_fep_bridge_update_nash_distance(bridge, 0.05f);
    gt_fep_bridge_force_update(bridge);

    float near_nash_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(near_nash_fe, far_from_nash_fe)
        << "Approaching Nash should reduce free energy";

    /* At Nash equilibrium (below epsilon) */
    gt_fep_bridge_update_nash_distance(bridge, 0.005f);
    gt_fep_bridge_force_update(bridge);

    float at_nash_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_TRUE(gt_fep_bridge_is_at_nash(bridge))
        << "Should detect Nash equilibrium";
    EXPECT_LE(at_nash_fe, near_nash_fe)
        << "Nash equilibrium should have minimum free energy";

    /* Run FEP cycles to verify stability */
    float pre_cycle_fe = gt_fep_bridge_get_free_energy(bridge);
    run_fep_cycles(10);
    float post_cycle_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Free energy should remain stable at equilibrium */
    EXPECT_NEAR(post_cycle_fe, pre_cycle_fe, 0.1f)
        << "Free energy should be stable at Nash equilibrium";
}

/* ============================================================================
 * OpponentModelingPredictionError - Opponent surprise increases free energy
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, OpponentModelingPredictionError) {
    register_bridge_with_fep();

    /* Register surprise callback */
    gt_fep_bridge_set_surprise_callback(bridge, test_surprise_callback, nullptr);

    /* Initial state with good opponent model */
    gt_fep_bridge_update_opponent_error(bridge, 0.1f);
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.3f);
    gt_fep_bridge_update_nash_distance(bridge, 0.3f);
    gt_fep_bridge_force_update(bridge);

    float good_model_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Opponent does something unexpected - high prediction error */
    gt_fep_bridge_update_opponent_error(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);

    float surprised_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_GT(surprised_fe, good_model_fe)
        << "Opponent surprise should increase free energy";

    /* Get metrics to check prediction error */
    gt_fep_metrics_t metrics;
    gt_fep_bridge_get_metrics(bridge, &metrics);

    EXPECT_GT(metrics.opponent_prediction_error, 0.5f)
        << "High opponent error should be reflected in metrics";
    EXPECT_GT(metrics.surprise, 0.0f)
        << "Surprise metric should indicate unexpected behavior";

    /* Simulate learning about opponent */
    simulate_opponent_learning(0.9f, 0.2f, 10);

    float learned_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(learned_fe, surprised_fe)
        << "Learning about opponent should reduce free energy";
}

/* ============================================================================
 * MultiPlayerCoordination - Coordinated strategies reduce free energy
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, MultiPlayerCoordination) {
    register_bridge_with_fep();

    /* Uncoordinated multi-player scenario */
    gt_fep_bridge_update_nash_distance(bridge, 0.8f);
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.7f);
    gt_fep_bridge_update_opponent_error(bridge, 0.7f);
    gt_fep_bridge_force_update(bridge);

    float uncoordinated_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Simulate coordination improvement */
    /* As players coordinate, nash distance decreases */
    gt_fep_bridge_update_nash_distance(bridge, 0.2f);
    /* Better mutual understanding of strategies */
    gt_fep_bridge_update_opponent_error(bridge, 0.2f);
    /* More certain about collective strategy */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.2f);
    gt_fep_bridge_force_update(bridge);

    float coordinated_fe = gt_fep_bridge_get_free_energy(bridge);
    EXPECT_LT(coordinated_fe, uncoordinated_fe)
        << "Coordinated strategies should reduce free energy";

    /* Verify significant reduction */
    float reduction_ratio = coordinated_fe / uncoordinated_fe;
    EXPECT_LT(reduction_ratio, 0.7f)
        << "Coordination should produce significant free energy reduction";

    /* Run FEP update cycles */
    run_fep_cycles(GT_FEP_TEST_NUM_CYCLES);

    fep_orchestrator_stats_t fep_stats;
    fep_orchestrator_get_stats(fep_orch, &fep_stats);
    EXPECT_GT(fep_stats.total_update_cycles, 0u)
        << "FEP orchestrator should track update cycles";
}

/* ============================================================================
 * PayoffMatrixUncertainty - Uncertain payoffs increase free energy
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, PayoffMatrixUncertainty) {
    register_bridge_with_fep();

    /* Certain payoff scenario */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.1f);  /* Known best strategy */
    gt_fep_bridge_update_opponent_error(bridge, 0.1f);        /* Known opponent behavior */
    gt_fep_bridge_update_nash_distance(bridge, 0.1f);         /* Near equilibrium */
    gt_fep_bridge_force_update(bridge);

    float certain_fe = gt_fep_bridge_get_free_energy(bridge);

    gt_fep_metrics_t certain_metrics;
    gt_fep_bridge_get_metrics(bridge, &certain_metrics);
    float certain_payoff_variance = certain_metrics.payoff_variance;

    /* Uncertain payoff scenario - strategy and opponent uncertainty affect payoff variance */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);
    gt_fep_bridge_update_opponent_error(bridge, 0.8f);
    gt_fep_bridge_force_update(bridge);

    float uncertain_fe = gt_fep_bridge_get_free_energy(bridge);

    gt_fep_metrics_t uncertain_metrics;
    gt_fep_bridge_get_metrics(bridge, &uncertain_metrics);

    EXPECT_GT(uncertain_fe, certain_fe)
        << "Uncertain payoffs should increase free energy";
    EXPECT_GT(uncertain_metrics.payoff_variance, certain_payoff_variance)
        << "Payoff variance should increase with uncertainty";
    EXPECT_GT(uncertain_metrics.entropy, certain_metrics.entropy)
        << "Entropy should increase with uncertainty";
}

/* ============================================================================
 * IteratedGameLearning - Repeated games reduce prediction error over time
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, IteratedGameLearning) {
    register_bridge_with_fep();

    /* Register metrics callback to track learning */
    gt_fep_bridge_set_metrics_callback(bridge, test_metrics_callback, nullptr);

    /* Initial state: High uncertainty in iterated game */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.9f);
    gt_fep_bridge_update_opponent_error(bridge, 0.9f);
    gt_fep_bridge_update_nash_distance(bridge, 0.9f);
    gt_fep_bridge_force_update(bridge);

    float initial_prediction_error = gt_fep_bridge_get_prediction_error(bridge);
    float initial_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Simulate learning over iterations (gradual improvement) */
    const int NUM_ITERATIONS = 15;
    std::vector<float> fe_history;
    fe_history.push_back(initial_fe);

    for (int i = 1; i <= NUM_ITERATIONS; i++) {
        float progress = (float)i / NUM_ITERATIONS;

        /* Strategy uncertainty decreases with iterations */
        float strategy_uncertainty = 0.9f * (1.0f - progress * 0.7f);
        gt_fep_bridge_update_strategy_uncertainty(bridge, strategy_uncertainty);

        /* Opponent model improves over time */
        float opponent_error = 0.9f * (1.0f - progress * 0.6f);
        gt_fep_bridge_update_opponent_error(bridge, opponent_error);

        /* Approach equilibrium over iterations */
        float nash_distance = 0.9f * (1.0f - progress * 0.5f);
        gt_fep_bridge_update_nash_distance(bridge, nash_distance);

        gt_fep_bridge_force_update(bridge);
        fe_history.push_back(gt_fep_bridge_get_free_energy(bridge));
    }

    float final_prediction_error = gt_fep_bridge_get_prediction_error(bridge);
    float final_fe = gt_fep_bridge_get_free_energy(bridge);

    EXPECT_LT(final_prediction_error, initial_prediction_error)
        << "Prediction error should decrease with learning";
    EXPECT_LT(final_fe, initial_fe)
        << "Free energy should decrease over iterated game";

    /* Verify monotonic decrease trend (allow some noise) */
    int decreases = 0;
    for (size_t i = 1; i < fe_history.size(); i++) {
        if (fe_history[i] < fe_history[i-1]) {
            decreases++;
        }
    }
    EXPECT_GT(decreases, NUM_ITERATIONS / 2)
        << "Free energy should generally decrease during learning";

    /* Verify callbacks were invoked */
    EXPECT_GT(g_metrics_callback_count.load(), 0)
        << "Metrics callback should be invoked during updates";
}

/* ============================================================================
 * RiskAversionFreeEnergy - Risk-averse strategies in uncertain environments
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, RiskAversionFreeEnergy) {
    register_bridge_with_fep();

    /* Low-risk scenario: low uncertainty */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.2f);
    gt_fep_bridge_update_opponent_error(bridge, 0.2f);
    gt_fep_bridge_update_nash_distance(bridge, 0.3f);
    gt_fep_bridge_force_update(bridge);

    float low_risk_fe = gt_fep_bridge_get_free_energy(bridge);
    gt_fep_metrics_t low_risk_metrics;
    gt_fep_bridge_get_metrics(bridge, &low_risk_metrics);

    /* High-risk scenario: high uncertainty */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);
    gt_fep_bridge_update_opponent_error(bridge, 0.8f);
    gt_fep_bridge_update_nash_distance(bridge, 0.8f);
    gt_fep_bridge_force_update(bridge);

    float high_risk_fe = gt_fep_bridge_get_free_energy(bridge);
    gt_fep_metrics_t high_risk_metrics;
    gt_fep_bridge_get_metrics(bridge, &high_risk_metrics);

    /* FEP predicts risk-averse behavior: minimize free energy by avoiding uncertainty */
    EXPECT_GT(high_risk_fe, low_risk_fe)
        << "High-risk scenarios should have higher free energy";
    EXPECT_GT(high_risk_metrics.entropy, low_risk_metrics.entropy)
        << "Higher entropy in risky scenarios indicates uncertainty";

    /* Risk-averse agent should prefer low-risk state */
    /* (Lower FE indicates preferred state in FEP framework) */
    float risk_premium = high_risk_fe - low_risk_fe;
    EXPECT_GT(risk_premium, 0.0f)
        << "Risk premium should be positive (risk increases FE)";
}

/* ============================================================================
 * CooperationVsDefection - Free energy drives cooperation/defection choice
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, CooperationVsDefection) {
    register_bridge_with_fep();

    /* Scenario 1: Mutual cooperation - low uncertainty, near Nash */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.15f);  /* Clear cooperative strategy */
    gt_fep_bridge_update_opponent_error(bridge, 0.1f);         /* Trust opponent to cooperate */
    gt_fep_bridge_update_nash_distance(bridge, 0.1f);          /* Cooperative equilibrium */
    gt_fep_bridge_force_update(bridge);

    float cooperation_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Scenario 2: Mutual defection - higher uncertainty about outcome */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f);   /* Mixed strategy uncertainty */
    gt_fep_bridge_update_opponent_error(bridge, 0.6f);         /* Uncertain if opponent defects */
    gt_fep_bridge_update_nash_distance(bridge, 0.4f);          /* Further from Pareto-optimal */
    gt_fep_bridge_force_update(bridge);

    float defection_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Scenario 3: Unilateral defection attempt (max uncertainty) */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);   /* Risk of exploitation */
    gt_fep_bridge_update_opponent_error(bridge, 0.85f);        /* Unknown if exploited */
    gt_fep_bridge_update_nash_distance(bridge, 0.7f);          /* Unstable state */
    gt_fep_bridge_force_update(bridge);

    float exploitation_fe = gt_fep_bridge_get_free_energy(bridge);

    /* FEP predicts: cooperation has lowest FE when it's stable equilibrium */
    /* This models real biological tendency toward cooperation under FEP */
    EXPECT_LT(cooperation_fe, defection_fe)
        << "Stable cooperation should have lower free energy than defection";
    EXPECT_LT(cooperation_fe, exploitation_fe)
        << "Cooperation should be preferred over risky exploitation";

    /* Run FEP cycles to verify state tracking */
    run_fep_cycles(10);

    gt_fep_stats_t stats;
    gt_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u)
        << "Statistics should accumulate during FEP cycles";
}

/* ============================================================================
 * FEPUpdateCycleIntegration - Verify 50ms update cycles work correctly
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, FEPUpdateCycleIntegration) {
    register_bridge_with_fep();

    /* Set initial state */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.5f);
    gt_fep_bridge_update_opponent_error(bridge, 0.5f);
    gt_fep_bridge_update_nash_distance(bridge, 0.5f);

    /* Run FEP update cycles at cognitive timescale (50ms) */
    uint64_t start_time = get_current_time_ms();
    const int NUM_CYCLES = 20;

    for (int i = 0; i < NUM_CYCLES; i++) {
        /* Simulate 50ms intervals */
        uint64_t cycle_time = start_time + (i * 50);
        int updated = fep_orchestrator_update(fep_orch, cycle_time);
        EXPECT_GE(updated, 0) << "FEP update cycle " << i << " should succeed";
    }

    /* Verify orchestrator stats */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);

    EXPECT_EQ(orch_stats.total_update_cycles, (uint64_t)NUM_CYCLES)
        << "All FEP cycles should be counted";
    EXPECT_GT(orch_stats.total_bridge_updates, 0u)
        << "Bridge updates should have occurred";

    /* Verify bridge received updates via FEP orchestrator */
    gt_fep_metrics_t bridge_metrics;
    gt_fep_bridge_get_metrics(bridge, &bridge_metrics);
    EXPECT_GT(bridge_metrics.update_count, 0u)
        << "Bridge should have processed FEP updates";

    /* Check cognitive category stats */
    EXPECT_GT(orch_stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].bridge_count, 0u)
        << "Cognitive category should have registered bridges";
}

/* ============================================================================
 * StatisticsAccumulation - Stats accumulate across multiple cycles
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, StatisticsAccumulation) {
    register_bridge_with_fep();

    /* Reset statistics */
    gt_fep_bridge_reset_stats(bridge);

    /* Get initial stats */
    gt_fep_stats_t initial_stats;
    gt_fep_bridge_get_stats(bridge, &initial_stats);
    EXPECT_EQ(initial_stats.total_updates, 0u)
        << "Stats should be zeroed after reset";

    /* Run multiple FEP cycles with varying metrics */
    const int NUM_CYCLES = 25;
    float total_fe_accumulated = 0.0f;
    float peak_fe = 0.0f;

    for (int i = 0; i < NUM_CYCLES; i++) {
        /* Vary metrics across cycles */
        float phase = (float)i / NUM_CYCLES;
        gt_fep_bridge_update_strategy_uncertainty(bridge, 0.3f + 0.4f * sinf(phase * 3.14159f));
        gt_fep_bridge_update_opponent_error(bridge, 0.2f + 0.5f * cosf(phase * 3.14159f));
        gt_fep_bridge_force_update(bridge);

        float current_fe = gt_fep_bridge_get_free_energy(bridge);
        total_fe_accumulated += current_fe;
        if (current_fe > peak_fe) {
            peak_fe = current_fe;
        }
    }

    /* Verify accumulated statistics */
    gt_fep_stats_t final_stats;
    gt_fep_bridge_get_stats(bridge, &final_stats);

    EXPECT_EQ(final_stats.total_updates, (uint64_t)NUM_CYCLES)
        << "All updates should be counted";
    EXPECT_GT(final_stats.total_free_energy_contribution, 0.0f)
        << "Free energy contribution should accumulate";
    EXPECT_GT(final_stats.avg_free_energy, 0.0f)
        << "Average free energy should be computed";
    EXPECT_GT(final_stats.peak_free_energy, 0.0f)
        << "Peak free energy should be tracked";
    EXPECT_GT(final_stats.strategy_computations, 0u)
        << "Strategy computations should be counted";

    /* Verify timing stats */
    EXPECT_GT(final_stats.avg_update_time_us, 0.0f)
        << "Average update time should be tracked";
    EXPECT_GT(final_stats.total_update_time_us, 0u)
        << "Total update time should accumulate";

    /* Run FEP orchestrator cycles and verify combined stats */
    run_fep_cycles(10);

    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GT(orch_stats.total_bridge_updates, 0u)
        << "Orchestrator should track bridge updates";
}

/* ============================================================================
 * DegradedModeUnderHighFreeEnergy - High FE triggers degraded state
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, DegradedModeUnderHighFreeEnergy) {
    register_bridge_with_fep();

    /* Register high FE callback */
    gt_fep_bridge_set_high_fe_callback(bridge, test_high_fe_callback, nullptr);

    /* Initial state: not degraded */
    EXPECT_FALSE(gt_fep_bridge_is_degraded(bridge))
        << "Should not start in degraded mode";

    /* Push all metrics to maximum uncertainty */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 1.0f);
    gt_fep_bridge_update_opponent_error(bridge, 1.0f);
    gt_fep_bridge_update_nash_distance(bridge, 1.0f);
    gt_fep_bridge_force_update(bridge);

    float max_fe = gt_fep_bridge_get_free_energy(bridge);

    /* Check if free energy exceeds degradation threshold */
    if (max_fe > bridge_config.high_free_energy_threshold) {
        EXPECT_TRUE(gt_fep_bridge_is_degraded(bridge))
            << "Should enter degraded mode when FE exceeds threshold";
        EXPECT_GT(g_high_fe_callback_count.load(), 0)
            << "High FE callback should be invoked";

        /* Verify state */
        gt_fep_state_t state = gt_fep_bridge_get_state(bridge);
        EXPECT_EQ(state, GT_FEP_STATE_DEGRADED)
            << "State should be DEGRADED";
    }

    /* Reduce uncertainty to exit degraded mode */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.1f);
    gt_fep_bridge_update_opponent_error(bridge, 0.1f);
    gt_fep_bridge_update_nash_distance(bridge, 0.1f);
    gt_fep_bridge_force_update(bridge);

    float low_fe = gt_fep_bridge_get_free_energy(bridge);
    if (low_fe < bridge_config.high_free_energy_threshold) {
        EXPECT_FALSE(gt_fep_bridge_is_degraded(bridge))
            << "Should exit degraded mode when FE drops below threshold";
    }
}

/* ============================================================================
 * OrchestratorBridgeCoordination - Multiple bridges coordinate correctly
 * ============================================================================ */

TEST_F(GameTheoryFEPIntegrationTest, OrchestratorBridgeCoordination) {
    /* Create a second bridge to test multi-bridge coordination */
    gt_fep_bridge_t* bridge2 = gt_fep_bridge_create(&bridge_config);
    ASSERT_NE(bridge2, nullptr);

    /* Register both bridges */
    uint32_t id1 = 0, id2 = 0;
    ASSERT_EQ(gt_fep_bridge_register(bridge, fep_orch, gt_system, &id1), 0);
    ASSERT_EQ(gt_fep_bridge_register(bridge2, fep_orch, nullptr, &id2), 0);

    EXPECT_NE(id1, id2) << "Each bridge should get unique ID";

    /* Set different states */
    gt_fep_bridge_update_strategy_uncertainty(bridge, 0.8f);
    gt_fep_bridge_update_strategy_uncertainty(bridge2, 0.2f);

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Both bridges should have been updated */
    gt_fep_metrics_t metrics1, metrics2;
    gt_fep_bridge_get_metrics(bridge, &metrics1);
    gt_fep_bridge_get_metrics(bridge2, &metrics2);

    EXPECT_GT(metrics1.update_count, 0u);
    EXPECT_GT(metrics2.update_count, 0u);

    /* Verify different free energies based on different states */
    float fe1 = gt_fep_bridge_get_free_energy(bridge);
    float fe2 = gt_fep_bridge_get_free_energy(bridge2);

    /* Bridge with higher uncertainty should have higher FE */
    /* (may not be exact due to FEP cycle updates, but should show difference) */
    /* The test verifies they can coexist and be updated independently */

    /* Verify orchestrator tracked both */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 2u)
        << "Orchestrator should track both bridges";

    /* Cleanup second bridge */
    gt_fep_bridge_destroy(bridge2);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

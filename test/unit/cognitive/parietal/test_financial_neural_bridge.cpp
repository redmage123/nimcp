/**
 * @file test_financial_neural_bridge.cpp
 * @brief Unit tests for NIMCP Financial Neural Integration Bridge
 *
 * Tests spike encoding, STDP reward learning, LNN prediction,
 * plasticity adaptation, quantum optimization, memory consolidation,
 * training, modulation, and NULL safety.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/parietal/nimcp_financial_neural_bridge.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialNeuralBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fin_neural_config_t cfg = financial_neural_bridge_default_config();
        bridge = financial_neural_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override
    {
        if (bridge) {
            financial_neural_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /** Helper: create a market event */
    fin_market_event_t make_event(fin_market_event_type_t type,
                                   float magnitude, float direction)
    {
        fin_market_event_t e{};
        e.type = type;
        e.magnitude = magnitude;
        e.direction = direction;
        e.timestamp_us = 1000000;
        e.context_count = 0;
        return e;
    }

    /** Helper: create a simple time series */
    void make_time_series(fin_time_series_t* ts, uint32_t len)
    {
        memset(ts, 0, sizeof(*ts));
        ts->length = len;
        for (uint32_t i = 0; i < len; i++) {
            ts->prices[i] = 100.0f + 0.1f * (float)i +
                            sinf((float)i * 0.3f);
            ts->volumes[i] = 1000.0f;
            ts->timestamps[i] = 1000000ULL * i;
        }
        ts->open = ts->prices[0];
        ts->close = ts->prices[len - 1];
        ts->high = ts->close + 5.0f;
        ts->low = ts->open - 3.0f;
    }

    financial_neural_bridge_t* bridge = nullptr;
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, DefaultConfig)
{
    fin_neural_config_t cfg = financial_neural_bridge_default_config();
    EXPECT_GT(cfg.spike_channels, 0u);
    EXPECT_GT(cfg.lnn_state_dim, 0u);
    EXPECT_GT(cfg.stdp_learning_rate, 0.0f);
}

TEST_F(FinancialNeuralBridgeTest, CreateWithConfig)
{
    EXPECT_NE(bridge, nullptr);
}

TEST_F(FinancialNeuralBridgeTest, DestroyNullSafe)
{
    financial_neural_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(FinancialNeuralBridgeTest, GetState)
{
    fin_neural_state_t state = financial_neural_bridge_get_state(bridge);
    EXPECT_GE((int)state, 0);
    EXPECT_LE((int)state, (int)FIN_NEURAL_STATE_ERROR);
}

TEST_F(FinancialNeuralBridgeTest, Reset)
{
    int rc = financial_neural_bridge_reset(bridge);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

//=============================================================================
// 2. Subsystem Setter Tests (all 8)
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, SetSNN)
{
    int rc = financial_neural_bridge_set_snn(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetSTDP)
{
    int rc = financial_neural_bridge_set_stdp(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetLNN)
{
    int rc = financial_neural_bridge_set_lnn(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetPlasticity)
{
    int rc = financial_neural_bridge_set_plasticity(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetQuantum)
{
    int rc = financial_neural_bridge_set_quantum(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetImmune)
{
    int rc = financial_neural_bridge_set_immune(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetHealthAgent)
{
    int rc = financial_neural_bridge_set_health_agent(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetFuzzyBridge)
{
    int rc = financial_neural_bridge_set_fuzzy_bridge(bridge, nullptr);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

//=============================================================================
// 3. Spike Encoding Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, EncodeMarketEventPriceChange)
{
    fin_market_event_t event = make_event(FIN_EVENT_PRICE_CHANGE, 0.05f, 1.0f);
    fin_spike_train_t spikes{};

    int rc = financial_neural_bridge_encode_market_event(bridge, &event, &spikes);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(spikes.active_channels, 0u);
    EXPECT_GT(spikes.total_activity, 0.0f);
}

TEST_F(FinancialNeuralBridgeTest, EncodeMarketEventVolumeSpike)
{
    fin_market_event_t event = make_event(FIN_EVENT_VOLUME_SPIKE, 3.0f, 1.0f);
    fin_spike_train_t spikes{};

    int rc = financial_neural_bridge_encode_market_event(bridge, &event, &spikes);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(spikes.active_channels, 0u);
}

TEST_F(FinancialNeuralBridgeTest, EncodeMarketEventVolatilityShift)
{
    fin_market_event_t event = make_event(FIN_EVENT_VOLATILITY_SHIFT, 0.5f, -1.0f);
    fin_spike_train_t spikes{};

    int rc = financial_neural_bridge_encode_market_event(bridge, &event, &spikes);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, EncodeFuzzyRegime)
{
    fin_fuzzy_market_condition_t condition{};
    condition.bull_degree = 0.7f;
    condition.bear_degree = 0.1f;
    condition.sideways_degree = 0.2f;
    condition.dominant = FIN_MKT_BULL;

    fin_spike_train_t spikes{};
    int rc = financial_neural_bridge_encode_fuzzy_regime(bridge, &condition, &spikes);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(spikes.active_channels, 0u);
}

TEST_F(FinancialNeuralBridgeTest, DecodeSpikes)
{
    // Encode then decode
    fin_market_event_t event = make_event(FIN_EVENT_PRICE_CHANGE, 0.05f, 1.0f);
    fin_spike_train_t spikes{};
    financial_neural_bridge_encode_market_event(bridge, &event, &spikes);

    float signal = 0.0f, confidence = 0.0f;
    int rc = financial_neural_bridge_decode_spikes(bridge, &spikes,
        &signal, &confidence);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

//=============================================================================
// 4. STDP Reward Learning Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, STDPRewardPositive)
{
    fin_stdp_reward_t reward{};
    // Use 15% return (0.15f) which is above the s_shaped threshold of 0.05-0.20
    int rc = financial_neural_bridge_stdp_reward(bridge, 0.15f, 86400000000ULL,
        &reward);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(reward.reward_magnitude, 0.0f);
}

TEST_F(FinancialNeuralBridgeTest, STDPRewardNegative)
{
    fin_stdp_reward_t reward{};
    // Use -15% return which is above the s_shaped threshold for loss detection
    int rc = financial_neural_bridge_stdp_reward(bridge, -0.15f, 86400000000ULL,
        &reward);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_LT(reward.reward_magnitude, 0.0f);
}

TEST_F(FinancialNeuralBridgeTest, FuzzyRewardProfitable)
{
    fin_stdp_reward_t reward{};
    int rc = financial_neural_bridge_compute_fuzzy_reward(bridge, 0.10f, &reward);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(reward.fuzzy_profitable_degree, reward.fuzzy_loss_degree);
    // All degrees in [0,1]
    EXPECT_GE(reward.fuzzy_profitable_degree, 0.0f);
    EXPECT_LE(reward.fuzzy_profitable_degree, 1.0f);
    EXPECT_GE(reward.fuzzy_neutral_degree, 0.0f);
    EXPECT_LE(reward.fuzzy_neutral_degree, 1.0f);
    EXPECT_GE(reward.fuzzy_loss_degree, 0.0f);
    EXPECT_LE(reward.fuzzy_loss_degree, 1.0f);
}

TEST_F(FinancialNeuralBridgeTest, FuzzyRewardLoss)
{
    fin_stdp_reward_t reward{};
    financial_neural_bridge_compute_fuzzy_reward(bridge, -0.10f, &reward);
    EXPECT_GT(reward.fuzzy_loss_degree, reward.fuzzy_profitable_degree);
}

TEST_F(FinancialNeuralBridgeTest, FuzzyRewardNeutral)
{
    fin_stdp_reward_t reward{};
    financial_neural_bridge_compute_fuzzy_reward(bridge, 0.001f, &reward);
    // Near zero return should have higher neutral degree
    EXPECT_GT(reward.fuzzy_neutral_degree, 0.0f);
}

//=============================================================================
// 5. LNN Prediction Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, LNNPredict)
{
    fin_time_series_t ts{};
    make_time_series(&ts, 100);

    fin_neural_prediction_t prediction{};
    int rc = financial_neural_bridge_lnn_predict(bridge, &ts, 5, &prediction);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_EQ(prediction.horizon_steps, 5u);
    EXPECT_GE(prediction.confidence, 0.0f);
    EXPECT_LE(prediction.confidence, 1.0f);
}

TEST_F(FinancialNeuralBridgeTest, LNNPredictFuzzyRegime)
{
    fin_time_series_t ts{};
    make_time_series(&ts, 100);

    fin_neural_prediction_t prediction{};
    financial_neural_bridge_lnn_predict(bridge, &ts, 5, &prediction);
    // Fuzzy regime output should be valid
    EXPECT_GE(prediction.fuzzy_regime.bull_degree, 0.0f);
    EXPECT_LE(prediction.fuzzy_regime.bull_degree, 1.0f);
}

TEST_F(FinancialNeuralBridgeTest, LNNUpdate)
{
    float obs[] = {0.01f, -0.02f, 0.03f, 0.005f};
    int rc = financial_neural_bridge_lnn_update(bridge, obs, 4);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

//=============================================================================
// 6. Plasticity Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, AdaptRiskParams)
{
    fin_plasticity_params_t params{};
    int rc = financial_neural_bridge_adapt_risk_params(bridge,
        1.5f, -0.1f, &params);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(params.current_plasticity_rate, 0.0f);
    EXPECT_GT(params.adapted_risk_tolerance, 0.0f);
}

TEST_F(FinancialNeuralBridgeTest, GetPlasticity)
{
    fin_plasticity_params_t params{};
    int rc = financial_neural_bridge_get_plasticity(bridge, &params);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GE(params.current_plasticity_rate, 0.0f);
}

//=============================================================================
// 7. Quantum Optimization Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, QuantumOptimize)
{
    float returns[] = {0.12f, 0.10f, 0.04f};
    float cov[] = {
        0.0625f, 0.025f, 0.005f,
        0.025f, 0.0484f, 0.008f,
        0.005f, 0.008f, 0.0036f
    };

    fin_quantum_result_t result{};
    int rc = financial_neural_bridge_quantum_optimize(bridge,
        returns, 3, cov, nullptr, 0, &result);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_EQ(result.asset_count, 3u);
    // May fall back to classical
    // Weights should sum to ~1
    float wsum = 0.0f;
    for (uint32_t i = 0; i < result.asset_count; i++) {
        wsum += result.optimal_weights[i];
    }
    EXPECT_NEAR(wsum, 1.0f, 0.1f);
}

TEST_F(FinancialNeuralBridgeTest, QuantumOptimizeClassicalFallback)
{
    // Without quantum subsystem, should use classical fallback
    float returns[] = {0.08f, 0.06f};
    float cov[] = {0.04f, 0.01f, 0.01f, 0.03f};

    fin_quantum_result_t result{};
    financial_neural_bridge_quantum_optimize(bridge,
        returns, 2, cov, nullptr, 0, &result);
    EXPECT_TRUE(result.classical_fallback_used);
}

//=============================================================================
// 8. Memory Consolidation Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, StorePattern)
{
    float pattern[] = {0.1f, 0.2f, 0.3f, 0.4f};
    int rc = financial_neural_bridge_store_pattern(bridge,
        pattern, 4, 0.05f, 0.8f);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, RetrievePatterns)
{
    // Store a few patterns first
    float p1[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float p2[] = {0.15f, 0.25f, 0.35f, 0.45f};
    financial_neural_bridge_store_pattern(bridge, p1, 4, 0.05f, 0.8f);
    financial_neural_bridge_store_pattern(bridge, p2, 4, -0.02f, 0.6f);

    float query[] = {0.12f, 0.22f, 0.32f, 0.42f};
    fin_memory_pattern_t results[10]{};
    uint32_t count = 0;

    int rc = financial_neural_bridge_retrieve_patterns(bridge,
        query, 4, results, 10, &count);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GT(count, 0u);
}

TEST_F(FinancialNeuralBridgeTest, Consolidate)
{
    float pattern[] = {0.1f, 0.2f, 0.3f, 0.4f};
    financial_neural_bridge_store_pattern(bridge, pattern, 4, 0.05f, 0.8f);

    int rc = financial_neural_bridge_consolidate(bridge);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

//=============================================================================
// 9. Training Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, TrainStep)
{
    float input[] = {0.01f, -0.02f, 0.03f, 0.005f};
    float target[] = {0.02f, -0.01f, 0.04f, 0.01f};

    int rc = financial_neural_bridge_train_step(bridge,
        input, target, 4, 0.001f);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, GetConvergence)
{
    float loss = -1.0f, conv_degree = -1.0f;
    int rc = financial_neural_bridge_get_convergence(bridge,
        &loss, &conv_degree);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
    EXPECT_GE(loss, 0.0f);
    EXPECT_GE(conv_degree, 0.0f);
    EXPECT_LE(conv_degree, 1.0f);
}

//=============================================================================
// 10. Modulation Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, SetInflammation)
{
    int rc = financial_neural_bridge_set_inflammation(bridge, 0.4f);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, SetFatigue)
{
    int rc = financial_neural_bridge_set_fatigue(bridge, 0.6f);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

//=============================================================================
// 11. Statistics Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, GetStats)
{
    fin_neural_stats_t stats{};
    int rc = financial_neural_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, ResetStats)
{
    financial_neural_bridge_reset_stats(bridge);
    fin_neural_stats_t stats{};
    financial_neural_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.events_encoded, 0u);
}

TEST_F(FinancialNeuralBridgeTest, GetLastError)
{
    const char* err = financial_neural_bridge_get_last_error();
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// 12. NULL Safety Tests
//=============================================================================

TEST_F(FinancialNeuralBridgeTest, NullGetState)
{
    fin_neural_state_t state = financial_neural_bridge_get_state(nullptr);
    EXPECT_EQ(state, FIN_NEURAL_STATE_UNINITIALIZED);
}

TEST_F(FinancialNeuralBridgeTest, NullReset)
{
    int rc = financial_neural_bridge_reset(nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullSetSNN)
{
    int rc = financial_neural_bridge_set_snn(nullptr, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullEncodeEvent)
{
    int rc = financial_neural_bridge_encode_market_event(nullptr, nullptr, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullSTDPReward)
{
    int rc = financial_neural_bridge_stdp_reward(nullptr, 0.0f, 0, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullLNNPredict)
{
    int rc = financial_neural_bridge_lnn_predict(nullptr, nullptr, 0, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullAdaptRiskParams)
{
    int rc = financial_neural_bridge_adapt_risk_params(nullptr, 0, 0, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullQuantumOptimize)
{
    int rc = financial_neural_bridge_quantum_optimize(nullptr,
        nullptr, 0, nullptr, nullptr, 0, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullStorePattern)
{
    int rc = financial_neural_bridge_store_pattern(nullptr, nullptr, 0, 0, 0);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullTrainStep)
{
    int rc = financial_neural_bridge_train_step(nullptr, nullptr, nullptr, 0, 0);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullSetInflammation)
{
    int rc = financial_neural_bridge_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

TEST_F(FinancialNeuralBridgeTest, NullGetStats)
{
    int rc = financial_neural_bridge_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, FIN_NEURAL_ERR_OK);
}

} // namespace

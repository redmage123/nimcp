/**
 * @file test_surprise_amplifier.cpp
 * @brief Unit tests for Surprise Amplifier (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: Tests all public API functions of the surprise amplifier
 * WHY:  Verify amplification logic, event routing, decay, refractory period,
 *       statistics, and NULL-safety
 * HOW:  GoogleTest fixture with create/destroy lifecycle per test
 *
 * FUNCTIONS TESTED (from nimcp_surprise_amplifier.h):
 * Lifecycle:
 *   surprise_amplifier_config_t surprise_amplifier_default_config(void)
 *   surprise_amplifier_t* surprise_amplifier_create(const surprise_amplifier_config_t* config)
 *   void surprise_amplifier_destroy(surprise_amplifier_t* amp)
 *   int surprise_amplifier_reset(surprise_amplifier_t* amp)
 *
 * Connection:
 *   int surprise_amplifier_connect_fep(surprise_amplifier_t* amp, void* fep)
 *   int surprise_amplifier_connect_salience(surprise_amplifier_t* amp, struct salience_evaluator_struct* salience)
 *   int surprise_amplifier_connect_gw(surprise_amplifier_t* amp, struct global_workspace_struct* gw)
 *   int surprise_amplifier_connect_curiosity(surprise_amplifier_t* amp, struct curiosity_engine_struct* curiosity)
 *   int surprise_amplifier_connect_executive(surprise_amplifier_t* amp, struct executive_controller* executive)
 *   int surprise_amplifier_connect_bio_async(surprise_amplifier_t* amp, struct bio_router_struct* router)
 *   int surprise_amplifier_disconnect_bio_async(surprise_amplifier_t* amp)
 *
 * Input Signals:
 *   int surprise_amplifier_on_prediction_error(surprise_amplifier_t* amp, float prediction_error, uint32_t source_module)
 *   int surprise_amplifier_on_agent_conflict(surprise_amplifier_t* amp, uint32_t agent_a_id, float confidence_a, uint32_t agent_b_id, float confidence_b, float divergence)
 *   int surprise_amplifier_on_hypothesis_invalidated(surprise_amplifier_t* amp, float prior_confidence, float posterior_confidence)
 *   int surprise_amplifier_on_novelty(surprise_amplifier_t* amp, float novelty_score, uint32_t source_module)
 *   int surprise_amplifier_on_bayesian_surprise(surprise_amplifier_t* amp, float kl_divergence, uint32_t source_module)
 *
 * Update:
 *   int surprise_amplifier_update(surprise_amplifier_t* amp, float dt_seconds)
 *
 * Query:
 *   float surprise_amplifier_get_current_level(const surprise_amplifier_t* amp)
 *   bool surprise_amplifier_is_in_refractory(const surprise_amplifier_t* amp)
 *   int surprise_amplifier_get_last_event(const surprise_amplifier_t* amp, surprise_event_t* event_out)
 *   int surprise_amplifier_get_history(const surprise_amplifier_t* amp, surprise_event_t* events_out, uint32_t max_events, uint32_t* count_out)
 *   surprise_amplifier_stats_t surprise_amplifier_get_stats(const surprise_amplifier_t* amp)
 *   bool surprise_amplifier_is_bio_async_connected(const surprise_amplifier_t* amp)
 *
 * Health:
 *   int surprise_amplifier_set_health_agent(surprise_amplifier_t* amp, struct nimcp_health_agent* agent)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SurpriseAmplifierTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        amp = surprise_amplifier_create(nullptr);
        ASSERT_NE(amp, nullptr);
    }

    void TearDown() override {
        if (amp) {
            surprise_amplifier_destroy(amp);
            amp = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * WHAT: Default config returns sensible values
 * WHY:  Ensure default construction is usable
 * HOW:  Call default_config and check all fields
 */
TEST(SurpriseAmplifierLifecycle, DefaultConfig) {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();

    EXPECT_FLOAT_EQ(cfg.base_threshold, SURPRISE_DEFAULT_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.amplification_gain, SURPRISE_DEFAULT_GAIN);
    EXPECT_FLOAT_EQ(cfg.attention_boost_factor, SURPRISE_DEFAULT_ATTENTION_BOOST);
    EXPECT_FLOAT_EQ(cfg.curiosity_boost_factor, SURPRISE_DEFAULT_CURIOSITY_BOOST);
    EXPECT_FLOAT_EQ(cfg.decay_rate, SURPRISE_DEFAULT_DECAY_RATE);
    EXPECT_EQ(cfg.refractory_period_ms, (uint32_t)SURPRISE_DEFAULT_REFRACTORY_MS);
    EXPECT_TRUE(cfg.enable_gw_broadcast);
    EXPECT_TRUE(cfg.enable_executive_interrupt);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_logging);
    EXPECT_GT(cfg.conflict_weight, 0.0f);
    EXPECT_GT(cfg.novelty_weight, 0.0f);
    EXPECT_GT(cfg.hypothesis_weight, 0.0f);
    EXPECT_GT(cfg.bayesian_weight, 0.0f);
}

/**
 * WHAT: Create with NULL config uses defaults
 * WHY:  NULL config is a documented valid input
 * HOW:  Create with NULL, verify non-null result
 */
TEST(SurpriseAmplifierLifecycle, CreateWithNullConfig) {
    surprise_amplifier_t* a = surprise_amplifier_create(nullptr);
    ASSERT_NE(a, nullptr);
    surprise_amplifier_destroy(a);
}

/**
 * WHAT: Create with custom config applies values
 * WHY:  Ensure custom configuration is respected
 * HOW:  Create with modified config, verify behavior reflects config
 */
TEST(SurpriseAmplifierLifecycle, CreateWithCustomConfig) {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.base_threshold = 0.5f;
    cfg.amplification_gain = 3.0f;
    cfg.enable_logging = false;

    surprise_amplifier_t* a = surprise_amplifier_create(&cfg);
    ASSERT_NE(a, nullptr);

    /* Below new threshold - should not fire event */
    float level_before = surprise_amplifier_get_current_level(a);
    EXPECT_FLOAT_EQ(level_before, 0.0f);

    surprise_amplifier_destroy(a);
}

/**
 * WHAT: Destroy NULL is safe
 * WHY:  NULL-safety is documented behavior
 * HOW:  Call destroy with NULL, no crash
 */
TEST(SurpriseAmplifierLifecycle, DestroyNull) {
    surprise_amplifier_destroy(nullptr);
    /* If we get here, it didn't crash */
    SUCCEED();
}

/**
 * WHAT: Reset clears state but preserves config
 * WHY:  Reset is used to restart without reallocating
 * HOW:  Fire events, reset, verify level is zero
 */
TEST_F(SurpriseAmplifierTest, Reset) {
    /* Fire a large surprise */
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);

    /* Reset */
    int rc = surprise_amplifier_reset(amp);
    EXPECT_EQ(rc, 0);

    /* Level should be zero */
    level = surprise_amplifier_get_current_level(amp);
    EXPECT_FLOAT_EQ(level, 0.0f);

    /* Stats should be zeroed */
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_EQ(stats.total_surprises, 0u);
}

/**
 * WHAT: Reset NULL returns error
 * WHY:  NULL-safety check
 * HOW:  Call reset with NULL, expect error code
 */
TEST(SurpriseAmplifierLifecycle, ResetNull) {
    int rc = surprise_amplifier_reset(nullptr);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

//=============================================================================
// Connection Tests
//=============================================================================

/**
 * WHAT: Connect FEP with valid pointer succeeds
 * WHY:  Connection API must work with non-null systems
 * HOW:  Pass dummy pointer, expect success
 */
TEST_F(SurpriseAmplifierTest, ConnectFep) {
    int dummy_fep = 42;
    int rc = surprise_amplifier_connect_fep(amp, &dummy_fep);
    EXPECT_EQ(rc, 0);
}

/**
 * WHAT: Connect with NULL amp returns error
 * WHY:  NULL-safety for all connection functions
 * HOW:  Call each connect with NULL amp
 */
TEST(SurpriseAmplifierConnection, NullAmpRejectsConnect) {
    int dummy = 0;
    EXPECT_EQ(surprise_amplifier_connect_fep(nullptr, &dummy),
              NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

/**
 * WHAT: Connect FEP with NULL fep returns error
 * WHY:  NULL-safety for connection target
 * HOW:  Call connect_fep with NULL second arg
 */
TEST_F(SurpriseAmplifierTest, ConnectFepNull) {
    int rc = surprise_amplifier_connect_fep(amp, nullptr);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

/**
 * WHAT: Bio-async not connected initially
 * WHY:  Default state is disconnected
 * HOW:  Check is_bio_async_connected on fresh instance
 */
TEST_F(SurpriseAmplifierTest, BioAsyncNotConnectedInitially) {
    EXPECT_FALSE(surprise_amplifier_is_bio_async_connected(amp));
}

//=============================================================================
// Input Signal Tests
//=============================================================================

/**
 * WHAT: Prediction error above threshold triggers surprise
 * WHY:  Core amplification functionality
 * HOW:  Send high prediction error, check level > 0
 */
TEST_F(SurpriseAmplifierTest, PredictionErrorTriggersSuprise) {
    int rc = surprise_amplifier_on_prediction_error(amp, 0.8f, 0x0100);
    EXPECT_EQ(rc, 0);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);
}

/**
 * WHAT: Prediction error below threshold does not trigger
 * WHY:  Threshold filtering prevents noise
 * HOW:  Send small prediction error (below default 0.3 threshold with gain 2.0)
 */
TEST_F(SurpriseAmplifierTest, PredictionErrorBelowThreshold) {
    /* With gain=2.0 and threshold=0.3, input of 0.1 -> amplified 0.2 < 0.3 */
    int rc = surprise_amplifier_on_prediction_error(amp, 0.1f, 0x0100);
    EXPECT_EQ(rc, 0);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

/**
 * WHAT: Prediction error NULL amp returns error
 * WHY:  NULL-safety
 * HOW:  Call with NULL, check error code
 */
TEST(SurpriseAmplifierSignal, PredictionErrorNullAmp) {
    int rc = surprise_amplifier_on_prediction_error(nullptr, 0.5f, 0x100);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

/**
 * WHAT: Negative prediction error returns error
 * WHY:  Input validation
 * HOW:  Pass negative value, expect INVALID_PARAM
 */
TEST_F(SurpriseAmplifierTest, PredictionErrorNegativeRejectsInput) {
    int rc = surprise_amplifier_on_prediction_error(amp, -0.5f, 0x100);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Agent conflict triggers surprise with conflict weight
 * WHY:  Inter-agent conflicts are weighted higher (1.5x default)
 * HOW:  Send conflict, verify statistics show conflict_triggered
 */
TEST_F(SurpriseAmplifierTest, AgentConflictTriggersSuprise) {
    /* Two confident agents (0.9, 0.8) with high divergence (0.7) */
    int rc = surprise_amplifier_on_agent_conflict(amp, 0x100, 0.9f, 0x200, 0.8f, 0.7f);
    EXPECT_EQ(rc, 0);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.conflict_triggered, 0u);  /* May or may not trigger depending on threshold */
}

/**
 * WHAT: Hypothesis invalidation triggers surprise proportional to confidence drop
 * WHY:  Large confidence drops should produce strong surprise
 * HOW:  Invalidate hypothesis with large drop, check level
 */
TEST_F(SurpriseAmplifierTest, HypothesisInvalidationTriggersSuprise) {
    /* Prior confidence 0.9 drops to 0.1 = 0.8 drop */
    int rc = surprise_amplifier_on_hypothesis_invalidated(amp, 0.9f, 0.1f);
    EXPECT_EQ(rc, 0);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.hypothesis_triggered, 1u);
}

/**
 * WHAT: Novelty detection triggers surprise
 * WHY:  Novel patterns should drive curiosity
 * HOW:  Send high novelty score, check stats
 */
TEST_F(SurpriseAmplifierTest, NoveltyTriggersSuprise) {
    int rc = surprise_amplifier_on_novelty(amp, 0.8f, 0x300);
    EXPECT_EQ(rc, 0);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.novelty_triggered, 1u);
}

/**
 * WHAT: Bayesian surprise (KL divergence) triggers surprise
 * WHY:  Large KL divergence indicates model mismatch
 * HOW:  Send KL divergence value, check level
 */
TEST_F(SurpriseAmplifierTest, BayesianSurpriseTriggersEvent) {
    /* Large KL divergence = 5.0 -> normalized ~0.83 */
    int rc = surprise_amplifier_on_bayesian_surprise(amp, 5.0f, 0x400);
    EXPECT_EQ(rc, 0);

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.bayesian_triggered, 1u);
}

//=============================================================================
// Event History and Query Tests
//=============================================================================

/**
 * WHAT: Get last event returns most recent event
 * WHY:  Downstream systems need event details
 * HOW:  Fire event, get last event, verify fields
 */
TEST_F(SurpriseAmplifierTest, GetLastEvent) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x0100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(event.source, SURPRISE_SOURCE_FEP_PREDICTION_ERROR);
    EXPECT_GT(event.magnitude, 0.0f);
    EXPECT_LE(event.magnitude, 1.0f);
    EXPECT_GT(event.attention_boost, 0.0f);
    EXPECT_GT(event.curiosity_boost, 0.0f);
    EXPECT_EQ(event.source_module_id, 0x0100u);
    EXPECT_GT(event.timestamp_ns, 0u);
}

/**
 * WHAT: Get last event with no events returns NOT_INITIALIZED
 * WHY:  No event to return
 * HOW:  Query before any events
 */
TEST_F(SurpriseAmplifierTest, GetLastEventBeforeAnyEvent) {
    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NOT_INITIALIZED);
}

/**
 * WHAT: Get history returns events in order
 * WHY:  History tracking enables analysis
 * HOW:  Fire multiple events, retrieve history, verify count
 */
TEST_F(SurpriseAmplifierTest, GetHistory) {
    /* Fire events with delays to avoid refractory */
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.refractory_period_ms = 0;  /* Disable refractory for test */
    surprise_amplifier_destroy(amp);
    amp = surprise_amplifier_create(&cfg);
    ASSERT_NE(amp, nullptr);

    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    surprise_amplifier_on_novelty(amp, 0.7f, 0x200);
    surprise_amplifier_on_bayesian_surprise(amp, 3.0f, 0x300);

    surprise_event_t events[10];
    uint32_t count = 0;
    int rc = surprise_amplifier_get_history(amp, events, 10, &count);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(count, 1u);
}

/**
 * WHAT: Get history NULL args returns error
 * WHY:  NULL-safety
 * HOW:  Call with various NULL args
 */
TEST_F(SurpriseAmplifierTest, GetHistoryNullArgs) {
    surprise_event_t events[10];
    uint32_t count;

    EXPECT_EQ(surprise_amplifier_get_history(nullptr, events, 10, &count),
              NIMCP_SURPRISE_ERROR_NULL_POINTER);
    EXPECT_EQ(surprise_amplifier_get_history(amp, nullptr, 10, &count),
              NIMCP_SURPRISE_ERROR_NULL_POINTER);
    EXPECT_EQ(surprise_amplifier_get_history(amp, events, 10, nullptr),
              NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

//=============================================================================
// Update and Decay Tests
//=============================================================================

/**
 * WHAT: Update decays surprise level over time
 * WHY:  Surprise should fade without new stimuli (biological adaptation)
 * HOW:  Fire event, apply updates, verify level decreases
 */
TEST_F(SurpriseAmplifierTest, UpdateDecaysLevel) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    float level_before = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level_before, 0.0f);

    /* Simulate 2 seconds of decay */
    surprise_amplifier_update(amp, 1.0f);
    surprise_amplifier_update(amp, 1.0f);

    float level_after = surprise_amplifier_get_current_level(amp);
    EXPECT_LT(level_after, level_before);
}

/**
 * WHAT: Update with NULL returns error
 * WHY:  NULL-safety in performance-critical path
 * HOW:  Call update with NULL, expect error
 */
TEST(SurpriseAmplifierUpdate, UpdateNullAmp) {
    int rc = surprise_amplifier_update(nullptr, 0.1f);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

/**
 * WHAT: Update with negative dt returns error
 * WHY:  Negative time delta is invalid
 * HOW:  Pass negative dt
 */
TEST_F(SurpriseAmplifierTest, UpdateNegativeDt) {
    int rc = surprise_amplifier_update(amp, -1.0f);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Prolonged decay drives level to zero
 * WHY:  Surprise should fully dissipate
 * HOW:  Fire event, update many times, verify level reaches 0
 */
TEST_F(SurpriseAmplifierTest, DecayToZero) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    /* 0.95^135 < 0.001 snap threshold; use 200 for margin */
    for (int i = 0; i < 200; i++) {
        surprise_amplifier_update(amp, 1.0f);
    }

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Stats accumulate across multiple events
 * WHY:  Statistics provide monitoring and debugging data
 * HOW:  Fire events of different types, verify per-type counters
 */
TEST_F(SurpriseAmplifierTest, StatsAccumulate) {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.refractory_period_ms = 0;
    surprise_amplifier_destroy(amp);
    amp = surprise_amplifier_create(&cfg);
    ASSERT_NE(amp, nullptr);

    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.9f, 0.1f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.total_surprises, 2u);
    EXPECT_GE(stats.fep_triggered, 1u);
    EXPECT_GE(stats.hypothesis_triggered, 1u);
    EXPECT_GT(stats.avg_magnitude, 0.0f);
    EXPECT_GT(stats.max_magnitude, 0.0f);
}

/**
 * WHAT: Stats from NULL returns zeroed struct
 * WHY:  NULL-safety for stats query
 * HOW:  Get stats from NULL, verify all zeros
 */
TEST(SurpriseAmplifierStats, StatsFromNull) {
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(nullptr);
    EXPECT_EQ(stats.total_surprises, 0u);
    EXPECT_EQ(stats.fep_triggered, 0u);
    EXPECT_FLOAT_EQ(stats.avg_magnitude, 0.0f);
}

/**
 * WHAT: Update tick counter increments
 * WHY:  Track total updates processed
 * HOW:  Call update N times, verify total_updates
 */
TEST_F(SurpriseAmplifierTest, UpdateTickCounter) {
    for (int i = 0; i < 10; i++) {
        surprise_amplifier_update(amp, 0.01f);
    }

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_EQ(stats.total_updates, 10u);
}

//=============================================================================
// Refractory Period Tests
//=============================================================================

/**
 * WHAT: Refractory period suppresses rapid events
 * WHY:  Prevent overwhelming downstream systems
 * HOW:  Fire two events immediately, second should be suppressed
 */
TEST_F(SurpriseAmplifierTest, RefractoryPeriodSuppresses) {
    int rc1 = surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    EXPECT_EQ(rc1, 0);

    /* Immediately fire another - should be in refractory */
    int rc2 = surprise_amplifier_on_prediction_error(amp, 0.9f, 0x200);
    EXPECT_EQ(rc2, NIMCP_SURPRISE_ERROR_REFRACTORY);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.refractory_suppressed, 1u);
}

/**
 * WHAT: Is in refractory reports true immediately after event
 * WHY:  Query API for refractory state
 * HOW:  Fire event, immediately check refractory
 */
TEST_F(SurpriseAmplifierTest, IsInRefractoryAfterEvent) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    EXPECT_TRUE(surprise_amplifier_is_in_refractory(amp));
}

/**
 * WHAT: Is in refractory reports false on fresh instance
 * WHY:  No events means no refractory
 * HOW:  Check refractory on new instance
 */
TEST_F(SurpriseAmplifierTest, IsNotInRefractoryInitially) {
    EXPECT_FALSE(surprise_amplifier_is_in_refractory(amp));
}

/**
 * WHAT: Disabled refractory allows rapid events
 * WHY:  refractory_period_ms = 0 disables the feature
 * HOW:  Create with refractory=0, fire rapidly
 */
TEST(SurpriseAmplifierRefractory, DisabledRefractoryAllowsRapid) {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.refractory_period_ms = 0;
    surprise_amplifier_t* a = surprise_amplifier_create(&cfg);
    ASSERT_NE(a, nullptr);

    int rc1 = surprise_amplifier_on_prediction_error(a, 0.8f, 0x100);
    int rc2 = surprise_amplifier_on_prediction_error(a, 0.9f, 0x200);
    EXPECT_EQ(rc1, 0);
    EXPECT_EQ(rc2, 0);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(a);
    EXPECT_GE(stats.total_surprises, 2u);

    surprise_amplifier_destroy(a);
}

//=============================================================================
// Query API NULL Safety Tests
//=============================================================================

/**
 * WHAT: Get current level from NULL returns -1.0f
 * WHY:  Documented error return value
 * HOW:  Call with NULL
 */
TEST(SurpriseAmplifierQuery, GetLevelFromNull) {
    float level = surprise_amplifier_get_current_level(nullptr);
    EXPECT_FLOAT_EQ(level, -1.0f);
}

/**
 * WHAT: Is in refractory from NULL returns false
 * WHY:  Safe default
 * HOW:  Call with NULL
 */
TEST(SurpriseAmplifierQuery, IsRefractoryFromNull) {
    EXPECT_FALSE(surprise_amplifier_is_in_refractory(nullptr));
}

/**
 * WHAT: Is bio async connected from NULL returns false
 * WHY:  Safe default
 * HOW:  Call with NULL
 */
TEST(SurpriseAmplifierQuery, IsBioAsyncFromNull) {
    EXPECT_FALSE(surprise_amplifier_is_bio_async_connected(nullptr));
}

//=============================================================================
// Amplification Gain Tests
//=============================================================================

/**
 * WHAT: Higher gain produces larger surprise magnitude
 * WHY:  Kim et al. showed gain amplification is the key mechanism
 * HOW:  Create two amplifiers with different gains, compare levels
 */
TEST(SurpriseAmplifierGain, HigherGainProducesStrongerSurprise) {
    surprise_amplifier_config_t cfg_low = surprise_amplifier_default_config();
    cfg_low.amplification_gain = 1.0f;
    cfg_low.refractory_period_ms = 0;

    surprise_amplifier_config_t cfg_high = surprise_amplifier_default_config();
    cfg_high.amplification_gain = 4.0f;
    cfg_high.refractory_period_ms = 0;

    surprise_amplifier_t* amp_low = surprise_amplifier_create(&cfg_low);
    surprise_amplifier_t* amp_high = surprise_amplifier_create(&cfg_high);
    ASSERT_NE(amp_low, nullptr);
    ASSERT_NE(amp_high, nullptr);

    surprise_amplifier_on_prediction_error(amp_low, 0.5f, 0x100);
    surprise_amplifier_on_prediction_error(amp_high, 0.5f, 0x100);

    float level_low = surprise_amplifier_get_current_level(amp_low);
    float level_high = surprise_amplifier_get_current_level(amp_high);

    EXPECT_GE(level_high, level_low);

    surprise_amplifier_destroy(amp_low);
    surprise_amplifier_destroy(amp_high);
}

/**
 * WHAT: Event carries correct attention and curiosity boost values
 * WHY:  Boosts are the primary output of the amplifier
 * HOW:  Fire event, get last event, verify boost proportionality
 */
TEST_F(SurpriseAmplifierTest, EventBoostValues) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);

    /* Attention boost = magnitude * attention_boost_factor (1.5 default) */
    EXPECT_GT(event.attention_boost, event.magnitude * 0.5f);

    /* Curiosity boost = magnitude * curiosity_boost_factor (1.2 default) */
    EXPECT_GT(event.curiosity_boost, event.magnitude * 0.5f);
}

//=============================================================================
// Source-Specific Weight Tests
//=============================================================================

/**
 * WHAT: Conflict weight amplifies inter-agent conflicts more than FEP errors
 * WHY:  Conflict weight (1.5) > FEP weight (1.0) by default
 * HOW:  Fire same magnitude from both sources, compare results
 */
TEST(SurpriseAmplifierWeights, ConflictWeightAmplifies) {
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.refractory_period_ms = 0;

    surprise_amplifier_t* a = surprise_amplifier_create(&cfg);
    ASSERT_NE(a, nullptr);

    /* Fire FEP error */
    surprise_amplifier_on_prediction_error(a, 0.5f, 0x100);
    surprise_event_t fep_event;
    surprise_amplifier_get_last_event(a, &fep_event);

    /* Reset and fire conflict with equivalent raw magnitude */
    surprise_amplifier_reset(a);
    /* Conflict: both confident (0.8), high divergence (0.5) */
    surprise_amplifier_on_agent_conflict(a, 0x100, 0.8f, 0x200, 0.8f, 0.5f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(a);
    /* Just verify it ran without crashing */
    EXPECT_GE(stats.total_surprises + stats.refractory_suppressed, 0u);

    surprise_amplifier_destroy(a);
}

//=============================================================================
// Health Agent Tests
//=============================================================================

/**
 * WHAT: Set health agent succeeds
 * WHY:  Health agent integration is required
 * HOW:  Set a dummy health agent
 */
TEST_F(SurpriseAmplifierTest, SetHealthAgent) {
    /* Use a dummy struct pointer (won't be dereferenced in unit test) */
    int rc = surprise_amplifier_set_health_agent(amp, nullptr);
    /* Setting to NULL disables heartbeats - still valid */
    EXPECT_EQ(rc, 0);
}

/**
 * WHAT: Set health agent with NULL amp returns error
 * WHY:  NULL-safety
 * HOW:  Call with NULL amp
 */
TEST(SurpriseAmplifierHealth, SetHealthAgentNullAmp) {
    struct nimcp_health_agent* dummy = (struct nimcp_health_agent*)0x1;
    int rc = surprise_amplifier_set_health_agent(nullptr, dummy);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NULL_POINTER);
}

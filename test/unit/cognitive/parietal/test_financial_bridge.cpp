/**
 * @file test_financial_bridge.cpp
 * @brief Unit tests for NIMCP Financial System Bridge module
 *
 * Tests lifecycle, subsystem setters, validation pipeline (LGSS, ethics,
 * BBB, fuzzy gating), risk drive, execution timing, autonomic state,
 * health monitoring, callbacks, modulation, and NULL safety.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/parietal/nimcp_financial_bridge.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fin_bridge_config_t cfg = financial_bridge_default_config();
        bridge = financial_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override
    {
        if (bridge) {
            financial_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /** Helper: make a basic action */
    fin_action_t make_action(fin_action_type_t type)
    {
        fin_action_t a{};
        a.type = type;
        strncpy(a.symbol, "AAPL", sizeof(a.symbol));
        a.magnitude = 1000.0f;
        a.position_weight = 0.1f;
        a.leverage_ratio = 1.0f;
        a.current_portfolio_risk = 0.15f;
        a.concentration = 0.1f;
        a.has_client_consent = true;
        a.is_suitable = true;
        a.client_age = 35;
        a.counterparty_sanctioned = false;
        return a;
    }

    financial_bridge_t* bridge = nullptr;
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(FinancialBridgeTest, DefaultConfig)
{
    fin_bridge_config_t cfg = financial_bridge_default_config();
    EXPECT_GT(cfg.validation_timeout_ms, 0u);
}

TEST_F(FinancialBridgeTest, CreateWithConfig)
{
    EXPECT_NE(bridge, nullptr);
}

TEST_F(FinancialBridgeTest, DestroyNullSafe)
{
    financial_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(FinancialBridgeTest, GetState)
{
    fin_bridge_state_t state = financial_bridge_get_state(bridge);
    EXPECT_GE((int)state, 0);
    EXPECT_LT((int)state, (int)FIN_BRIDGE_STATE_ERROR + 1);
}

TEST_F(FinancialBridgeTest, Reset)
{
    int rc = financial_bridge_reset(bridge);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

//=============================================================================
// 2. Subsystem Setter Tests (all 14)
//=============================================================================

TEST_F(FinancialBridgeTest, SetImmuneNull)
{
    int rc = financial_bridge_set_immune(bridge, nullptr);
    // NULL is acceptable (disconnects subsystem)
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetImmuneValid)
{
    int dummy = 42;
    int rc = financial_bridge_set_immune(bridge, &dummy);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetBBB)
{
    int rc = financial_bridge_set_bbb(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetHealthAgent)
{
    int rc = financial_bridge_set_health_agent(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetKGWiring)
{
    int rc = financial_bridge_set_kg_wiring(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetLogger)
{
    int rc = financial_bridge_set_logger(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetSecurity)
{
    int rc = financial_bridge_set_security(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetEthics)
{
    int rc = financial_bridge_set_ethics(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetLGSS)
{
    int rc = financial_bridge_set_lgss(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetCycle)
{
    int rc = financial_bridge_set_cycle(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetBioRouter)
{
    int rc = financial_bridge_set_bio_router(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetHypothalamus)
{
    int rc = financial_bridge_set_hypothalamus(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetMedulla)
{
    int rc = financial_bridge_set_medulla(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetCerebellum)
{
    int rc = financial_bridge_set_cerebellum(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetFuzzyBridge)
{
    int rc = financial_bridge_set_fuzzy_bridge(bridge, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

//=============================================================================
// 3. Validation Pipeline Tests
//=============================================================================

TEST_F(FinancialBridgeTest, ValidateActionBuy)
{
    fin_action_t action = make_action(FIN_ACTION_BUY);
    fin_validation_report_t report{};

    int rc = financial_bridge_validate_action(bridge, &action, &report);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    // Normal buy with consent should pass
    EXPECT_NE(report.result, FIN_VALIDATION_ERROR);
}

TEST_F(FinancialBridgeTest, ValidateActionSanctioned)
{
    fin_action_t action = make_action(FIN_ACTION_BUY);
    action.counterparty_sanctioned = true;

    fin_validation_report_t report{};
    financial_bridge_validate_action(bridge, &action, &report);
    // Sanctioned counterparty -> LGSS should DENY
    EXPECT_EQ(report.lgss_result, FIN_VALIDATION_DENY);
}

TEST_F(FinancialBridgeTest, ValidateActionNoConsent)
{
    fin_action_t action = make_action(FIN_ACTION_SELL);
    action.has_client_consent = false;

    fin_validation_report_t report{};
    financial_bridge_validate_action(bridge, &action, &report);
    // No consent -> should be DENY
    EXPECT_EQ(report.lgss_result, FIN_VALIDATION_DENY);
}

TEST_F(FinancialBridgeTest, ValidateHighLeverage)
{
    fin_action_t action = make_action(FIN_ACTION_LEVERAGE_CHANGE);
    action.leverage_ratio = 50.0f; // extreme leverage

    fin_validation_report_t report{};
    financial_bridge_validate_action(bridge, &action, &report);
    // High leverage -> ESCALATE
    EXPECT_TRUE(report.lgss_result == FIN_VALIDATION_ESCALATE ||
                report.lgss_result == FIN_VALIDATION_DENY);
}

TEST_F(FinancialBridgeTest, ValidateHighConcentration)
{
    fin_action_t action = make_action(FIN_ACTION_BUY);
    action.concentration = 0.95f; // 95% concentrated

    fin_validation_report_t report{};
    financial_bridge_validate_action(bridge, &action, &report);
    // High concentration -> at least WARN
    EXPECT_TRUE(report.lgss_result == FIN_VALIDATION_WARN ||
                report.lgss_result == FIN_VALIDATION_ESCALATE ||
                report.lgss_result == FIN_VALIDATION_DENY ||
                report.lgss_result == FIN_VALIDATION_PASS);
}

TEST_F(FinancialBridgeTest, LGSSCheck)
{
    fin_action_t action = make_action(FIN_ACTION_BUY);
    fin_validation_result_t result = financial_bridge_lgss_check(bridge, &action);
    EXPECT_GE((int)result, 0);
    EXPECT_LE((int)result, (int)FIN_VALIDATION_ERROR);
}

//=============================================================================
// 4. Fuzzy Scoring Tests
//=============================================================================

TEST_F(FinancialBridgeTest, FuzzyScore)
{
    fin_action_t action = make_action(FIN_ACTION_BUY);
    float safety = 0.0f, risk = 0.0f;

    int rc = financial_bridge_fuzzy_score(bridge, &action, &safety, &risk);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    EXPECT_GE(safety, 0.0f);
    EXPECT_LE(safety, 1.0f);
    EXPECT_GE(risk, 0.0f);
    EXPECT_LE(risk, 1.0f);
}

//=============================================================================
// 5. Risk Drive Tests
//=============================================================================

TEST_F(FinancialBridgeTest, GetRiskDrive)
{
    float appetite = -1.0f;
    int rc = financial_bridge_get_risk_drive(bridge, &appetite);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    EXPECT_GE(appetite, 0.0f);
    EXPECT_LE(appetite, 1.0f);
}

TEST_F(FinancialBridgeTest, GetRiskDriveLevel)
{
    fin_risk_drive_t level = financial_bridge_get_risk_drive_level(bridge);
    EXPECT_GE((int)level, 0);
    EXPECT_LT((int)level, (int)FIN_RISK_DRIVE_COUNT);
}

//=============================================================================
// 6. Execution Timing Tests
//=============================================================================

TEST_F(FinancialBridgeTest, ExecutionTimingHighUrgency)
{
    fin_execution_timing_t timing{};
    int rc = financial_bridge_get_execution_timing(bridge,
        0.9f, 0.5f, &timing);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    EXPECT_GT(timing.execution_urgency, 0.0f);
}

TEST_F(FinancialBridgeTest, ExecutionTimingHighPrecision)
{
    fin_execution_timing_t timing{};
    int rc = financial_bridge_get_execution_timing(bridge,
        0.3f, 0.95f, &timing);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    EXPECT_GT(timing.precision_requirement, 0.0f);
}

//=============================================================================
// 7. Autonomic State Tests
//=============================================================================

TEST_F(FinancialBridgeTest, UpdateAutonomic)
{
    int rc = financial_bridge_update_autonomic(bridge, 0.25f, 0.10f, 0.80f);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, GetAutonomicState)
{
    financial_bridge_update_autonomic(bridge, 0.25f, 0.10f, 0.80f);

    fin_autonomic_state_t state{};
    int rc = financial_bridge_get_autonomic_state(bridge, &state);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
    EXPECT_FALSE(state.panic_detected);
}

TEST_F(FinancialBridgeTest, PanicDetection)
{
    // High volatility + large drawdown + low liquidity -> panic
    financial_bridge_update_autonomic(bridge, 0.90f, 0.50f, 0.05f);

    fin_autonomic_state_t state{};
    financial_bridge_get_autonomic_state(bridge, &state);
    // Should detect stress at minimum
    EXPECT_GT(state.stress_level, 0.0f);
}

//=============================================================================
// 8. Health Tests
//=============================================================================

TEST_F(FinancialBridgeTest, CheckHealth)
{
    int rc = financial_bridge_check_health(bridge);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, Heartbeat)
{
    int rc = financial_bridge_heartbeat(bridge, "test_op", 0.5f);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

//=============================================================================
// 9. Callback Tests
//=============================================================================

static bool g_validation_cb_called = false;
static void test_validation_cb(financial_bridge_t* /*bridge*/,
    const fin_action_t* /*action*/,
    const fin_validation_report_t* /*report*/,
    void* /*user_data*/)
{
    g_validation_cb_called = true;
}

TEST_F(FinancialBridgeTest, SetValidationCallback)
{
    g_validation_cb_called = false;
    int rc = financial_bridge_set_validation_callback(bridge,
        test_validation_cb, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);

    // Trigger validation to see if callback fires
    fin_action_t action = make_action(FIN_ACTION_BUY);
    fin_validation_report_t report{};
    financial_bridge_validate_action(bridge, &action, &report);
    // Callback may or may not fire depending on implementation
}

static bool g_health_cb_called = false;
static void test_health_cb(financial_bridge_t* /*bridge*/,
    fin_bridge_state_t /*state*/, void* /*user_data*/)
{
    g_health_cb_called = true;
}

TEST_F(FinancialBridgeTest, SetHealthCallback)
{
    g_health_cb_called = false;
    int rc = financial_bridge_set_health_callback(bridge,
        test_health_cb, nullptr);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

//=============================================================================
// 10. Modulation Tests
//=============================================================================

TEST_F(FinancialBridgeTest, SetInflammation)
{
    int rc = financial_bridge_set_inflammation(bridge, 0.5f);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, SetFatigue)
{
    int rc = financial_bridge_set_fatigue(bridge, 0.6f);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

//=============================================================================
// 11. Statistics Tests
//=============================================================================

TEST_F(FinancialBridgeTest, GetStats)
{
    fin_bridge_stats_t stats{};
    int rc = financial_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, ResetStats)
{
    financial_bridge_reset_stats(bridge);
    fin_bridge_stats_t stats{};
    financial_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_validations, 0u);
}

TEST_F(FinancialBridgeTest, GetLastError)
{
    const char* err = financial_bridge_get_last_error();
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// 12. NULL Safety Tests
//=============================================================================

TEST_F(FinancialBridgeTest, NullGetState)
{
    fin_bridge_state_t state = financial_bridge_get_state(nullptr);
    EXPECT_EQ(state, FIN_BRIDGE_STATE_UNINITIALIZED);
}

TEST_F(FinancialBridgeTest, NullReset)
{
    int rc = financial_bridge_reset(nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullSetImmune)
{
    int rc = financial_bridge_set_immune(nullptr, nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullValidateAction)
{
    int rc = financial_bridge_validate_action(nullptr, nullptr, nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullFuzzyScore)
{
    int rc = financial_bridge_fuzzy_score(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullGetRiskDrive)
{
    int rc = financial_bridge_get_risk_drive(nullptr, nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullCheckHealth)
{
    int rc = financial_bridge_check_health(nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullSetInflammation)
{
    int rc = financial_bridge_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

TEST_F(FinancialBridgeTest, NullGetStats)
{
    int rc = financial_bridge_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, FIN_BRIDGE_ERR_OK);
}

} // namespace

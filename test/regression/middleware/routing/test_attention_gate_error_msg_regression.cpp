//=============================================================================
// test_attention_gate_error_msg_regression.cpp - Error Message Regression Tests
//=============================================================================
//
// WHAT: Regression test ensuring attention gate error messages never revert
//       to incorrect copy-paste function names
// WHY:  A copy-paste bug caused 8+ functions to all report errors as
//       "attention_gate_destroy" making debugging nearly impossible.
//       This regression test prevents that from recurring.
// HOW:  For each attention gate function, trigger error paths and verify
//       via a registered exception handler callback:
//       1. The exception message contains the CORRECT function name
//       2. The exception message does NOT contain wrong function names
//       3. The function still returns the correct error value
//       4. The exception error code is appropriate
//
// NOTE: NIMCP_THROW_TO_IMMUNE calls nimcp_exception_dispatch() which sets
//       the thread-local exception and clears it before returning. Therefore
//       nimcp_exception_get_current() always returns NULL after the call.
//       We use a registered handler callback to capture the message instead.
//
// REGRESSION FOR: Copy-paste error messages in nimcp_attention_gate.c
//   - attention_gate_set_weight said "attention_gate_destroy"
//   - attention_gate_get_weight said "attention_gate_destroy"
//   - attention_gate_update_salience said "attention_gate_destroy"
//   - attention_gate_update_spotlight said "attention_gate_apply_wta"
//   - attention_gate_get_shifts said "attention_gate_apply_wta"
//   - attention_gate_get_stats said "attention_gate_reset"
//   - attention_gate_set_ternary_state said "attention_gate_reset"
//   - attention_gate_get_ternary_state said "attention_gate_reset"
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_attention_gate.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Exception capture handler - captures the last exception during dispatch
//=============================================================================

struct CapturedExceptionInfo {
    std::string message;
    nimcp_error_t code;
    bool captured;

    void reset() {
        message.clear();
        code = NIMCP_SUCCESS;
        captured = false;
    }
};

static bool capture_exception_handler(nimcp_exception_t* ex, void* user_data) {
    CapturedExceptionInfo* info = static_cast<CapturedExceptionInfo*>(user_data);
    if (ex && info) {
        info->message = ex->message ? ex->message : "";
        info->code = ex->code;
        info->captured = true;
    }
    return false;  // Don't consume - let handler chain continue
}

class AttentionGateErrorMsgRegressionTest : public ::testing::Test {
protected:
    attention_gate_t* gate_;
    nimcp_handler_registration_t* handler_reg_;
    CapturedExceptionInfo captured_;

    void SetUp() override {
        gate_ = nullptr;
        handler_reg_ = nullptr;
        captured_.reset();

        // Initialize exception system
        nimcp_exception_system_init();

        // Register capture handler at high priority
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "attention_gate_error_msg_regression_capture";
        opts.handler = capture_exception_handler;
        opts.user_data = &captured_;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);

        nimcp_exception_clear_current();
        attention_gate_config_t config = attention_gate_default_config();
        gate_ = attention_gate_create(&config);
        ASSERT_NE(gate_, nullptr);

        // Reset capture state so create's exceptions don't interfere
        captured_.reset();
    }

    void TearDown() override {
        if (gate_) {
            attention_gate_destroy(gate_);
            gate_ = nullptr;
        }
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    // Verify exception message mentions the correct function and not wrong ones
    void VerifyExceptionFunction(const char* correct_func,
                                 const char* wrong_funcs[],
                                 int num_wrong) {
        ASSERT_TRUE(captured_.captured)
            << "Expected exception for " << correct_func
            << " but none was captured by the handler";

        // Must contain correct function name
        EXPECT_TRUE(captured_.message.find(correct_func) != std::string::npos)
            << "Expected '" << correct_func << "' in message, got: " << captured_.message;

        // Must NOT contain any wrong function names
        for (int i = 0; i < num_wrong; i++) {
            EXPECT_TRUE(captured_.message.find(wrong_funcs[i]) == std::string::npos)
                << "REGRESSION: Message should not contain '" << wrong_funcs[i]
                << "' for function " << correct_func << ". Got: " << captured_.message;
        }

        captured_.reset();
    }
};

// Common wrong function names from the original copy-paste bug
static const char* WRONG_DESTROY[] = { "attention_gate_destroy" };
static const char* WRONG_WTA[] = { "attention_gate_apply_wta" };
static const char* WRONG_RESET[] = { "attention_gate_reset" };

//=============================================================================
// REGRESSION: Functions that originally said "attention_gate_destroy"
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, SetWeight_NotDestroyMsg) {
    attention_gate_set_weight(nullptr, 0, 0, 0.5f);
    VerifyExceptionFunction("attention_gate_set_weight", WRONG_DESTROY, 1);
}

TEST_F(AttentionGateErrorMsgRegressionTest, GetWeight_NotDestroyMsg) {
    attention_gate_get_weight(nullptr, 0, 0, nullptr);
    VerifyExceptionFunction("attention_gate_get_weight", WRONG_DESTROY, 1);
}

TEST_F(AttentionGateErrorMsgRegressionTest, UpdateSalience_NotDestroyMsg) {
    attention_gate_update_salience(nullptr, 0, 0.5f);
    VerifyExceptionFunction("attention_gate_update_salience", WRONG_DESTROY, 1);
}

//=============================================================================
// REGRESSION: Functions that originally said "attention_gate_apply_wta"
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, UpdateSpotlight_NotWtaMsg) {
    attention_gate_update_spotlight(nullptr, nullptr, nullptr);
    VerifyExceptionFunction("attention_gate_update_spotlight", WRONG_WTA, 1);
}

TEST_F(AttentionGateErrorMsgRegressionTest, GetShifts_NotWtaMsg) {
    attention_shift_t shifts[1];
    uint32_t num = 0;
    attention_gate_get_shifts(nullptr, shifts, 1, &num);
    VerifyExceptionFunction("attention_gate_get_shifts", WRONG_WTA, 1);
}

//=============================================================================
// REGRESSION: Functions that originally said "attention_gate_reset"
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, GetStats_NotResetMsg) {
    attention_gate_get_stats(nullptr, nullptr, nullptr, nullptr);
    VerifyExceptionFunction("attention_gate_get_stats", WRONG_RESET, 1);
}

TEST_F(AttentionGateErrorMsgRegressionTest, SetTernaryState_NotResetMsg) {
    attention_gate_set_ternary_state(nullptr, 0, 0, TRIT_POSITIVE);
    VerifyExceptionFunction("attention_gate_set_ternary_state", WRONG_RESET, 1);
}

TEST_F(AttentionGateErrorMsgRegressionTest, GetTernaryState_NotResetMsg) {
    trit_t state;
    attention_gate_get_ternary_state(nullptr, 0, 0, &state);
    VerifyExceptionFunction("attention_gate_get_ternary_state", WRONG_RESET, 1);
}

//=============================================================================
// CORRECT FUNCTION: attention_gate_apply_wta should keep its own name
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, ApplyWta_KeepsOwnName) {
    attention_gate_apply_wta(nullptr, nullptr);
    ASSERT_TRUE(captured_.captured)
        << "Expected exception for apply_wta but none was captured";
    EXPECT_TRUE(captured_.message.find("attention_gate_apply_wta") != std::string::npos)
        << "apply_wta should keep its own function name. Got: " << captured_.message;
    captured_.reset();
}

//=============================================================================
// CORRECT FUNCTION: attention_gate_create should keep its own name
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, Create_KeepsOwnName) {
    captured_.reset();
    attention_gate_t* g = attention_gate_create(nullptr);
    EXPECT_EQ(g, nullptr);

    ASSERT_TRUE(captured_.captured)
        << "Expected exception for attention_gate_create but none was captured";
    EXPECT_TRUE(captured_.message.find("attention_gate_create") != std::string::npos)
        << "create should keep its own function name. Got: " << captured_.message;
    captured_.reset();
}

//=============================================================================
// ERROR CODE CORRECTNESS - Verify correct error codes are thrown
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, NullGate_ThrowsNullPointerOrInvalidParam) {
    // NULL gate parameter should throw NULL_POINTER or INVALID_PARAM
    attention_gate_set_weight(nullptr, 0, 0, 0.5f);
    ASSERT_TRUE(captured_.captured)
        << "Expected exception for NULL gate but none was captured";
    EXPECT_TRUE(
        captured_.code == NIMCP_ERROR_NULL_POINTER ||
        captured_.code == NIMCP_ERROR_INVALID_PARAM)
        << "Expected NULL_POINTER or INVALID_PARAM, got code: " << captured_.code;
    captured_.reset();
}

TEST_F(AttentionGateErrorMsgRegressionTest, InvalidWeight_ThrowsInvalidParam) {
    // Weight > 1.0 should throw INVALID_PARAM
    attention_gate_set_weight(gate_, 0, 0, 999.0f);
    ASSERT_TRUE(captured_.captured)
        << "Expected exception for invalid weight but none was captured";
    EXPECT_EQ(captured_.code, NIMCP_ERROR_INVALID_PARAM);
    captured_.reset();
}

//=============================================================================
// FUNCTIONAL REGRESSION - Ensure the fix didn't break normal behavior
//=============================================================================

TEST_F(AttentionGateErrorMsgRegressionTest, NormalWorkflow_StillFunctions) {
    captured_.reset();

    // Set weight
    bool ok = attention_gate_set_weight(gate_, 1, 100, 0.8f);
    EXPECT_TRUE(ok);

    // Get weight
    float weight = 0.0f;
    ok = attention_gate_get_weight(gate_, 1, 100, &weight);
    EXPECT_TRUE(ok);
    EXPECT_GT(weight, 0.0f);

    // Update salience
    ok = attention_gate_update_salience(gate_, 100, 0.7f);
    EXPECT_TRUE(ok);

    // Update spotlight
    uint32_t spotlight[8];
    uint32_t spotlight_count = 0;
    ok = attention_gate_update_spotlight(gate_, spotlight, &spotlight_count);
    EXPECT_TRUE(ok);

    // Get stats
    uint32_t targets = 0, in_spotlight = 0;
    uint64_t shifts = 0;
    ok = attention_gate_get_stats(gate_, &targets, &in_spotlight, &shifts);
    EXPECT_TRUE(ok);
    EXPECT_GT(targets, 0u);

    // Get shifts
    attention_shift_t shift_buf[10];
    uint32_t num_shifts = 0;
    ok = attention_gate_get_shifts(gate_, shift_buf, 10, &num_shifts);
    EXPECT_TRUE(ok);

    // Apply WTA (needs entries)
    uint32_t winner = 0;
    ok = attention_gate_apply_wta(gate_, &winner);
    EXPECT_TRUE(ok);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

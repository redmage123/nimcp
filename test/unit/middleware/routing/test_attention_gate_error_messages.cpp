//=============================================================================
// test_attention_gate_error_messages.cpp - Verify error messages reference correct function names
//=============================================================================
//
// WHAT: Validate that NIMCP_THROW_TO_IMMUNE error messages in attention_gate.c
//       reference the actual function name, not a copy-paste "attention_gate_destroy"
// WHY:  Copy-paste error messages make debugging extremely difficult because
//       the error message points to the wrong function
// HOW:  Call each function with invalid parameters, register a custom exception
//       handler to capture the exception message during dispatch, then verify
//       the message contains the correct function name.
//
// NOTE: NIMCP_THROW_TO_IMMUNE calls nimcp_exception_dispatch() which sets
//       the thread-local exception and clears it before returning. Therefore
//       nimcp_exception_get_current() always returns NULL after the call.
//       We use a registered handler callback to capture the message instead.
//
// REGRESSION: Fixes copy-paste bug where 8+ functions all had error messages
//             saying "attention_gate_destroy" instead of their actual name
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_attention_gate.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Exception capture handler - captures the last exception message during dispatch
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

class AttentionGateErrorMessageTest : public ::testing::Test {
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

        // Register capture handler at high priority so we see the exception
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "attention_gate_error_msg_capture";
        opts.handler = capture_exception_handler;
        opts.user_data = &captured_;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);

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

    // Helper: check that the captured exception message contains the expected function name
    // and does NOT contain the wrong function name "attention_gate_destroy"
    // (unless the actual function IS attention_gate_destroy)
    void ExpectCorrectFunctionInMessage(const char* expected_func_name) {
        ASSERT_TRUE(captured_.captured)
            << "Expected an exception to be dispatched for " << expected_func_name
            << " but none was captured by the handler";

        EXPECT_TRUE(captured_.message.find(expected_func_name) != std::string::npos)
            << "Expected message to contain '" << expected_func_name
            << "' but got: " << captured_.message;

        // If the expected function is NOT attention_gate_destroy, make sure the
        // message doesn't incorrectly say "attention_gate_destroy"
        if (std::string(expected_func_name) != "attention_gate_destroy") {
            EXPECT_TRUE(captured_.message.find("attention_gate_destroy") == std::string::npos)
                << "Message incorrectly references 'attention_gate_destroy' "
                << "instead of '" << expected_func_name << "': " << captured_.message;
        }

        captured_.reset();
    }
};

//=============================================================================
// ERROR MESSAGE TESTS - Each function's error path must name itself
//=============================================================================

TEST_F(AttentionGateErrorMessageTest, SetWeight_NullGate_CorrectFuncName) {
    // Call with NULL gate to trigger error path
    bool result = attention_gate_set_weight(nullptr, 1, 1, 0.5f);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_set_weight");
}

TEST_F(AttentionGateErrorMessageTest, SetWeight_InvalidWeight_CorrectFuncName) {
    // Call with weight > 1.0 to trigger error path
    bool result = attention_gate_set_weight(gate_, 1, 1, 2.0f);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_set_weight");
}

TEST_F(AttentionGateErrorMessageTest, GetWeight_NullGate_CorrectFuncName) {
    float weight = 0.0f;
    bool result = attention_gate_get_weight(nullptr, 1, 1, &weight);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_weight");
}

TEST_F(AttentionGateErrorMessageTest, GetWeight_NullWeight_CorrectFuncName) {
    bool result = attention_gate_get_weight(gate_, 1, 1, nullptr);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_weight");
}

TEST_F(AttentionGateErrorMessageTest, UpdateSalience_NullGate_CorrectFuncName) {
    bool result = attention_gate_update_salience(nullptr, 1, 0.5f);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_update_salience");
}

TEST_F(AttentionGateErrorMessageTest, UpdateSalience_InvalidSalience_CorrectFuncName) {
    bool result = attention_gate_update_salience(gate_, 1, -0.1f);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_update_salience");
}

TEST_F(AttentionGateErrorMessageTest, UpdateSpotlight_NullGate_CorrectFuncName) {
    bool result = attention_gate_update_spotlight(nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_update_spotlight");
}

TEST_F(AttentionGateErrorMessageTest, GetShifts_NullGate_CorrectFuncName) {
    attention_shift_t shifts[10];
    uint32_t num_shifts = 0;
    bool result = attention_gate_get_shifts(nullptr, shifts, 10, &num_shifts);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_shifts");
}

TEST_F(AttentionGateErrorMessageTest, GetShifts_NullShifts_CorrectFuncName) {
    uint32_t num_shifts = 0;
    bool result = attention_gate_get_shifts(gate_, nullptr, 10, &num_shifts);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_shifts");
}

TEST_F(AttentionGateErrorMessageTest, GetStats_NullGate_CorrectFuncName) {
    uint32_t num_targets = 0;
    bool result = attention_gate_get_stats(nullptr, &num_targets, nullptr, nullptr);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_stats");
}

TEST_F(AttentionGateErrorMessageTest, SetTernaryState_NullGate_CorrectFuncName) {
    bool result = attention_gate_set_ternary_state(nullptr, 1, 1, TRIT_POSITIVE);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_set_ternary_state");
}

TEST_F(AttentionGateErrorMessageTest, GetTernaryState_NullGate_CorrectFuncName) {
    trit_t state;
    bool result = attention_gate_get_ternary_state(nullptr, 1, 1, &state);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_ternary_state");
}

TEST_F(AttentionGateErrorMessageTest, GetTernaryState_NullState_CorrectFuncName) {
    bool result = attention_gate_get_ternary_state(gate_, 1, 1, nullptr);
    EXPECT_FALSE(result);
    ExpectCorrectFunctionInMessage("attention_gate_get_ternary_state");
}

//=============================================================================
// POSITIVE TESTS - Functions should still work correctly after the fix
//=============================================================================

TEST_F(AttentionGateErrorMessageTest, SetWeight_ValidParams_Success) {
    captured_.reset();
    bool result = attention_gate_set_weight(gate_, 1, 100, 0.8f);
    EXPECT_TRUE(result);
}

TEST_F(AttentionGateErrorMessageTest, GetWeight_ValidParams_Success) {
    // First set a weight
    attention_gate_set_weight(gate_, 1, 100, 0.8f);
    captured_.reset();

    float weight = 0.0f;
    bool result = attention_gate_get_weight(gate_, 1, 100, &weight);
    EXPECT_TRUE(result);
    EXPECT_GT(weight, 0.0f);
}

TEST_F(AttentionGateErrorMessageTest, UpdateSalience_ValidParams_Success) {
    captured_.reset();
    bool result = attention_gate_update_salience(gate_, 100, 0.5f);
    EXPECT_TRUE(result);
}

TEST_F(AttentionGateErrorMessageTest, GetStats_ValidParams_Success) {
    captured_.reset();
    uint32_t num_targets = 999;
    bool result = attention_gate_get_stats(gate_, &num_targets, nullptr, nullptr);
    EXPECT_TRUE(result);
    // Freshly created gate should have 0 targets
    EXPECT_EQ(num_targets, 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

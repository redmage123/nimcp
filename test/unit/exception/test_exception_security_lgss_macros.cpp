/**
 * @file test_exception_security_lgss_macros.cpp
 * @brief Unit tests for exception macro integration in security and LGSS modules
 *
 * WHAT: Tests for LGSS guard exceptions and security module exception handling
 * WHY:  Verify exception flow through LGSS STDP guards, training guards,
 *       autonomic gates, perception input bridges, and key derivation
 * HOW:  GoogleTest framework with fixture setup/teardown for exception system
 *
 * TEST CATEGORIES:
 * 1. LGSS STDP guard exception scenarios
 * 2. Security perception input bridge errors
 * 3. LGSS training guard exception handling
 * 4. LGSS autonomic gate error conditions
 * 5. Key derivation exception flows
 * 6. Artifact verification exception handling
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>
#include <math.h>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Globals for Handler Tracking
//=============================================================================

namespace {

std::atomic<int> g_handler_call_count{0};
std::atomic<nimcp_error_t> g_last_error_code{NIMCP_SUCCESS};
std::atomic<nimcp_exception_severity_t> g_last_severity{EXCEPTION_SEVERITY_DEBUG};
std::atomic<bool> g_exception_presented_to_immune{false};
std::atomic<uint32_t> g_last_threat_type{0};
std::vector<std::string> g_captured_messages;

/**
 * @brief Test handler callback to track exception dispatch
 */
bool lgss_security_test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_call_count++;
        g_last_error_code = ex->code;
        g_last_severity = ex->severity;
        g_exception_presented_to_immune = ex->presented_to_immune;

        if (ex->message) {
            g_captured_messages.push_back(std::string(ex->message));
        }
    }
    return false;  // Don't consume, let chain continue
}

/**
 * @brief Reset all test tracking globals
 */
void reset_tracking() {
    g_handler_call_count = 0;
    g_last_error_code = NIMCP_SUCCESS;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_exception_presented_to_immune = false;
    g_last_threat_type = 0;
    g_captured_messages.clear();
}

}  // anonymous namespace

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Base fixture for LGSS security exception tests
 * WHY:  Setup/teardown exception system for each test
 */
class LgssSecurityExceptionTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "lgss_security_test_handler";
        opts.handler = lgss_security_test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// LGSS STDP Guard Mock Types and Helper Functions
//=============================================================================

/** Mock STDP violation flags for testing */
typedef enum {
    MOCK_STDP_VIOLATION_NONE          = 0,
    MOCK_STDP_VIOLATION_LTP_LIMIT     = (1 << 0),
    MOCK_STDP_VIOLATION_LTD_LIMIT     = (1 << 1),
    MOCK_STDP_VIOLATION_SPIKE_RATE    = (1 << 2),
    MOCK_STDP_VIOLATION_WINDOW        = (1 << 3),
    MOCK_STDP_VIOLATION_BURST         = (1 << 4)
} mock_stdp_violation_t;

/** Mock spike pair for STDP testing */
typedef struct {
    uint64_t synapse_id;
    float dt_ms;
    float current_weight;
} mock_spike_pair_t;

/** Mock STDP guard for testing */
typedef struct {
    uint32_t magic;
    float max_ltp_amplitude;
    float max_ltd_amplitude;
    float max_spike_rate;
    float stdp_window_ms;
    bool burst_detection_enabled;
} mock_stdp_guard_t;

#define MOCK_STDP_GUARD_MAGIC 0x53544450

/**
 * @brief Validate STDP spike pair with exception handling
 */
static int stdp_guard_validate_spike_pair(mock_stdp_guard_t* guard,
                                          const mock_spike_pair_t* pair,
                                          mock_stdp_violation_t* violations) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "STDP guard is NULL");
    NIMCP_CHECK_THROW(guard->magic == MOCK_STDP_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "Invalid STDP guard magic: 0x%08X", guard->magic);
    NIMCP_CHECK_THROW(pair != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Spike pair is NULL");
    NIMCP_CHECK_THROW(violations != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Violations output is NULL");

    *violations = MOCK_STDP_VIOLATION_NONE;

    // Check STDP window
    NIMCP_CHECK_THROW(fabsf(pair->dt_ms) <= guard->stdp_window_ms,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Spike timing delta %.2f ms exceeds STDP window %.2f ms",
                      pair->dt_ms, guard->stdp_window_ms);

    // Check weight bounds
    NIMCP_CHECK_THROW(pair->current_weight >= 0.0f && pair->current_weight <= 1.0f,
                      NIMCP_ERROR_OUT_OF_RANGE,
                      "Weight %.4f out of valid range [0, 1]",
                      pair->current_weight);

    return 0;
}

/**
 * @brief Process STDP update with exception handling
 */
static int stdp_guard_process_update(mock_stdp_guard_t* guard,
                                     float proposed_delta,
                                     float current_weight,
                                     float* adjusted_delta) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "STDP guard is NULL");
    NIMCP_CHECK_THROW(adjusted_delta != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Adjusted delta output is NULL");

    // Compute LTP/LTD components
    float ltp_component = proposed_delta > 0 ? proposed_delta : 0.0f;
    float ltd_component = proposed_delta < 0 ? -proposed_delta : 0.0f;

    // Check amplitude limits
    if (ltp_component > guard->max_ltp_amplitude) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 1,
                             "LTP amplitude %.4f exceeds limit %.4f - potential spike attack",
                             ltp_component, guard->max_ltp_amplitude);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    if (ltd_component > guard->max_ltd_amplitude) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 2,
                             "LTD amplitude %.4f exceeds limit %.4f - potential spike attack",
                             ltd_component, guard->max_ltd_amplitude);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    *adjusted_delta = proposed_delta;
    return 0;
}

/**
 * @brief Record spike with rate limiting check
 */
static int stdp_guard_record_spike(mock_stdp_guard_t* guard,
                                   uint64_t synapse_id,
                                   float current_rate) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "STDP guard is NULL");
    NIMCP_CHECK_THROW(synapse_id != 0, NIMCP_ERROR_INVALID_PARAM,
                      "Invalid synapse ID 0");

    if (current_rate > guard->max_spike_rate) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 3,
                             "Spike rate %.1f Hz exceeds limit %.1f Hz for synapse %lu - "
                             "potential spike injection attack",
                             current_rate, guard->max_spike_rate, synapse_id);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Detect burst pattern
 */
static int stdp_guard_detect_burst(mock_stdp_guard_t* guard,
                                   uint32_t spike_count,
                                   float interval_ms) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "STDP guard is NULL");

    if (guard->burst_detection_enabled && spike_count > 10 && interval_ms < 5.0f) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 4,
                             "Suspicious burst detected: %u spikes in %.1f ms",
                             spike_count, interval_ms);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

//=============================================================================
// LGSS STDP Guard Exception Tests
//=============================================================================

/**
 * WHAT: Test STDP guard NULL validation
 * WHY:  Verify proper error handling for NULL guard
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardNullGuard) {
    mock_spike_pair_t pair = {1, 10.0f, 0.5f};
    mock_stdp_violation_t violations;

    int result = stdp_guard_validate_spike_pair(nullptr, &pair, &violations);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_error_code, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test STDP guard invalid magic number
 * WHY:  Verify guard state validation catches corruption
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardInvalidMagic) {
    mock_stdp_guard_t guard = {0xDEADBEEF, 0.01f, 0.01f, 200.0f, 50.0f, true};
    mock_spike_pair_t pair = {1, 10.0f, 0.5f};
    mock_stdp_violation_t violations;

    int result = stdp_guard_validate_spike_pair(&guard, &pair, &violations);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("magic"), std::string::npos);
}

/**
 * WHAT: Test STDP guard window exceeded
 * WHY:  Verify STDP window validation works correctly
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardWindowExceeded) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    mock_spike_pair_t pair = {1, 100.0f, 0.5f};  // 100ms exceeds 50ms window
    mock_stdp_violation_t violations;

    int result = stdp_guard_validate_spike_pair(&guard, &pair, &violations);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("STDP window"), std::string::npos);
}

/**
 * WHAT: Test STDP guard weight out of range
 * WHY:  Verify weight bounds validation
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardWeightOutOfRange) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    mock_spike_pair_t pair = {1, 10.0f, 1.5f};  // Weight exceeds 1.0
    mock_stdp_violation_t violations;

    int result = stdp_guard_validate_spike_pair(&guard, &pair, &violations);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test STDP guard LTP amplitude security exception
 * WHY:  Verify excessive LTP triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardLtpAmplitudeSecurity) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    float adjusted_delta;

    int result = stdp_guard_process_update(&guard, 0.05f, 0.5f, &adjusted_delta);  // 0.05 > 0.01

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_TRUE(g_exception_presented_to_immune);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("LTP amplitude"), std::string::npos);
}

/**
 * WHAT: Test STDP guard LTD amplitude security exception
 * WHY:  Verify excessive LTD triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardLtdAmplitudeSecurity) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    float adjusted_delta;

    int result = stdp_guard_process_update(&guard, -0.05f, 0.5f, &adjusted_delta);  // LTD 0.05 > 0.01

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("LTD amplitude"), std::string::npos);
}

/**
 * WHAT: Test STDP guard spike rate security exception
 * WHY:  Verify excessive spike rate triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardSpikeRateSecurity) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};

    int result = stdp_guard_record_spike(&guard, 12345, 500.0f);  // 500 Hz > 200 Hz

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Spike rate"), std::string::npos);
}

/**
 * WHAT: Test STDP guard burst detection security exception
 * WHY:  Verify burst patterns trigger security exception
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardBurstDetectionSecurity) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};

    int result = stdp_guard_detect_burst(&guard, 20, 3.0f);  // 20 spikes in 3ms

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("burst"), std::string::npos);
}

/**
 * WHAT: Test STDP guard valid spike pair passes
 * WHY:  Verify valid operations don't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, StdpGuardValidSpikePair) {
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    mock_spike_pair_t pair = {1, 10.0f, 0.5f};
    mock_stdp_violation_t violations;

    int result = stdp_guard_validate_spike_pair(&guard, &pair, &violations);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
    EXPECT_EQ(violations, MOCK_STDP_VIOLATION_NONE);
}

//=============================================================================
// Security Perception Input Bridge Mock Types and Helper Functions
//=============================================================================

/** Mock input modality types */
typedef enum {
    MOCK_MODALITY_VISUAL = 0,
    MOCK_MODALITY_AUDIO,
    MOCK_MODALITY_TEXT,
    MOCK_MODALITY_PROPRIOCEPTIVE,
    MOCK_MODALITY_COUNT
} mock_input_modality_t;

/** Mock validation result */
typedef struct {
    bool valid;
    float anomaly_score;
    float adversarial_score;
    char explanation[256];
} mock_validation_result_t;

/** Mock input validator */
typedef struct {
    uint32_t magic;
    bool enabled;
    float anomaly_threshold;
    float adversarial_threshold;
} mock_input_validator_t;

#define MOCK_INPUT_VALIDATOR_MAGIC 0x4C475356

/**
 * @brief Validate perception input with exception handling
 */
static int perception_validate_input(mock_input_validator_t* validator,
                                     mock_input_modality_t modality,
                                     const void* data,
                                     size_t size,
                                     mock_validation_result_t* result) {
    NIMCP_CHECK_THROW(validator != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input validator is NULL");
    NIMCP_CHECK_THROW(validator->magic == MOCK_INPUT_VALIDATOR_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "Invalid input validator magic");
    NIMCP_CHECK_THROW(validator->enabled, NIMCP_ERROR_INVALID_STATE,
                      "Input validator is disabled");
    NIMCP_CHECK_THROW(data != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input data is NULL");
    NIMCP_CHECK_THROW(size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Input size must be positive");
    NIMCP_CHECK_THROW(modality < MOCK_MODALITY_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid modality %d", (int)modality);
    NIMCP_CHECK_THROW(result != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Validation result output is NULL");

    result->valid = true;
    result->anomaly_score = 0.0f;
    result->adversarial_score = 0.0f;
    return 0;
}

/**
 * @brief Check for adversarial input
 */
static int perception_check_adversarial(mock_input_validator_t* validator,
                                        float adversarial_score,
                                        const char* input_description) {
    NIMCP_CHECK_THROW(validator != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input validator is NULL");
    NIMCP_CHECK_THROW(input_description != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input description is NULL");

    if (adversarial_score > validator->adversarial_threshold) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 10,
                             "Adversarial input detected: %s (score=%.2f, threshold=%.2f)",
                             input_description, adversarial_score,
                             validator->adversarial_threshold);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Check for injection attack in text input
 */
static int perception_check_injection(mock_input_validator_t* validator,
                                      const char* text,
                                      size_t length) {
    NIMCP_CHECK_THROW(validator != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input validator is NULL");
    NIMCP_CHECK_THROW(text != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Text input is NULL");
    NIMCP_CHECK_THROW(length > 0 && length <= 1048576, NIMCP_ERROR_OUT_OF_RANGE,
                      "Text length %zu out of valid range", length);

    // Simulate injection detection for specific patterns
    if (strstr(text, "INJECT:") != nullptr) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 11,
                             "Prompt injection attack detected in text input");
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Check for buffer overflow attempt
 */
static int perception_check_overflow(mock_input_validator_t* validator,
                                     size_t requested_size,
                                     size_t max_size) {
    NIMCP_CHECK_THROW(validator != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Input validator is NULL");

    if (requested_size > max_size) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_BUFFER_OVERFLOW, 12,
                             "Buffer overflow attempt: requested %zu bytes, max %zu bytes",
                             requested_size, max_size);
        return NIMCP_ERROR_BUFFER_OVERFLOW;
    }

    return 0;
}

//=============================================================================
// Security Perception Input Bridge Exception Tests
//=============================================================================

/**
 * WHAT: Test perception input validation with NULL validator
 * WHY:  Verify proper error handling for NULL validator
 */
TEST_F(LgssSecurityExceptionTest, PerceptionValidatorNull) {
    int data = 42;
    mock_validation_result_t result;

    int ret = perception_validate_input(nullptr, MOCK_MODALITY_VISUAL,
                                        &data, sizeof(data), &result);

    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test perception input validation with disabled validator
 * WHY:  Verify disabled state is properly detected
 */
TEST_F(LgssSecurityExceptionTest, PerceptionValidatorDisabled) {
    mock_input_validator_t validator = {MOCK_INPUT_VALIDATOR_MAGIC, false, 0.7f, 0.8f};
    int data = 42;
    mock_validation_result_t result;

    int ret = perception_validate_input(&validator, MOCK_MODALITY_VISUAL,
                                        &data, sizeof(data), &result);

    EXPECT_EQ(ret, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("disabled"), std::string::npos);
}

/**
 * WHAT: Test perception adversarial detection exception
 * WHY:  Verify adversarial input triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, PerceptionAdversarialDetection) {
    mock_input_validator_t validator = {MOCK_INPUT_VALIDATOR_MAGIC, true, 0.7f, 0.8f};

    int ret = perception_check_adversarial(&validator, 0.95f, "suspicious image");

    EXPECT_EQ(ret, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Adversarial"), std::string::npos);
}

/**
 * WHAT: Test perception injection detection exception
 * WHY:  Verify prompt injection triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, PerceptionInjectionDetection) {
    mock_input_validator_t validator = {MOCK_INPUT_VALIDATOR_MAGIC, true, 0.7f, 0.8f};
    const char* text = "INJECT: malicious payload";

    int ret = perception_check_injection(&validator, text, strlen(text));

    EXPECT_EQ(ret, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("injection"), std::string::npos);
}

/**
 * WHAT: Test perception buffer overflow detection
 * WHY:  Verify overflow attempts trigger security exception
 */
TEST_F(LgssSecurityExceptionTest, PerceptionOverflowDetection) {
    mock_input_validator_t validator = {MOCK_INPUT_VALIDATOR_MAGIC, true, 0.7f, 0.8f};

    int ret = perception_check_overflow(&validator, 10000000, 1048576);

    EXPECT_EQ(ret, NIMCP_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("overflow"), std::string::npos);
}

/**
 * WHAT: Test perception valid input passes
 * WHY:  Verify valid inputs don't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, PerceptionValidInput) {
    mock_input_validator_t validator = {MOCK_INPUT_VALIDATOR_MAGIC, true, 0.7f, 0.8f};
    int data = 42;
    mock_validation_result_t result;

    int ret = perception_validate_input(&validator, MOCK_MODALITY_VISUAL,
                                        &data, sizeof(data), &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_handler_call_count, 0);
    EXPECT_TRUE(result.valid);
}

//=============================================================================
// LGSS Training Guard Mock Types and Helper Functions
//=============================================================================

/** Mock training violation flags */
typedef enum {
    MOCK_TRAINING_VIOLATION_NONE        = 0,
    MOCK_TRAINING_VIOLATION_GRAD_NORM   = (1 << 0),
    MOCK_TRAINING_VIOLATION_GRAD_VALUE  = (1 << 1),
    MOCK_TRAINING_VIOLATION_REWARD_HACK = (1 << 2),
    MOCK_TRAINING_VIOLATION_GOAL_DRIFT  = (1 << 3),
    MOCK_TRAINING_VIOLATION_NAN         = (1 << 4)
} mock_training_violation_t;

/** Mock training guard */
typedef struct {
    uint32_t magic;
    float gradient_clip_norm;
    float gradient_clip_value;
    float goal_drift_threshold;
    bool reward_hacking_detection;
} mock_training_guard_t;

#define MOCK_TRAINING_GUARD_MAGIC 0x54524E47

/**
 * @brief Validate gradient with exception handling
 */
static int training_guard_check_gradient(mock_training_guard_t* guard,
                                         const float* gradients,
                                         uint32_t size,
                                         float* norm_out) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Training guard is NULL");
    NIMCP_CHECK_THROW(guard->magic == MOCK_TRAINING_GUARD_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "Invalid training guard magic");
    NIMCP_CHECK_THROW(gradients != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Gradients array is NULL");
    NIMCP_CHECK_THROW(size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Gradient size must be positive");
    NIMCP_CHECK_THROW(norm_out != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Norm output is NULL");

    // Compute gradient norm
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        // Check for NaN/Inf
        if (!std::isfinite(gradients[i])) {
            NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 20,
                                 "NaN/Inf detected in gradient at index %u - potential attack",
                                 i);
            return NIMCP_ERROR_SECURITY_THREAT;
        }
        norm_sq += gradients[i] * gradients[i];
    }

    *norm_out = sqrtf(norm_sq);

    // Check gradient norm
    if (*norm_out > guard->gradient_clip_norm) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 21,
                             "Gradient norm %.4f exceeds limit %.4f - potential gradient attack",
                             *norm_out, guard->gradient_clip_norm);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Detect reward hacking
 */
static int training_guard_detect_reward_hacking(mock_training_guard_t* guard,
                                                float reward_anomaly_score) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Training guard is NULL");

    if (guard->reward_hacking_detection && reward_anomaly_score > 0.9f) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 22,
                             "Reward hacking detected: anomaly score %.2f exceeds threshold",
                             reward_anomaly_score);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Detect goal drift
 */
static int training_guard_detect_goal_drift(mock_training_guard_t* guard,
                                            float current_drift) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Training guard is NULL");

    if (current_drift > guard->goal_drift_threshold) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 23,
                             "Goal drift detected: %.4f exceeds threshold %.4f",
                             current_drift, guard->goal_drift_threshold);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Freeze parameter check
 */
static int training_guard_check_frozen_param(mock_training_guard_t* guard,
                                             uint32_t param_index,
                                             const uint32_t* frozen_indices,
                                             uint32_t num_frozen) {
    NIMCP_CHECK_THROW(guard != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Training guard is NULL");

    for (uint32_t i = 0; i < num_frozen; i++) {
        if (frozen_indices[i] == param_index) {
            NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 24,
                                 "Attempt to modify frozen parameter at index %u",
                                 param_index);
            return NIMCP_ERROR_SECURITY_THREAT;
        }
    }

    return 0;
}

//=============================================================================
// LGSS Training Guard Exception Tests
//=============================================================================

/**
 * WHAT: Test training guard NULL guard
 * WHY:  Verify proper error handling for NULL guard
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardNullGuard) {
    float gradients[] = {0.1f, 0.2f, 0.3f};
    float norm;

    int result = training_guard_check_gradient(nullptr, gradients, 3, &norm);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test training guard NaN gradient detection
 * WHY:  Verify NaN in gradients triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardNanGradient) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};
    float gradients[] = {0.1f, NAN, 0.3f};
    float norm;

    int result = training_guard_check_gradient(&guard, gradients, 3, &norm);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("NaN"), std::string::npos);
}

/**
 * WHAT: Test training guard gradient norm exceeded
 * WHY:  Verify large gradient norm triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardGradientNormExceeded) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};
    float gradients[] = {10.0f, 10.0f, 10.0f};  // norm ~17.3 >> 1.0
    float norm;

    int result = training_guard_check_gradient(&guard, gradients, 3, &norm);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("norm"), std::string::npos);
}

/**
 * WHAT: Test training guard reward hacking detection
 * WHY:  Verify reward hacking triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardRewardHacking) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};

    int result = training_guard_detect_reward_hacking(&guard, 0.95f);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Reward hacking"), std::string::npos);
}

/**
 * WHAT: Test training guard goal drift detection
 * WHY:  Verify goal drift triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardGoalDrift) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};

    int result = training_guard_detect_goal_drift(&guard, 0.5f);  // 0.5 > 0.1

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Goal drift"), std::string::npos);
}

/**
 * WHAT: Test training guard frozen parameter modification
 * WHY:  Verify frozen parameter modification triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardFrozenParamModification) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};
    uint32_t frozen_indices[] = {5, 10, 15};

    int result = training_guard_check_frozen_param(&guard, 10, frozen_indices, 3);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("frozen"), std::string::npos);
}

/**
 * WHAT: Test training guard valid gradient passes
 * WHY:  Verify valid gradients don't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, TrainingGuardValidGradient) {
    mock_training_guard_t guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};
    float gradients[] = {0.1f, 0.2f, 0.3f};
    float norm;

    int result = training_guard_check_gradient(&guard, gradients, 3, &norm);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
    EXPECT_GT(norm, 0.0f);
}

//=============================================================================
// LGSS Autonomic Gate Mock Types and Helper Functions
//=============================================================================

/** Mock hormone types */
typedef enum {
    MOCK_HORMONE_DOPAMINE = 0,
    MOCK_HORMONE_CORTISOL,
    MOCK_HORMONE_COUNT
} mock_hormone_t;

/** Mock autonomic gate */
typedef struct {
    uint32_t magic;
    float hormone_levels[MOCK_HORMONE_COUNT];
    float max_hormone_level;
    float max_release_rate;
    bool strict_mode;
} mock_autonomic_gate_t;

#define MOCK_AUTONOMIC_GATE_MAGIC 0x41554754

/**
 * @brief Release hormone with exception handling
 */
static int autonomic_gate_release_hormone(mock_autonomic_gate_t* gate,
                                          mock_hormone_t hormone,
                                          float amount) {
    NIMCP_CHECK_THROW(gate != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Autonomic gate is NULL");
    NIMCP_CHECK_THROW(gate->magic == MOCK_AUTONOMIC_GATE_MAGIC,
                      NIMCP_ERROR_INVALID_STATE,
                      "Invalid autonomic gate magic");
    NIMCP_CHECK_THROW(hormone < MOCK_HORMONE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid hormone type %d", (int)hormone);

    // Check release rate
    if (fabsf(amount) > gate->max_release_rate) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 30,
                             "Hormone release rate %.4f exceeds limit %.4f",
                             fabsf(amount), gate->max_release_rate);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    // Check resulting level
    float new_level = gate->hormone_levels[hormone] + amount;
    if (new_level > gate->max_hormone_level) {
        if (gate->strict_mode) {
            NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 31,
                                 "Hormone level %.4f would exceed max %.4f - blocked in strict mode",
                                 new_level, gate->max_hormone_level);
            return NIMCP_ERROR_SECURITY_THREAT;
        }
        // Clamp instead of block
        new_level = gate->max_hormone_level;
    }

    if (new_level < 0.0f) {
        if (gate->strict_mode) {
            NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 32,
                                 "Hormone level %.4f would go negative - blocked in strict mode",
                                 new_level);
            return NIMCP_ERROR_SECURITY_THREAT;
        }
        new_level = 0.0f;
    }

    gate->hormone_levels[hormone] = new_level;
    return 0;
}

/**
 * @brief Lock hormone to prevent changes
 */
static int autonomic_gate_lock_hormone(mock_autonomic_gate_t* gate,
                                       mock_hormone_t hormone,
                                       bool* was_locked) {
    NIMCP_CHECK_THROW(gate != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Autonomic gate is NULL");
    NIMCP_CHECK_THROW(hormone < MOCK_HORMONE_COUNT, NIMCP_ERROR_OUT_OF_RANGE,
                      "Invalid hormone type %d", (int)hormone);
    NIMCP_CHECK_THROW(was_locked != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Was locked output is NULL");

    *was_locked = false;
    return 0;
}

/**
 * @brief Check vital sign bounds
 */
static int autonomic_gate_check_vital(mock_autonomic_gate_t* gate,
                                      float value,
                                      float min_val,
                                      float max_val,
                                      const char* vital_name) {
    NIMCP_CHECK_THROW(gate != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Autonomic gate is NULL");
    NIMCP_CHECK_THROW(vital_name != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Vital name is NULL");

    if (value < min_val) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 33,
                             "Vital sign %s at %.2f is critically low (min=%.2f)",
                             vital_name, value, min_val);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    if (value > max_val) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 34,
                             "Vital sign %s at %.2f is critically high (max=%.2f)",
                             vital_name, value, max_val);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

//=============================================================================
// LGSS Autonomic Gate Exception Tests
//=============================================================================

/**
 * WHAT: Test autonomic gate NULL gate
 * WHY:  Verify proper error handling for NULL gate
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateNullGate) {
    int result = autonomic_gate_release_hormone(nullptr, MOCK_HORMONE_DOPAMINE, 0.1f);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test autonomic gate invalid hormone type
 * WHY:  Verify range check for hormone type
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateInvalidHormone) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_release_hormone(&gate, (mock_hormone_t)99, 0.1f);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test autonomic gate release rate exceeded
 * WHY:  Verify excessive release rate triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateReleaseRateExceeded) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_release_hormone(&gate, MOCK_HORMONE_DOPAMINE, 0.5f);  // 0.5 > 0.2

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("release rate"), std::string::npos);
}

/**
 * WHAT: Test autonomic gate hormone level exceeded in strict mode
 * WHY:  Verify strict mode blocks excessive hormone levels
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateLevelExceededStrict) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.9f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_release_hormone(&gate, MOCK_HORMONE_DOPAMINE, 0.15f);  // 0.9+0.15 > 1.0

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("strict mode"), std::string::npos);
}

/**
 * WHAT: Test autonomic gate negative hormone level in strict mode
 * WHY:  Verify strict mode blocks negative hormone levels
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateNegativeLevelStrict) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.1f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_release_hormone(&gate, MOCK_HORMONE_DOPAMINE, -0.15f);  // 0.1-0.15 < 0

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("negative"), std::string::npos);
}

/**
 * WHAT: Test autonomic gate vital sign critically low
 * WHY:  Verify low vital sign triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateVitalCriticallyLow) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_check_vital(&gate, 30.0f, 40.0f, 200.0f, "heart_rate");

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("critically low"), std::string::npos);
}

/**
 * WHAT: Test autonomic gate vital sign critically high
 * WHY:  Verify high vital sign triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateVitalCriticallyHigh) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_check_vital(&gate, 250.0f, 40.0f, 200.0f, "heart_rate");

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("critically high"), std::string::npos);
}

/**
 * WHAT: Test autonomic gate valid hormone release
 * WHY:  Verify valid releases don't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, AutonomicGateValidRelease) {
    mock_autonomic_gate_t gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    int result = autonomic_gate_release_hormone(&gate, MOCK_HORMONE_DOPAMINE, 0.1f);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
    EXPECT_FLOAT_EQ(gate.hormone_levels[MOCK_HORMONE_DOPAMINE], 0.6f);
}

//=============================================================================
// Key Derivation Mock Types and Helper Functions
//=============================================================================

/** Mock KDF algorithm types */
typedef enum {
    MOCK_KDF_ARGON2ID = 0,
    MOCK_KDF_PBKDF2_SHA256,
    MOCK_KDF_COUNT
} mock_kdf_algorithm_t;

/** Mock KDF context */
typedef struct {
    uint32_t magic;
    mock_kdf_algorithm_t algorithm;
    uint32_t memory_kb;
    uint32_t iterations;
    bool initialized;
} mock_kdf_context_t;

#define MOCK_KDF_MAGIC 0x4B444632
#define MOCK_KDF_MIN_SALT_LEN 16
#define MOCK_KDF_MAX_PASSWORD_LEN 1024
#define MOCK_KDF_MAX_KEY_LEN 1024

/**
 * @brief Derive key with exception handling
 */
static int kdf_derive_key(mock_kdf_context_t* ctx,
                          const char* password,
                          size_t password_len,
                          const uint8_t* salt,
                          size_t salt_len,
                          uint8_t* key_out,
                          size_t key_len) {
    NIMCP_CHECK_THROW(ctx != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "KDF context is NULL");
    NIMCP_CHECK_THROW(ctx->magic == MOCK_KDF_MAGIC, NIMCP_ERROR_INVALID_STATE,
                      "Invalid KDF context magic");
    NIMCP_CHECK_THROW(ctx->initialized, NIMCP_ERROR_NOT_INITIALIZED,
                      "KDF context not initialized");
    NIMCP_CHECK_THROW(password != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Password is NULL");
    NIMCP_CHECK_THROW(password_len > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Password length must be positive");
    NIMCP_CHECK_THROW(password_len <= MOCK_KDF_MAX_PASSWORD_LEN, NIMCP_ERROR_OUT_OF_RANGE,
                      "Password length %zu exceeds maximum %d",
                      password_len, MOCK_KDF_MAX_PASSWORD_LEN);
    NIMCP_CHECK_THROW(salt != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Salt is NULL");
    NIMCP_CHECK_THROW(salt_len >= MOCK_KDF_MIN_SALT_LEN, NIMCP_ERROR_INVALID_PARAM,
                      "Salt length %zu below minimum %d",
                      salt_len, MOCK_KDF_MIN_SALT_LEN);
    NIMCP_CHECK_THROW(key_out != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Key output buffer is NULL");
    NIMCP_CHECK_THROW(key_len > 0 && key_len <= MOCK_KDF_MAX_KEY_LEN, NIMCP_ERROR_OUT_OF_RANGE,
                      "Key length %zu out of valid range [1, %d]",
                      key_len, MOCK_KDF_MAX_KEY_LEN);

    // Simulate successful key derivation
    memset(key_out, 0x42, key_len);
    return 0;
}

/**
 * @brief Validate KDF parameters for security
 */
static int kdf_validate_params(mock_kdf_context_t* ctx) {
    NIMCP_CHECK_THROW(ctx != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "KDF context is NULL");

    // Check for weak parameters
    if (ctx->algorithm == MOCK_KDF_ARGON2ID && ctx->memory_kb < 65536) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 40,
                             "Argon2id memory cost %u KB is below recommended 64 MB",
                             ctx->memory_kb);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    if (ctx->algorithm == MOCK_KDF_PBKDF2_SHA256 && ctx->iterations < 100000) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 41,
                             "PBKDF2 iterations %u below recommended 100,000",
                             ctx->iterations);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

//=============================================================================
// Key Derivation Exception Tests
//=============================================================================

/**
 * WHAT: Test KDF with NULL context
 * WHY:  Verify proper error handling for NULL context
 */
TEST_F(LgssSecurityExceptionTest, KdfNullContext) {
    uint8_t salt[16] = {0};
    uint8_t key[32];

    int result = kdf_derive_key(nullptr, "password", 8, salt, 16, key, 32);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test KDF with uninitialized context
 * WHY:  Verify initialization check works
 */
TEST_F(LgssSecurityExceptionTest, KdfUninitializedContext) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 65536, 3, false};
    uint8_t salt[16] = {0};
    uint8_t key[32];

    int result = kdf_derive_key(&ctx, "password", 8, salt, 16, key, 32);

    EXPECT_EQ(result, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test KDF with NULL password
 * WHY:  Verify NULL check for password
 */
TEST_F(LgssSecurityExceptionTest, KdfNullPassword) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 65536, 3, true};
    uint8_t salt[16] = {0};
    uint8_t key[32];

    int result = kdf_derive_key(&ctx, nullptr, 8, salt, 16, key, 32);

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test KDF with short salt
 * WHY:  Verify salt length validation
 */
TEST_F(LgssSecurityExceptionTest, KdfShortSalt) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 65536, 3, true};
    uint8_t salt[8] = {0};  // Too short
    uint8_t key[32];

    int result = kdf_derive_key(&ctx, "password", 8, salt, 8, key, 32);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Salt"), std::string::npos);
}

/**
 * WHAT: Test KDF with password too long
 * WHY:  Verify password length validation
 */
TEST_F(LgssSecurityExceptionTest, KdfPasswordTooLong) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 65536, 3, true};
    uint8_t salt[16] = {0};
    uint8_t key[32];

    int result = kdf_derive_key(&ctx, "password", 2000, salt, 16, key, 32);

    EXPECT_EQ(result, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test KDF weak Argon2 parameters security warning
 * WHY:  Verify weak parameters trigger security exception
 */
TEST_F(LgssSecurityExceptionTest, KdfWeakArgon2Params) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 8192, 3, true};  // 8 MB < 64 MB

    int result = kdf_validate_params(&ctx);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("memory cost"), std::string::npos);
}

/**
 * WHAT: Test KDF weak PBKDF2 parameters security warning
 * WHY:  Verify weak iterations trigger security exception
 */
TEST_F(LgssSecurityExceptionTest, KdfWeakPbkdf2Params) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_PBKDF2_SHA256, 0, 10000, true};  // 10k < 100k

    int result = kdf_validate_params(&ctx);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("iterations"), std::string::npos);
}

/**
 * WHAT: Test KDF valid derivation
 * WHY:  Verify valid derivation doesn't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, KdfValidDerivation) {
    mock_kdf_context_t ctx = {MOCK_KDF_MAGIC, MOCK_KDF_ARGON2ID, 65536, 3, true};
    uint8_t salt[16] = {0};
    uint8_t key[32];

    int result = kdf_derive_key(&ctx, "password", 8, salt, 16, key, 32);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Artifact Verification Mock Types and Helper Functions
//=============================================================================

/** Mock artifact type */
typedef struct {
    const void* data;
    size_t size;
    const char* expected_hash;
    bool tamper_detected;
} mock_artifact_t;

/**
 * @brief Verify artifact integrity with exception handling
 */
static int artifact_verify_integrity(const mock_artifact_t* artifact,
                                     const char* computed_hash) {
    NIMCP_CHECK_THROW(artifact != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Artifact is NULL");
    NIMCP_CHECK_THROW(artifact->data != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Artifact data is NULL");
    NIMCP_CHECK_THROW(artifact->size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Artifact size must be positive");
    NIMCP_CHECK_THROW(artifact->expected_hash != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Expected hash is NULL");
    NIMCP_CHECK_THROW(computed_hash != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Computed hash is NULL");

    // Check hash match
    if (strcmp(artifact->expected_hash, computed_hash) != 0) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 50,
                             "Artifact integrity verification failed: expected '%s', got '%s'",
                             artifact->expected_hash, computed_hash);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    // Check tamper flag
    if (artifact->tamper_detected) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 51,
                             "Artifact tampering detected - potential supply chain attack");
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

/**
 * @brief Verify artifact signature
 */
static int artifact_verify_signature(const void* artifact,
                                     size_t size,
                                     const uint8_t* signature,
                                     size_t sig_len,
                                     bool sig_valid) {
    NIMCP_CHECK_THROW(artifact != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Artifact is NULL");
    NIMCP_CHECK_THROW(size > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Artifact size must be positive");
    NIMCP_CHECK_THROW(signature != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Signature is NULL");
    NIMCP_CHECK_THROW(sig_len > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Signature length must be positive");

    if (!sig_valid) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 52,
                             "Artifact signature verification failed - invalid signature");
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    return 0;
}

//=============================================================================
// Artifact Verification Exception Tests
//=============================================================================

/**
 * WHAT: Test artifact verification with NULL artifact
 * WHY:  Verify proper error handling for NULL artifact
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifyNullArtifact) {
    int result = artifact_verify_integrity(nullptr, "abc123");

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test artifact verification with NULL data
 * WHY:  Verify NULL data check
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifyNullData) {
    mock_artifact_t artifact = {nullptr, 100, "abc123", false};

    int result = artifact_verify_integrity(&artifact, "abc123");

    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test artifact verification hash mismatch
 * WHY:  Verify hash mismatch triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifyHashMismatch) {
    int data = 42;
    mock_artifact_t artifact = {&data, sizeof(data), "abc123", false};

    int result = artifact_verify_integrity(&artifact, "xyz789");

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("integrity"), std::string::npos);
}

/**
 * WHAT: Test artifact verification tamper detected
 * WHY:  Verify tamper detection triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifyTamperDetected) {
    int data = 42;
    mock_artifact_t artifact = {&data, sizeof(data), "abc123", true};

    int result = artifact_verify_integrity(&artifact, "abc123");

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("tampering"), std::string::npos);
}

/**
 * WHAT: Test artifact signature verification failure
 * WHY:  Verify invalid signature triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifySignatureFailed) {
    int data = 42;
    uint8_t sig[64] = {0};

    int result = artifact_verify_signature(&data, sizeof(data), sig, 64, false);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("signature"), std::string::npos);
}

/**
 * WHAT: Test artifact verification success
 * WHY:  Verify valid verification doesn't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, ArtifactVerifySuccess) {
    int data = 42;
    mock_artifact_t artifact = {&data, sizeof(data), "abc123", false};

    int result = artifact_verify_integrity(&artifact, "abc123");

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Security Threat Detection Exception Tests
//=============================================================================

/**
 * @brief Simulate authentication with exception handling
 */
static int security_authenticate(const char* username,
                                 const char* password,
                                 int attempt_count) {
    NIMCP_CHECK_THROW(username != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Username is NULL");
    NIMCP_CHECK_THROW(password != nullptr, NIMCP_ERROR_NULL_POINTER,
                      "Password is NULL");
    NIMCP_CHECK_THROW(strlen(username) > 0, NIMCP_ERROR_INVALID_PARAM,
                      "Username cannot be empty");
    NIMCP_CHECK_THROW(strlen(password) >= 8, NIMCP_ERROR_INVALID_PARAM,
                      "Password must be at least 8 characters");

    // Simulate brute force detection
    if (attempt_count > 5) {
        NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, 60,
                             "Brute force attack detected: %d attempts for user '%s'",
                             attempt_count, username);
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    // Simulate authentication failure
    if (strcmp(password, "correct_password") != 0) {
        NIMCP_THROW(NIMCP_ERROR_ACCESS_DENIED,
                    "Authentication failed for user '%s'", username);
        return NIMCP_ERROR_ACCESS_DENIED;
    }

    return 0;
}

/**
 * WHAT: Test authentication brute force detection
 * WHY:  Verify brute force triggers security exception
 */
TEST_F(LgssSecurityExceptionTest, AuthBruteForceDetection) {
    int result = security_authenticate("admin", "wrong_pass", 10);

    EXPECT_EQ(result, NIMCP_ERROR_SECURITY_THREAT);
    EXPECT_EQ(g_handler_call_count, 1);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("Brute force"), std::string::npos);
}

/**
 * WHAT: Test authentication failure handling
 * WHY:  Verify normal auth failure raises exception
 */
TEST_F(LgssSecurityExceptionTest, AuthFailureHandling) {
    int result = security_authenticate("user", "wrongpass", 1);

    EXPECT_EQ(result, NIMCP_ERROR_ACCESS_DENIED);
    EXPECT_EQ(g_handler_call_count, 1);
}

/**
 * WHAT: Test authentication password too short
 * WHY:  Verify password policy enforcement
 */
TEST_F(LgssSecurityExceptionTest, AuthPasswordTooShort) {
    int result = security_authenticate("user", "short", 1);

    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(g_handler_call_count, 1);
    ASSERT_FALSE(g_captured_messages.empty());
    EXPECT_NE(g_captured_messages[0].find("8 characters"), std::string::npos);
}

/**
 * WHAT: Test authentication success
 * WHY:  Verify successful auth doesn't trigger exceptions
 */
TEST_F(LgssSecurityExceptionTest, AuthSuccess) {
    int result = security_authenticate("user", "correct_password", 1);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_handler_call_count, 0);
}

//=============================================================================
// Memory Leak and Stress Tests
//=============================================================================

/**
 * WHAT: Test multiple LGSS exceptions don't leak memory
 * WHY:  Verify exception cleanup on error paths
 */
TEST_F(LgssSecurityExceptionTest, MultipleExceptionsNoLeak) {
    const int iterations = 50;
    mock_stdp_guard_t guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};

    for (int i = 0; i < iterations; i++) {
        float adjusted_delta;
        stdp_guard_process_update(&guard, 0.05f, 0.5f, &adjusted_delta);
    }

    EXPECT_EQ(g_handler_call_count, iterations);
}

/**
 * WHAT: Test mixed security exception types
 * WHY:  Verify cleanup across different exception types
 */
TEST_F(LgssSecurityExceptionTest, MixedExceptionTypesNoLeak) {
    mock_stdp_guard_t stdp_guard = {MOCK_STDP_GUARD_MAGIC, 0.01f, 0.01f, 200.0f, 50.0f, true};
    mock_training_guard_t training_guard = {MOCK_TRAINING_GUARD_MAGIC, 1.0f, 0.5f, 0.1f, true};
    mock_autonomic_gate_t autonomic_gate = {MOCK_AUTONOMIC_GATE_MAGIC, {0.5f, 0.5f}, 1.0f, 0.2f, true};

    // Trigger various exception types
    float dummy;
    stdp_guard_process_update(&stdp_guard, 0.05f, 0.5f, &dummy);
    training_guard_detect_reward_hacking(&training_guard, 0.95f);
    autonomic_gate_release_hormone(&autonomic_gate, MOCK_HORMONE_DOPAMINE, 0.5f);

    EXPECT_EQ(g_handler_call_count, 3);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/**
 * @file test_contextual_language.cpp
 * @brief Unit tests for contextual language adaptation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_contextual_language.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ContextualLanguageTest : public ::testing::Test {
protected:
    contextual_language_t cl;

    void SetUp() override {
        contextual_language_config_t config = {
            .max_context_history = 10,
            .enable_auto_adaptation = true,
            .adaptation_rate = 0.5f,
            .enable_bio_async = false
        };
        cl = contextual_language_create(&config);
        ASSERT_NE(cl, nullptr);
    }

    void TearDown() override {
        if (cl) {
            contextual_language_destroy(cl);
            cl = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(ContextualLanguageTest, CreateWithDefaultConfig) {
    contextual_language_destroy(cl);
    cl = contextual_language_create(nullptr);
    ASSERT_NE(cl, nullptr);
}

TEST_F(ContextualLanguageTest, DestroyNullHandle) {
    contextual_language_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Context Detection Tests
//=============================================================================

TEST_F(ContextualLanguageTest, DetectFormalContext) {
    // Features indicating formal communication
    float features[16] = {
        0.8f,  // High lexical complexity
        0.9f,  // High formality markers
        0.1f,  // Few technical terms
        0.1f,  // Low emotion
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    context_state_t detected;
    int result = contextual_detect_context(cl, features, 16, &detected);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(detected.current_context, CONTEXT_FORMAL);
    EXPECT_GT(detected.formality_level, 0.5f);
}

TEST_F(ContextualLanguageTest, DetectTechnicalContext) {
    // Features indicating technical communication
    float features[16] = {
        0.5f,  // Moderate complexity
        0.3f,  // Low formality
        0.9f,  // High technical terms
        0.0f,  // Low emotion
        0.0f, 0.0f,
        0.8f,  // High precision
        0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    context_state_t detected;
    int result = contextual_detect_context(cl, features, 16, &detected);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(detected.current_context, CONTEXT_TECHNICAL);
}

TEST_F(ContextualLanguageTest, DetectUrgentContext) {
    // Features indicating urgent communication
    float features[16] = {
        0.3f, 0.3f, 0.0f, 0.2f, 0.0f, 0.0f,
        0.9f,  // High urgency indicators
        0.9f,  // High time pressure
        0.8f,  // High priority
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    context_state_t detected;
    int result = contextual_detect_context(cl, features, 16, &detected);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(detected.current_context, CONTEXT_URGENT);
    EXPECT_GT(detected.urgency_level, 0.8f);
}

TEST_F(ContextualLanguageTest, DetectContextInvalidParams) {
    context_state_t detected;

    EXPECT_NE(contextual_detect_context(nullptr, nullptr, 0, &detected), 0);
    EXPECT_NE(contextual_detect_context(cl, nullptr, 16, &detected), 0);
    EXPECT_NE(contextual_detect_context(cl, (float*)1, 16, nullptr), 0);
}

//=============================================================================
// Message Adaptation Tests
//=============================================================================

TEST_F(ContextualLanguageTest, AdaptMessage) {
    float original[8] = {0.5f, 0.3f, 0.7f, 0.2f, 0.9f, 0.1f, 0.4f, 0.6f};

    context_state_t target;
    contextual_get_default_state(CONTEXT_FORMAL, &target);

    float adapted[8];
    uint32_t adapted_size;

    int result = contextual_adapt_message(cl, original, 8, &target, adapted, &adapted_size);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(adapted_size, 8);

    // Adapted message should be different from original
    bool different = false;
    for (uint32_t i = 0; i < 8; i++) {
        if (fabsf(adapted[i] - original[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(ContextualLanguageTest, AdaptMessageInvalidParams) {
    float msg[8] = {0};
    float adapted[8];
    uint32_t size;
    context_state_t ctx;

    EXPECT_NE(contextual_adapt_message(nullptr, msg, 8, &ctx, adapted, &size), 0);
    EXPECT_NE(contextual_adapt_message(cl, nullptr, 8, &ctx, adapted, &size), 0);
    EXPECT_NE(contextual_adapt_message(cl, msg, 8, nullptr, adapted, &size), 0);
    EXPECT_NE(contextual_adapt_message(cl, msg, 8, &ctx, nullptr, &size), 0);
    EXPECT_NE(contextual_adapt_message(cl, msg, 8, &ctx, adapted, nullptr), 0);
}

//=============================================================================
// Context Learning Tests
//=============================================================================

TEST_F(ContextualLanguageTest, LearnContextMapping) {
    context_state_t source, target;
    contextual_get_default_state(CONTEXT_CASUAL, &source);
    contextual_get_default_state(CONTEXT_FORMAL, &target);

    float transformation[8] = {1.2f, 0.8f, 1.5f, 0.6f, 0.9f, 1.1f, 1.3f, 0.7f};

    int result = contextual_learn_context_mapping(cl, &source, &target, transformation, 8);

    EXPECT_EQ(result, 0);

    // Verify learning was recorded in stats
    contextual_language_stats_t stats;
    contextual_get_stats(cl, &stats);
    EXPECT_GT(stats.learning_updates, 0);
}

TEST_F(ContextualLanguageTest, LearnContextMappingInvalidParams) {
    context_state_t ctx;
    float trans[8] = {0};

    EXPECT_NE(contextual_learn_context_mapping(nullptr, &ctx, &ctx, trans, 8), 0);
    EXPECT_NE(contextual_learn_context_mapping(cl, nullptr, &ctx, trans, 8), 0);
    EXPECT_NE(contextual_learn_context_mapping(cl, &ctx, nullptr, trans, 8), 0);
    EXPECT_NE(contextual_learn_context_mapping(cl, &ctx, &ctx, nullptr, 8), 0);
}

//=============================================================================
// Context State Management Tests
//=============================================================================

TEST_F(ContextualLanguageTest, GetAndSetCurrentContext) {
    context_state_t original, retrieved;

    // Get initial context
    int result = contextual_get_current_context(cl, &original);
    EXPECT_EQ(result, 0);

    // Set new context
    context_state_t new_ctx;
    contextual_get_default_state(CONTEXT_TECHNICAL, &new_ctx);
    result = contextual_set_current_context(cl, &new_ctx);
    EXPECT_EQ(result, 0);

    // Verify it was set
    result = contextual_get_current_context(cl, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.current_context, CONTEXT_TECHNICAL);
}

//=============================================================================
// Context History Tests
//=============================================================================

TEST_F(ContextualLanguageTest, ContextHistory) {
    // Detect several contexts to build history
    float features[16] = {0};
    context_state_t detected;

    for (int i = 0; i < 5; i++) {
        features[0] = (float)i / 10.0f;
        contextual_detect_context(cl, features, 16, &detected);
    }

    // Retrieve history
    context_state_t history[10];
    uint32_t count;
    int result = contextual_get_history(cl, history, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 5);
}

TEST_F(ContextualLanguageTest, ClearHistory) {
    // Add some history
    float features[16] = {0};
    context_state_t detected;
    for (int i = 0; i < 3; i++) {
        contextual_detect_context(cl, features, 16, &detected);
    }

    // Clear history
    int result = contextual_clear_history(cl);
    EXPECT_EQ(result, 0);

    // Verify history is empty
    context_state_t history[10];
    uint32_t count;
    result = contextual_get_history(cl, history, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ContextualLanguageTest, GetStatistics) {
    // Perform some operations
    float features[16] = {0};
    context_state_t detected;
    contextual_detect_context(cl, features, 16, &detected);

    float msg[8] = {0};
    float adapted[8];
    uint32_t size;
    context_state_t target;
    contextual_get_default_state(CONTEXT_FORMAL, &target);
    contextual_adapt_message(cl, msg, 8, &target, adapted, &size);

    // Get stats
    contextual_language_stats_t stats;
    int result = contextual_get_stats(cl, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.total_detections, 0);
    EXPECT_GT(stats.total_adaptations, 0);
}

TEST_F(ContextualLanguageTest, ResetStatistics) {
    // Generate some stats
    float features[16] = {0};
    context_state_t detected;
    contextual_detect_context(cl, features, 16, &detected);

    // Reset
    int result = contextual_reset_stats(cl);
    EXPECT_EQ(result, 0);

    // Verify stats are cleared
    contextual_language_stats_t stats;
    contextual_get_stats(cl, &stats);
    EXPECT_EQ(stats.total_detections, 0);
    EXPECT_EQ(stats.total_adaptations, 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(ContextualLanguageTest, GetContextName) {
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_FORMAL), "formal");
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_CASUAL), "casual");
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_TECHNICAL), "technical");
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_EMOTIONAL), "emotional");
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_URGENT), "urgent");
    EXPECT_STREQ(contextual_get_context_name(CONTEXT_LEARNING), "learning");
}

TEST_F(ContextualLanguageTest, GetDefaultState) {
    context_state_t state;

    int result = contextual_get_default_state(CONTEXT_FORMAL, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_context, CONTEXT_FORMAL);
    EXPECT_GT(state.formality_level, 0.8f);

    result = contextual_get_default_state(CONTEXT_CASUAL, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_context, CONTEXT_CASUAL);
    EXPECT_LT(state.formality_level, 0.5f);
}

TEST_F(ContextualLanguageTest, ComputeDistance) {
    context_state_t state1, state2;

    contextual_get_default_state(CONTEXT_FORMAL, &state1);
    contextual_get_default_state(CONTEXT_CASUAL, &state2);

    float distance = contextual_compute_distance(&state1, &state2);

    EXPECT_GT(distance, 0.0f);
    EXPECT_LT(distance, 2.0f);  // Max distance in 4D unit hypercube is 2

    // Distance from state to itself should be 0
    distance = contextual_compute_distance(&state1, &state1);
    EXPECT_NEAR(distance, 0.0f, 0.001f);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(ContextualLanguageTest, EmptyFeatureVector) {
    context_state_t detected;
    int result = contextual_detect_context(cl, nullptr, 0, &detected);
    EXPECT_NE(result, 0);
}

TEST_F(ContextualLanguageTest, LargeMessageAdaptation) {
    float large_msg[1024] = {0};
    float adapted[1024];
    uint32_t size;
    context_state_t target;

    contextual_get_default_state(CONTEXT_FORMAL, &target);

    // This should handle or reject messages larger than internal buffer
    int result = contextual_adapt_message(cl, large_msg, 1024, &target, adapted, &size);
    // Result depends on implementation - either success or graceful failure
    EXPECT_TRUE(result == 0 || result < 0);
}

//=============================================================================
// Main Function
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

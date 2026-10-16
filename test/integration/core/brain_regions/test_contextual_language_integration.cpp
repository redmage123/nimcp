/**
 * @file test_contextual_language_integration.cpp
 * @brief Integration tests for contextual language with brain regions
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_contextual_language.h"

class ContextualLanguageIntegrationTest : public ::testing::Test {
protected:
    contextual_language_t cl;

    void SetUp() override {
        cl = contextual_language_create(nullptr);
        ASSERT_NE(cl, nullptr);
    }

    void TearDown() override {
        contextual_language_destroy(cl);
    }
};

TEST_F(ContextualLanguageIntegrationTest, MultiContextWorkflow) {
    // Test complete workflow through multiple contexts
    float features[16] = {0.8f, 0.9f, 0.1f, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    context_state_t detected;

    EXPECT_EQ(contextual_detect_context(cl, features, 16, &detected), 0);

    float message[8] = {0.5f, 0.3f, 0.7f, 0.2f, 0.9f, 0.1f, 0.4f, 0.6f};
    float adapted[8];
    uint32_t adapted_size;

    context_state_t target;
    contextual_get_default_state(CONTEXT_TECHNICAL, &target);
    EXPECT_EQ(contextual_adapt_message(cl, message, 8, &target, adapted, &adapted_size), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

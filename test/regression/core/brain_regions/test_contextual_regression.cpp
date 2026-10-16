/**
 * @file test_contextual_regression.cpp
 * @brief Regression tests for contextual language system
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_contextual_language.h"

TEST(ContextualLanguageRegression, ConsistentContextDetection) {
    // Ensure context detection is deterministic
    contextual_language_t cl = contextual_language_create(nullptr);
    ASSERT_NE(cl, nullptr);

    float features[16] = {0.8f, 0.9f, 0.1f, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    context_state_t detected1, detected2;

    contextual_detect_context(cl, features, 16, &detected1);
    contextual_detect_context(cl, features, 16, &detected2);

    EXPECT_EQ(detected1.current_context, detected2.current_context);

    contextual_language_destroy(cl);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

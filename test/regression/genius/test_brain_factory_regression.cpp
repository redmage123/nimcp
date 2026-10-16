/**
 * @file test_brain_factory_regression.cpp
 * @brief Regression tests for genius profiles brain factory integration
 *
 * @author NIMCP Development Team
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>
#include <atomic>

extern "C" {
#include "core/brain/genius/nimcp_genius_profiles.h"
#include "core/brain/genius/nimcp_genius_types.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainFactoryRegressionTest : public ::testing::Test {
protected:
    genius_profiles_bridge_t* bridge = nullptr;
    genius_profiles_config_t config;

    void SetUp() override {
        ASSERT_EQ(genius_profiles_config_default(&config), GENIUS_ERROR_SUCCESS);
        config.enable_bio_async = false;
        config.enable_mesh_coordination = false;
        config.enable_training_integration = false;
        config.enable_rcog_integration = false;
        config.enable_ccog_integration = false;
        config.enable_quantum_optimization = false;
        config.enable_kg_wiring = false;

        bridge = genius_profiles_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            genius_profiles_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// 1. BRAIN CREATION REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, PolymathBrainCreationRegression) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_POLYMATH, true);
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryRegressionTest, EnumBoundaryRegression) {
    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_COUNT, true);
    EXPECT_EQ(brain, nullptr);

    brain = genius_brain_create_ex((genius_type_t)(GENIUS_TYPE_COUNT - 1), true);
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

TEST_F(BrainFactoryRegressionTest, NegativeTypeRegression) {
    brain_t brain = genius_brain_create_ex((genius_type_t)-1, true);
    EXPECT_EQ(brain, nullptr);

    brain = genius_brain_create_ex((genius_type_t)-100, true);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(BrainFactoryRegressionTest, RapidCreationDestructionRegression) {
    for (int i = 0; i < 2; i++) {  // Reduced from 10 to avoid OOM
        brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

//=============================================================================
// 2. MEMORY LEAK REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, MultipleBrainMemoryLeakRegression) {
    for (int i = 0; i < 2; i++) {  // Reduced from 3 to avoid OOM
        brain_t brains[4] = {nullptr};

        brains[0] = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
        brains[1] = genius_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
        brains[2] = genius_brain_create_ex(GENIUS_TYPE_MUSICAL, true);
        brains[3] = genius_brain_create_ex(GENIUS_TYPE_SCIENTIFIC, true);

        for (int j = 0; j < 4; j++) {
            if (brains[j]) {
                brain_destroy(brains[j]);
            }
        }
    }
}

TEST_F(BrainFactoryRegressionTest, HemisphericMemoryLeakRegression) {
    for (int i = 0; i < 2; i++) {  // Reduced from 5 to avoid OOM
        hemispheric_brain_t* hemi = genius_hemispheric_brain_create_ex(GENIUS_TYPE_VISUAL_ARTISTIC, true);
        ASSERT_NE(hemi, nullptr);
        hemispheric_brain_destroy(hemi);
    }
}

//=============================================================================
// 3. STATE CONSISTENCY REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, StateConsistencyRegression) {
    genius_error_t err = genius_profiles_activate(bridge, GENIUS_TYPE_MATHEMATICAL, 1.0f);
    EXPECT_EQ(err, GENIUS_ERROR_SUCCESS);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    brain_t brain = genius_brain_create_ex(GENIUS_TYPE_MATHEMATICAL, true);
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);

    brain_destroy(brain);
    EXPECT_EQ(genius_profiles_get_state(bridge), GENIUS_STATE_ACTIVE);
}

TEST_F(BrainFactoryRegressionTest, MultipleActivationRegression) {
    for (int i = 0; i < 20; i++) {
        genius_type_t type = (genius_type_t)(i % GENIUS_TYPE_POLYMATH);

        genius_error_t err = genius_profiles_activate(bridge, type, 1.0f);
        // May hit max profiles capacity after enough activations
        if (err != GENIUS_ERROR_SUCCESS) {
            EXPECT_EQ(err, GENIUS_ERROR_ALREADY_ACTIVE);
            break;
        }

        genius_activation_state_t state = genius_profiles_get_state(bridge);
        // First activation -> ACTIVE, subsequent -> BLENDED (multiple profiles)
        if (i == 0) {
            EXPECT_EQ(state, GENIUS_STATE_ACTIVE);
        } else {
            EXPECT_EQ(state, GENIUS_STATE_BLENDED);
        }
    }
}

//=============================================================================
// 4. CONCURRENT ACCESS REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, ConcurrentBrainCreationRegression) {
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto create_task = [&](genius_type_t type) {
        brain_t brain = genius_brain_create_ex(type, true);
        if (brain != nullptr) {
            success_count++;
            brain_destroy(brain);
        } else {
            failure_count++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 2; i++) {  // Reduced from 4 to avoid OOM
        genius_type_t type = (genius_type_t)(i % 4);
        threads.emplace_back(create_task, type);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 2);
    EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(BrainFactoryRegressionTest, ConcurrentActivationRegression) {
    std::atomic<int> completed{0};

    auto activate_task = [&](genius_type_t type) {
        for (int i = 0; i < 5; i++) {
            genius_profiles_activate(bridge, type, 1.0f);
        }
        completed++;
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 2; i++) {  // Reduced from 4 to avoid OOM
        genius_type_t type = (genius_type_t)(i % 4);
        threads.emplace_back(activate_task, type);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), 2);
}

//=============================================================================
// 5. TYPE VALIDATION REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, TypeValidationRegression) {
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_MATHEMATICAL));
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_VISUAL_ARTISTIC));
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_MUSICAL));
    EXPECT_TRUE(genius_type_is_valid(GENIUS_TYPE_SCIENTIFIC));

    EXPECT_FALSE(genius_type_is_valid((genius_type_t)-1));
    EXPECT_FALSE(genius_type_is_valid(GENIUS_TYPE_COUNT));
    EXPECT_FALSE(genius_type_is_valid((genius_type_t)999));
}

TEST_F(BrainFactoryRegressionTest, TypeNameRegression) {
    for (int i = 0; i < GENIUS_TYPE_POLYMATH; i++) {
        const char* name = genius_type_name((genius_type_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    const char* invalid_name = genius_type_name((genius_type_t)999);
    EXPECT_NE(invalid_name, nullptr);
}

//=============================================================================
// 6. HEMISPHERIC BRAIN REGRESSION TESTS
//=============================================================================

TEST_F(BrainFactoryRegressionTest, AllHemisphericTypesRegression) {
    // Reduced from 5 types to 2 to avoid OOM (each hemispheric brain is ~500MB+)
    genius_type_t types[] = {
        GENIUS_TYPE_MATHEMATICAL,
        GENIUS_TYPE_VISUAL_ARTISTIC
    };

    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        hemispheric_brain_t* hemi = genius_hemispheric_brain_create_ex(types[i], true);
        EXPECT_NE(hemi, nullptr);
        if (hemi) {
            hemispheric_brain_destroy(hemi);
        }
    }
}

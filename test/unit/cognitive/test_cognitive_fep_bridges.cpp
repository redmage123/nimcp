/**
 * @file test_cognitive_fep_bridges.cpp
 * @brief Unit tests for Cognitive-FEP Bridge modules
 *
 * WHAT: Comprehensive tests for cognitive-FEP bidirectional integrations
 * WHY:  Ensure cognitive systems integrate with FEP predictions and precision
 * HOW:  Test lifecycle, effects, and bio-async for each bridge type
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/consolidation/nimcp_consolidation_fep_bridge.h"
#include "cognitive/nimcp_hierarchical_fep_bridge.h"
#include "cognitive/nimcp_meta_learning_fep_bridge.h"
#include "cognitive/nimcp_personality_fep_bridge.h"
#include "cognitive/wellbeing/nimcp_wellbeing_fep_bridge.h"
#include "cognitive/sleep_wake/nimcp_sleep_wake_fep_bridge.h"
#include "cognitive/immune/nimcp_brain_immune_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"

class CognitiveFepBridgesTestBase : public ::testing::Test {
protected:
    fep_system_t* fep = nullptr;

    void SetUp() override {
        bio_router_init(NULL);
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        bio_router_shutdown();
    }
};

/* ============================================================================
 * Consolidation FEP Bridge Tests
 * ============================================================================ */

class ConsolidationFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    consolidation_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        consolidation_fep_config_t config;
        consolidation_fep_bridge_default_config(&config);
        bridge = consolidation_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            consolidation_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(ConsolidationFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ConsolidationFepBridgeTest, DefaultConfig) {
    consolidation_fep_config_t config;
    int ret = consolidation_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.fe_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_complexity_guided_consolidation);
}

TEST_F(ConsolidationFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(consolidation_fep_bridge_default_config(nullptr), 0);
}

TEST_F(ConsolidationFepBridgeTest, DestroyNull) {
    consolidation_fep_bridge_destroy(nullptr);
}

TEST_F(ConsolidationFepBridgeTest, ConnectFep) {
    int ret = consolidation_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, Update) {
    consolidation_fep_bridge_connect_fep(bridge, fep);
    int ret = consolidation_fep_bridge_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, GetState) {
    consolidation_fep_bridge_connect_fep(bridge, fep);
    consolidation_fep_bridge_update(bridge);

    consolidation_fep_state_t state;
    int ret = consolidation_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, GetStats) {
    consolidation_fep_bridge_connect_fep(bridge, fep);
    consolidation_fep_bridge_update(bridge);

    consolidation_fep_stats_t stats;
    int ret = consolidation_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, BioAsync) {
    EXPECT_FALSE(consolidation_fep_bridge_is_bio_async_connected(bridge));
    consolidation_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(consolidation_fep_bridge_is_bio_async_connected(bridge));
    consolidation_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(consolidation_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Hierarchical FEP Bridge Tests
 * ============================================================================ */

class HierarchicalFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    hierarchical_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        hierarchical_fep_config_t config;
        hierarchical_fep_bridge_default_config(&config);
        bridge = hierarchical_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            hierarchical_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(HierarchicalFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HierarchicalFepBridgeTest, DefaultConfig) {
    hierarchical_fep_config_t config;
    int ret = hierarchical_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.hierarchy_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_hierarchical_prediction);
}

TEST_F(HierarchicalFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(hierarchical_fep_bridge_default_config(nullptr), 0);
}

TEST_F(HierarchicalFepBridgeTest, DestroyNull) {
    hierarchical_fep_bridge_destroy(nullptr);
}

TEST_F(HierarchicalFepBridgeTest, ConnectFep) {
    int ret = hierarchical_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(HierarchicalFepBridgeTest, Update) {
    hierarchical_fep_bridge_connect_fep(bridge, fep);
    int ret = hierarchical_fep_bridge_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(HierarchicalFepBridgeTest, GetStats) {
    hierarchical_fep_bridge_connect_fep(bridge, fep);
    hierarchical_fep_bridge_update(bridge);

    hierarchical_fep_stats_t stats;
    int ret = hierarchical_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(HierarchicalFepBridgeTest, BioAsync) {
    EXPECT_FALSE(hierarchical_fep_bridge_is_bio_async_connected(bridge));
    hierarchical_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(hierarchical_fep_bridge_is_bio_async_connected(bridge));
    hierarchical_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(hierarchical_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Meta-Learning FEP Bridge Tests
 * ============================================================================ */

class MetaLearningFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    meta_learning_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        meta_learning_fep_config_t config;
        meta_learning_fep_bridge_default_config(&config);
        bridge = meta_learning_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            meta_learning_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(MetaLearningFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MetaLearningFepBridgeTest, DefaultConfig) {
    meta_learning_fep_config_t config;
    int ret = meta_learning_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.meta_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_task_similarity_fe);
}

TEST_F(MetaLearningFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(meta_learning_fep_bridge_default_config(nullptr), 0);
}

TEST_F(MetaLearningFepBridgeTest, DestroyNull) {
    meta_learning_fep_bridge_destroy(nullptr);
}

TEST_F(MetaLearningFepBridgeTest, ConnectFep) {
    int ret = meta_learning_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(MetaLearningFepBridgeTest, Update) {
    meta_learning_fep_bridge_connect_fep(bridge, fep);
    int ret = meta_learning_fep_bridge_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MetaLearningFepBridgeTest, GetStats) {
    meta_learning_fep_bridge_connect_fep(bridge, fep);
    meta_learning_fep_bridge_update(bridge);

    meta_learning_fep_stats_t stats;
    int ret = meta_learning_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(MetaLearningFepBridgeTest, BioAsync) {
    EXPECT_FALSE(meta_learning_fep_bridge_is_bio_async_connected(bridge));
    meta_learning_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(meta_learning_fep_bridge_is_bio_async_connected(bridge));
    meta_learning_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(meta_learning_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Personality FEP Bridge Tests
 * ============================================================================ */

class PersonalityFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    personality_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        personality_fep_config_t config;
        personality_fep_bridge_default_config(&config);
        bridge = personality_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            personality_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(PersonalityFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PersonalityFepBridgeTest, DefaultConfig) {
    personality_fep_config_t config;
    int ret = personality_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(PersonalityFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(personality_fep_bridge_default_config(nullptr), 0);
}

TEST_F(PersonalityFepBridgeTest, DestroyNull) {
    personality_fep_bridge_destroy(nullptr);
}

TEST_F(PersonalityFepBridgeTest, ConnectFep) {
    int ret = personality_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(PersonalityFepBridgeTest, Update) {
    personality_fep_bridge_connect_fep(bridge, fep);
    /* Update requires personality system to be connected - without it, returns error */
    int ret = personality_fep_bridge_update(bridge);
    EXPECT_NE(ret, 0);
}

TEST_F(PersonalityFepBridgeTest, BioAsync) {
    EXPECT_FALSE(personality_fep_bridge_is_bio_async_connected(bridge));
    personality_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(personality_fep_bridge_is_bio_async_connected(bridge));
    personality_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(personality_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Wellbeing FEP Bridge Tests
 * ============================================================================ */

class WellbeingFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    wellbeing_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        wellbeing_fep_config_t config;
        wellbeing_fep_bridge_default_config(&config);
        bridge = wellbeing_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            wellbeing_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(WellbeingFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(WellbeingFepBridgeTest, DefaultConfig) {
    wellbeing_fep_config_t config;
    int ret = wellbeing_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(WellbeingFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(wellbeing_fep_bridge_default_config(nullptr), 0);
}

TEST_F(WellbeingFepBridgeTest, DestroyNull) {
    wellbeing_fep_bridge_destroy(nullptr);
}

TEST_F(WellbeingFepBridgeTest, ConnectFep) {
    int ret = wellbeing_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(WellbeingFepBridgeTest, Update) {
    wellbeing_fep_bridge_connect_fep(bridge, fep);
    int ret = wellbeing_fep_bridge_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(WellbeingFepBridgeTest, BioAsync) {
    EXPECT_FALSE(wellbeing_fep_bridge_is_bio_async_connected(bridge));
    wellbeing_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(wellbeing_fep_bridge_is_bio_async_connected(bridge));
    wellbeing_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(wellbeing_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Sleep-Wake FEP Bridge Tests
 * ============================================================================ */

class SleepWakeFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    sleep_wake_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();
        sleep_wake_fep_config_t config;
        sleep_wake_fep_bridge_default_config(&config);
        bridge = sleep_wake_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            sleep_wake_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(SleepWakeFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SleepWakeFepBridgeTest, DefaultConfig) {
    sleep_wake_fep_config_t config;
    int ret = sleep_wake_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(SleepWakeFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(sleep_wake_fep_bridge_default_config(nullptr), 0);
}

TEST_F(SleepWakeFepBridgeTest, DestroyNull) {
    sleep_wake_fep_bridge_destroy(nullptr);
}

TEST_F(SleepWakeFepBridgeTest, ConnectFep) {
    int ret = sleep_wake_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(SleepWakeFepBridgeTest, Update) {
    sleep_wake_fep_bridge_connect_fep(bridge, fep);
    /* Update requires sleep_system to be connected - without it, returns error */
    int ret = sleep_wake_fep_bridge_update(bridge);
    EXPECT_NE(ret, 0);
}

TEST_F(SleepWakeFepBridgeTest, BioAsync) {
    EXPECT_FALSE(sleep_wake_fep_bridge_is_bio_async_connected(bridge));
    sleep_wake_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(sleep_wake_fep_bridge_is_bio_async_connected(bridge));
    sleep_wake_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(sleep_wake_fep_bridge_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Brain Immune FEP Bridge Tests
 * ============================================================================ */

class BrainImmuneFepBridgeTest : public CognitiveFepBridgesTestBase {
protected:
    brain_immune_fep_bridge_t* bridge = nullptr;
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        CognitiveFepBridgesTestBase::SetUp();

        /* Create brain immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        /* Create FEP bridge */
        brain_immune_fep_config_t config;
        brain_immune_fep_default_config(&config);
        bridge = brain_immune_fep_create(&config, immune, fep);
    }

    void TearDown() override {
        if (bridge) {
            brain_immune_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        CognitiveFepBridgesTestBase::TearDown();
    }
};

TEST_F(BrainImmuneFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BrainImmuneFepBridgeTest, DefaultConfig) {
    brain_immune_fep_config_t config;
    int ret = brain_immune_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_precision_modulation);
    EXPECT_TRUE(config.enable_inflammation_errors);
    EXPECT_TRUE(config.enable_fep_guided_responses);
}

TEST_F(BrainImmuneFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(brain_immune_fep_default_config(nullptr), 0);
}

TEST_F(BrainImmuneFepBridgeTest, CreateWithNullImmune) {
    brain_immune_fep_config_t config;
    brain_immune_fep_default_config(&config);
    brain_immune_fep_bridge_t* br = brain_immune_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BrainImmuneFepBridgeTest, CreateWithNullFep) {
    brain_immune_fep_config_t config;
    brain_immune_fep_default_config(&config);
    brain_immune_fep_bridge_t* br = brain_immune_fep_create(&config, immune, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BrainImmuneFepBridgeTest, DestroyNull) {
    brain_immune_fep_destroy(nullptr);
}

TEST_F(BrainImmuneFepBridgeTest, Update) {
    int ret = brain_immune_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BrainImmuneFepBridgeTest, GetPrecisionModulation) {
    brain_immune_fep_update(bridge);
    float precision = brain_immune_fep_get_precision_modulation(bridge);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(BrainImmuneFepBridgeTest, GetPredictionError) {
    brain_immune_fep_update(bridge);
    float error = brain_immune_fep_get_prediction_error(bridge);
    EXPECT_GE(error, 0.0f);
}

TEST_F(BrainImmuneFepBridgeTest, GetStats) {
    brain_immune_fep_update(bridge);
    brain_immune_fep_stats_t stats;
    int ret = brain_immune_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_updates, 1);
}

TEST_F(BrainImmuneFepBridgeTest, BioAsync) {
    EXPECT_FALSE(brain_immune_fep_is_bio_async_connected(bridge));
    brain_immune_fep_connect_bio_async(bridge);
    EXPECT_TRUE(brain_immune_fep_is_bio_async_connected(bridge));
    brain_immune_fep_disconnect_bio_async(bridge);
    EXPECT_FALSE(brain_immune_fep_is_bio_async_connected(bridge));
}


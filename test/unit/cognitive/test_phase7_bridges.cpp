/**
 * @file test_phase7_bridges.cpp
 * @brief Unit tests for Phase 7 Cognitive Substrate and Thalamic Bridges
 *
 * Tests the additional substrate and thalamic bridges added in Phase 7:
 * - consolidation, emotion_tensor, empathetic_response, fault_tolerance
 * - fractal_cognitive, free_energy, game_theory, brain_immune, social
 * - theory_of_mind, predictive_immune, self_awareness_extended
 *
 * @date 2024-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
/* Substrate bridges */
#include "cognitive/consolidation/nimcp_consolidation_substrate_bridge.h"
#include "cognitive/emotion_tensor/nimcp_emotion_tensor_substrate_bridge.h"
#include "cognitive/empathetic_response/nimcp_empathetic_response_substrate_bridge.h"
#include "cognitive/fault_tolerance/nimcp_fault_tolerance_substrate_bridge.h"
#include "cognitive/fractal_cognitive/nimcp_fractal_cognitive_substrate_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy_substrate_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory_substrate_bridge.h"
#include "cognitive/immune/nimcp_brain_immune_substrate_bridge.h"
#include "cognitive/social/nimcp_social_substrate_bridge.h"
#include "cognitive/theory_of_mind/nimcp_theory_of_mind_substrate_bridge.h"
#include "cognitive/predictive_immune/nimcp_predictive_immune_substrate_bridge.h"
#include "cognitive/self_awareness_extended/nimcp_self_awareness_extended_substrate_bridge.h"

/* Thalamic bridges */
#include "cognitive/consolidation/nimcp_consolidation_thalamic_bridge.h"
#include "cognitive/emotion_tensor/nimcp_emotion_tensor_thalamic_bridge.h"
#include "cognitive/empathetic_response/nimcp_empathetic_response_thalamic_bridge.h"
#include "cognitive/fault_tolerance/nimcp_fault_tolerance_thalamic_bridge.h"
#include "cognitive/fractal_cognitive/nimcp_fractal_cognitive_thalamic_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy_thalamic_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory_thalamic_bridge.h"
#include "cognitive/immune/nimcp_brain_immune_thalamic_bridge.h"
#include "cognitive/social/nimcp_social_thalamic_bridge.h"
#include "cognitive/theory_of_mind/nimcp_theory_of_mind_thalamic_bridge.h"
#include "cognitive/predictive_immune/nimcp_predictive_immune_thalamic_bridge.h"
#include "cognitive/self_awareness_extended/nimcp_self_awareness_extended_thalamic_bridge.h"

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Consolidation Substrate Bridge Tests
 * ============================================================================ */

class ConsolidationSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    consolidation_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        substrate_config_t cfg;
        substrate_default_config(&cfg);
        substrate = substrate_create(&cfg);
    }

    void TearDown() override {
        if (bridge) {
            consolidation_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(ConsolidationSubstrateBridgeTest, CreateDestroy) {
    consolidation_substrate_config_t cfg = consolidation_substrate_default_config();
    bridge = consolidation_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ConsolidationSubstrateBridgeTest, CreateWithNullSubstrate) {
    consolidation_substrate_config_t cfg = consolidation_substrate_default_config();
    bridge = consolidation_substrate_bridge_create(nullptr, nullptr, &cfg);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ConsolidationSubstrateBridgeTest, DefaultConfig) {
    consolidation_substrate_config_t cfg = consolidation_substrate_default_config();
    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_modulation);
    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(ConsolidationSubstrateBridgeTest, UpdateAndGetEffects) {
    consolidation_substrate_config_t cfg = consolidation_substrate_default_config();
    bridge = consolidation_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);

    int ret = consolidation_substrate_bridge_update(bridge);
    EXPECT_EQ(ret, 0);

    consolidation_substrate_effects_t effects;
    ret = consolidation_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

/* ============================================================================
 * Consolidation Thalamic Bridge Tests
 * ============================================================================ */

class ConsolidationThalamicBridgeTest : public ::testing::Test {
protected:
    consolidation_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        consolidation_thalamic_config_t cfg = consolidation_thalamic_default_config();
        bridge = consolidation_thalamic_bridge_create(nullptr, nullptr, &cfg);
    }

    void TearDown() override {
        if (bridge) {
            consolidation_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ConsolidationThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ConsolidationThalamicBridgeTest, DefaultConfig) {
    consolidation_thalamic_config_t cfg = consolidation_thalamic_default_config();
    EXPECT_TRUE(cfg.enable_attention_gating);
    EXPECT_GT(cfg.min_memory_salience, 0.0f);
}

TEST_F(ConsolidationThalamicBridgeTest, SetGetAttention) {
    float attention;
    EXPECT_EQ(consolidation_thalamic_set_attention(bridge, 0.5f), 0);
    EXPECT_EQ(consolidation_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 0.5f);
}

TEST_F(ConsolidationThalamicBridgeTest, RouteEncode) {
    consolidation_thalamic_signal_t signal = {
        .signal_type = CONSOLIDATION_SIGNAL_ENCODE,
        .memory_salience = 0.7f,
        .emotional_weight = 0.6f,
        .urgency = 0.5f,
        .content = nullptr,
        .content_size = 0,
        .timestamp_us = 0
    };
    int ret = consolidation_thalamic_route_encode(bridge, &signal);
    EXPECT_EQ(ret, 0);

    consolidation_thalamic_stats_t stats;
    EXPECT_EQ(consolidation_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.encodes_routed, 1u);
}

/* ============================================================================
 * Fractal Cognitive Substrate Bridge Tests
 * ============================================================================ */

class FractalCognitiveSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    fractal_cognitive_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        substrate_config_t cfg;
        substrate_default_config(&cfg);
        substrate = substrate_create(&cfg);
    }

    void TearDown() override {
        if (bridge) {
            fractal_cognitive_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(FractalCognitiveSubstrateBridgeTest, CreateDestroy) {
    fractal_cognitive_substrate_config_t cfg = fractal_cognitive_substrate_default_config();
    bridge = fractal_cognitive_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FractalCognitiveSubstrateBridgeTest, UpdateAndGetEffects) {
    fractal_cognitive_substrate_config_t cfg = fractal_cognitive_substrate_default_config();
    bridge = fractal_cognitive_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);

    int ret = fractal_cognitive_substrate_bridge_update(bridge);
    EXPECT_EQ(ret, 0);

    fractal_cognitive_substrate_effects_t effects;
    ret = fractal_cognitive_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

/* ============================================================================
 * Fractal Cognitive Thalamic Bridge Tests
 * ============================================================================ */

class FractalCognitiveThalamicBridgeTest : public ::testing::Test {
protected:
    fractal_cognitive_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        fractal_cognitive_thalamic_config_t cfg = fractal_cognitive_thalamic_default_config();
        bridge = fractal_cognitive_thalamic_bridge_create(nullptr, nullptr, &cfg);
    }

    void TearDown() override {
        if (bridge) {
            fractal_cognitive_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(FractalCognitiveThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FractalCognitiveThalamicBridgeTest, RouteScale) {
    fractal_cognitive_thalamic_signal_t signal = {
        .signal_type = FRACTAL_SIGNAL_SCALE_UP,
        .scale_level = 0.5f,
        .complexity = 0.5f,
        .urgency = 0.5f,
        .fractal_data = nullptr,
        .data_size = 0,
        .timestamp_us = 0
    };
    int ret = fractal_cognitive_thalamic_route_scale(bridge, &signal);
    EXPECT_EQ(ret, 0);

    fractal_cognitive_thalamic_stats_t stats;
    EXPECT_EQ(fractal_cognitive_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.scales_routed, 1u);
}

/* ============================================================================
 * Free Energy Thalamic Bridge Tests
 * ============================================================================ */

class FreeEnergyThalamicBridgeTest : public ::testing::Test {
protected:
    free_energy_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        free_energy_thalamic_config_t cfg = free_energy_thalamic_default_config();
        bridge = free_energy_thalamic_bridge_create(nullptr, nullptr, &cfg);
    }

    void TearDown() override {
        if (bridge) {
            free_energy_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(FreeEnergyThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FreeEnergyThalamicBridgeTest, DefaultConfig) {
    free_energy_thalamic_config_t cfg = free_energy_thalamic_default_config();
    EXPECT_TRUE(cfg.enable_attention_gating);
    EXPECT_TRUE(cfg.enable_precision_boost);
}

TEST_F(FreeEnergyThalamicBridgeTest, RoutePredictionError) {
    free_energy_thalamic_signal_t signal = {
        .signal_type = 0,
        .prediction_error = 0.5f,
        .precision = 0.8f,
        .content = nullptr,
        .content_size = 0,
        .timestamp_us = 0
    };
    int ret = free_energy_thalamic_route_prediction_error(bridge, &signal);
    EXPECT_EQ(ret, 0);

    free_energy_thalamic_stats_t stats;
    EXPECT_EQ(free_energy_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.errors_routed, 1u);
}

/* ============================================================================
 * Predictive Immune Substrate Bridge Tests
 * ============================================================================ */

class PredictiveImmuneSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    predictive_immune_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        substrate_config_t cfg;
        substrate_default_config(&cfg);
        substrate = substrate_create(&cfg);
    }

    void TearDown() override {
        if (bridge) {
            predictive_immune_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(PredictiveImmuneSubstrateBridgeTest, CreateDestroy) {
    predictive_immune_substrate_config_t cfg = predictive_immune_substrate_default_config();
    bridge = predictive_immune_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PredictiveImmuneSubstrateBridgeTest, DefaultConfig) {
    predictive_immune_substrate_config_t cfg = predictive_immune_substrate_default_config();
    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_modulation);
}

TEST_F(PredictiveImmuneSubstrateBridgeTest, UpdateAndGetEffects) {
    predictive_immune_substrate_config_t cfg = predictive_immune_substrate_default_config();
    bridge = predictive_immune_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);

    int ret = predictive_immune_substrate_bridge_update(bridge);
    EXPECT_EQ(ret, 0);

    predictive_immune_substrate_effects_t effects;
    ret = predictive_immune_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

/* ============================================================================
 * Predictive Immune Thalamic Bridge Tests
 * ============================================================================ */

class PredictiveImmuneThalamicBridgeTest : public ::testing::Test {
protected:
    predictive_immune_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        predictive_immune_thalamic_config_t cfg = predictive_immune_thalamic_default_config();
        bridge = predictive_immune_thalamic_bridge_create(nullptr, nullptr, &cfg);
    }

    void TearDown() override {
        if (bridge) {
            predictive_immune_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(PredictiveImmuneThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PredictiveImmuneThalamicBridgeTest, DefaultConfig) {
    predictive_immune_thalamic_config_t cfg = predictive_immune_thalamic_default_config();
    EXPECT_TRUE(cfg.enable_attention_gating);
    EXPECT_TRUE(cfg.enable_urgency_boost);
}

TEST_F(PredictiveImmuneThalamicBridgeTest, RouteInteroception) {
    int ret = predictive_immune_thalamic_route_interoception(bridge, 0.7f, 0.6f);
    EXPECT_EQ(ret, 0);

    predictive_immune_thalamic_stats_t stats;
    EXPECT_EQ(predictive_immune_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.interoceptions_routed, 1u);
}

TEST_F(PredictiveImmuneThalamicBridgeTest, RouteCytokine) {
    int ret = predictive_immune_thalamic_route_cytokine(bridge, 0.5f, 0.8f);
    EXPECT_EQ(ret, 0);

    predictive_immune_thalamic_stats_t stats;
    EXPECT_EQ(predictive_immune_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.cytokine_updates, 1u);
}

/* ============================================================================
 * Self Awareness Extended Substrate Bridge Tests
 * ============================================================================ */

class SelfAwarenessExtSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    self_awareness_ext_substrate_bridge_t* bridge = nullptr;

    void SetUp() override {
        substrate_config_t cfg;
        substrate_default_config(&cfg);
        substrate = substrate_create(&cfg);
    }

    void TearDown() override {
        if (bridge) {
            self_awareness_ext_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(SelfAwarenessExtSubstrateBridgeTest, CreateDestroy) {
    self_awareness_ext_substrate_config_t cfg = self_awareness_ext_substrate_default_config();
    bridge = self_awareness_ext_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SelfAwarenessExtSubstrateBridgeTest, DefaultConfig) {
    self_awareness_ext_substrate_config_t cfg = self_awareness_ext_substrate_default_config();
    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_modulation);
}

TEST_F(SelfAwarenessExtSubstrateBridgeTest, UpdateAndGetEffects) {
    self_awareness_ext_substrate_config_t cfg = self_awareness_ext_substrate_default_config();
    bridge = self_awareness_ext_substrate_bridge_create(nullptr, substrate, &cfg);
    ASSERT_NE(bridge, nullptr);

    int ret = self_awareness_ext_substrate_bridge_update(bridge);
    EXPECT_EQ(ret, 0);

    self_awareness_ext_substrate_effects_t effects;
    ret = self_awareness_ext_substrate_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

/* ============================================================================
 * Self Awareness Extended Thalamic Bridge Tests
 * ============================================================================ */

class SelfAwarenessExtThalamicBridgeTest : public ::testing::Test {
protected:
    self_awareness_ext_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        self_awareness_ext_thalamic_config_t cfg = self_awareness_ext_thalamic_default_config();
        bridge = self_awareness_ext_thalamic_bridge_create(nullptr, nullptr, &cfg);
    }

    void TearDown() override {
        if (bridge) {
            self_awareness_ext_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(SelfAwarenessExtThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SelfAwarenessExtThalamicBridgeTest, DefaultConfig) {
    self_awareness_ext_thalamic_config_t cfg = self_awareness_ext_thalamic_default_config();
    EXPECT_TRUE(cfg.enable_attention_gating);
    EXPECT_TRUE(cfg.enable_depth_boost);
}

TEST_F(SelfAwarenessExtThalamicBridgeTest, RouteMetacognition) {
    int ret = self_awareness_ext_thalamic_route_metacognition(bridge, 0.7f, 0.6f);
    EXPECT_EQ(ret, 0);

    self_awareness_ext_thalamic_stats_t stats;
    EXPECT_EQ(self_awareness_ext_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.metacognitions_routed, 1u);
}

TEST_F(SelfAwarenessExtThalamicBridgeTest, RouteTemporal) {
    int ret = self_awareness_ext_thalamic_route_temporal(bridge, 0.8f, 0.5f);
    EXPECT_EQ(ret, 0);

    self_awareness_ext_thalamic_stats_t stats;
    EXPECT_EQ(self_awareness_ext_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.temporal_updates, 1u);
}

TEST_F(SelfAwarenessExtThalamicBridgeTest, SetGetAttention) {
    float attention;
    EXPECT_EQ(self_awareness_ext_thalamic_set_attention(bridge, 0.5f), 0);
    EXPECT_EQ(self_awareness_ext_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 0.5f);

    /* Test clamping */
    EXPECT_EQ(self_awareness_ext_thalamic_set_attention(bridge, -0.5f), 0);
    EXPECT_EQ(self_awareness_ext_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 0.0f);

    EXPECT_EQ(self_awareness_ext_thalamic_set_attention(bridge, 1.5f), 0);
    EXPECT_EQ(self_awareness_ext_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 1.0f);
}

/* ============================================================================
 * Null Safety Tests
 * ============================================================================ */

TEST(NullSafetyTest, ConsolidationSubstrateBridgeNullSafety) {
    EXPECT_EQ(consolidation_substrate_bridge_update(nullptr), -1);
    EXPECT_EQ(consolidation_substrate_bridge_get_effects(nullptr, nullptr), -1);
    consolidation_substrate_bridge_destroy(nullptr);  /* Should not crash */
}

TEST(NullSafetyTest, ConsolidationThalamicBridgeNullSafety) {
    EXPECT_EQ(consolidation_thalamic_set_attention(nullptr, 0.5f), -1);
    EXPECT_EQ(consolidation_thalamic_get_attention(nullptr, nullptr), -1);
    EXPECT_EQ(consolidation_thalamic_bridge_get_stats(nullptr, nullptr), -1);
    consolidation_thalamic_bridge_destroy(nullptr);  /* Should not crash */
}

TEST(NullSafetyTest, PredictiveImmuneSubstrateBridgeNullSafety) {
    EXPECT_EQ(predictive_immune_substrate_bridge_update(nullptr), -1);
    EXPECT_EQ(predictive_immune_substrate_bridge_get_effects(nullptr, nullptr), -1);
    predictive_immune_substrate_bridge_destroy(nullptr);  /* Should not crash */
}

TEST(NullSafetyTest, SelfAwarenessExtSubstrateBridgeNullSafety) {
    EXPECT_EQ(self_awareness_ext_substrate_bridge_update(nullptr), -1);
    EXPECT_EQ(self_awareness_ext_substrate_bridge_get_effects(nullptr, nullptr), -1);
    self_awareness_ext_substrate_bridge_destroy(nullptr);  /* Should not crash */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

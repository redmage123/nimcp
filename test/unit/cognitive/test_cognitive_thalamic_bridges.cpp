/**
 * @file test_cognitive_thalamic_bridges.cpp
 * @brief Unit tests for Cognitive-Thalamic Bridge modules
 *
 * WHAT: Comprehensive tests for cognitive-thalamic attention-gated routing
 * WHY:  Ensure cognitive systems integrate with thalamic pathways for conscious processing
 * HOW:  Test lifecycle, signal routing, attention gating, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/analysis/nimcp_analysis_thalamic_bridge.h"
#include "cognitive/attention/nimcp_attention_thalamic_bridge.h"
#include "cognitive/emotional_tagging/nimcp_emotional_tagging_thalamic_bridge.h"
#include "cognitive/emotion/nimcp_emotion_thalamic_bridge.h"
#include "cognitive/executive/nimcp_executive_thalamic_bridge.h"
#include "cognitive/epistemic/nimcp_epistemic_thalamic_bridge.h"
#include "cognitive/explanations/nimcp_explanations_thalamic_bridge.h"
#include "cognitive/introspection/nimcp_introspection_thalamic_bridge.h"
#include "cognitive/memory/nimcp_memory_thalamic_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_thalamic_bridge.h"
#include "cognitive/self_model/nimcp_self_model_thalamic_bridge.h"
#include "cognitive/shadow_emotions/nimcp_shadow_emotions_thalamic_bridge.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_thalamic_bridge.h"
#include "cognitive/tom/nimcp_tom_thalamic_bridge.h"
#include "cognitive/wellbeing/nimcp_wellbeing_thalamic_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_thalamic_bridge.h"

/* ============================================================================
 * Analysis Thalamic Bridge Tests
 * ============================================================================ */

class AnalysisThalamicBridgeTest : public ::testing::Test {
protected:
    analysis_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        analysis_thalamic_config_t config = analysis_thalamic_default_config();
        bridge = analysis_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            analysis_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(AnalysisThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AnalysisThalamicBridgeTest, DefaultConfig) {
    analysis_thalamic_config_t config = analysis_thalamic_default_config();
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_complexity_routing);
    EXPECT_GT(config.min_urgency_threshold, 0.0f);
    EXPECT_GT(config.complexity_boost, 1.0f);
}

TEST_F(AnalysisThalamicBridgeTest, Reset) {
    int ret = analysis_thalamic_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(analysis_thalamic_bridge_reset(nullptr), -1);
}

TEST_F(AnalysisThalamicBridgeTest, SetGetAttention) {
    float attention;
    EXPECT_EQ(analysis_thalamic_set_attention(bridge, 0.5f), 0);
    EXPECT_EQ(analysis_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 0.5f);

    /* Clamp tests */
    EXPECT_EQ(analysis_thalamic_set_attention(bridge, -0.5f), 0);
    EXPECT_EQ(analysis_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 0.0f);

    EXPECT_EQ(analysis_thalamic_set_attention(bridge, 1.5f), 0);
    EXPECT_EQ(analysis_thalamic_get_attention(bridge, &attention), 0);
    EXPECT_FLOAT_EQ(attention, 1.0f);
}

TEST_F(AnalysisThalamicBridgeTest, RouteDecomposition) {
    int ret = analysis_thalamic_route_decomposition(bridge, nullptr, 0, 0.5f);
    EXPECT_EQ(ret, 0);

    analysis_thalamic_stats_t stats;
    EXPECT_EQ(analysis_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.decompositions_routed, 1u);
}

TEST_F(AnalysisThalamicBridgeTest, RequestDepth) {
    int ret = analysis_thalamic_request_depth(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);

    analysis_thalamic_stats_t stats;
    EXPECT_EQ(analysis_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.depth_requests, 1u);
}

TEST_F(AnalysisThalamicBridgeTest, AttentionGating) {
    /* Set low attention to gate signals */
    analysis_thalamic_set_attention(bridge, 0.1f);

    analysis_thalamic_signal_t signal = {
        .signal_type = ANALYSIS_SIGNAL_DECOMPOSITION,
        .analysis_urgency = 0.3f,
        .depth_required = 0.5f,
        .complexity = 0.5f,
        .content = nullptr,
        .content_size = 0,
        .timestamp_us = 0
    };

    int ret = analysis_thalamic_route_signal(bridge, &signal);
    EXPECT_EQ(ret, 0);

    analysis_thalamic_stats_t stats;
    EXPECT_EQ(analysis_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.signals_gated, 1u);
}

TEST_F(AnalysisThalamicBridgeTest, NullChecks) {
    EXPECT_EQ(analysis_thalamic_route_decomposition(nullptr, nullptr, 0, 0.5f), -1);
    EXPECT_EQ(analysis_thalamic_route_signal(bridge, nullptr), -1);
    EXPECT_EQ(analysis_thalamic_set_attention(nullptr, 0.5f), -1);
    EXPECT_EQ(analysis_thalamic_get_attention(nullptr, nullptr), -1);
    EXPECT_EQ(analysis_thalamic_bridge_get_stats(nullptr, nullptr), -1);
}

/* ============================================================================
 * Attention Thalamic Bridge Tests
 * ============================================================================ */

class AttentionThalamicBridgeTest : public ::testing::Test {
protected:
    attention_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_thalamic_config_t config = attention_thalamic_default_config();
        bridge = attention_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            attention_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(AttentionThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AttentionThalamicBridgeTest, DefaultConfig) {
    attention_thalamic_config_t config = attention_thalamic_default_config();
    EXPECT_TRUE(config.enable_priority_gating);
    EXPECT_TRUE(config.enable_shift_cost);
    EXPECT_TRUE(config.enable_vigilance_boost);
}

TEST_F(AttentionThalamicBridgeTest, RequestFocus) {
    int ret = attention_thalamic_request_focus(bridge, 0.8f, 0.7f);
    EXPECT_EQ(ret, 0);

    attention_thalamic_stats_t stats;
    EXPECT_EQ(attention_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.focus_requests, 1u);
}

TEST_F(AttentionThalamicBridgeTest, RequestShift) {
    int ret = attention_thalamic_request_shift(bridge, 0.7f, 0.3f);
    EXPECT_EQ(ret, 0);

    attention_thalamic_stats_t stats;
    EXPECT_EQ(attention_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.shifts_executed, 1u);
}

TEST_F(AttentionThalamicBridgeTest, ActivateFilter) {
    int ret = attention_thalamic_activate_filter(bridge, 0.6f);
    EXPECT_EQ(ret, 0);

    attention_thalamic_stats_t stats;
    EXPECT_EQ(attention_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.filter_activations, 1u);
}

/* ============================================================================
 * Emotion Thalamic Bridge Tests
 * ============================================================================ */

class EmotionThalamicBridgeTest : public ::testing::Test {
protected:
    emotion_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_thalamic_config_t config = emotion_thalamic_default_config();
        bridge = emotion_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            emotion_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EmotionThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionThalamicBridgeTest, RouteArousal) {
    int ret = emotion_thalamic_route_arousal(bridge, 0.7f, 0.6f);
    EXPECT_EQ(ret, 0);

    emotion_thalamic_stats_t stats;
    EXPECT_EQ(emotion_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.arousal_signals, 1u);
}

TEST_F(EmotionThalamicBridgeTest, RouteRegulation) {
    int ret = emotion_thalamic_route_regulation(bridge, 0.5f, 0.8f);
    EXPECT_EQ(ret, 0);

    emotion_thalamic_stats_t stats;
    EXPECT_EQ(emotion_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.regulations_attempted, 1u);
}

/* ============================================================================
 * Executive Thalamic Bridge Tests
 * ============================================================================ */

class ExecutiveThalamicBridgeTest : public ::testing::Test {
protected:
    executive_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        executive_thalamic_config_t config = executive_thalamic_default_config();
        bridge = executive_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            executive_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ExecutiveThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ExecutiveThalamicBridgeTest, RouteInhibition) {
    int ret = executive_thalamic_route_inhibition(bridge, 0.8f, 0.7f);
    EXPECT_EQ(ret, 0);

    executive_thalamic_stats_t stats;
    EXPECT_EQ(executive_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inhibitions_routed, 1u);
}

TEST_F(ExecutiveThalamicBridgeTest, RouteSwitch) {
    int ret = executive_thalamic_route_switch(bridge, 0.4f, 0.6f);
    EXPECT_EQ(ret, 0);

    executive_thalamic_stats_t stats;
    EXPECT_EQ(executive_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.switches_executed, 1u);
}

/* ============================================================================
 * Memory Thalamic Bridge Tests
 * ============================================================================ */

class MemoryThalamicBridgeTest : public ::testing::Test {
protected:
    memory_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        memory_thalamic_config_t config = memory_thalamic_default_config();
        bridge = memory_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            memory_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(MemoryThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(MemoryThalamicBridgeTest, RouteEncode) {
    int ret = memory_thalamic_route_encode(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);

    memory_thalamic_stats_t stats;
    EXPECT_EQ(memory_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.encodings_routed, 1u);
}

TEST_F(MemoryThalamicBridgeTest, RouteRetrieve) {
    int ret = memory_thalamic_route_retrieve(bridge, 0.7f, 0.5f);
    EXPECT_EQ(ret, 0);

    memory_thalamic_stats_t stats;
    EXPECT_EQ(memory_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.retrievals_routed, 1u);
}

TEST_F(MemoryThalamicBridgeTest, EmotionalBoost) {
    /* High emotional weight should get routed even with lower base urgency */
    int ret = memory_thalamic_route_encode(bridge, 0.6f, 0.9f);  /* High emotional weight */
    EXPECT_EQ(ret, 0);

    memory_thalamic_stats_t stats;
    EXPECT_EQ(memory_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.avg_emotional_weight, 0.0f);
}

/* ============================================================================
 * Reasoning Thalamic Bridge Tests
 * ============================================================================ */

class ReasoningThalamicBridgeTest : public ::testing::Test {
protected:
    reasoning_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        reasoning_thalamic_config_t config = reasoning_thalamic_default_config();
        bridge = reasoning_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            reasoning_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ReasoningThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ReasoningThalamicBridgeTest, RouteInference) {
    int ret = reasoning_thalamic_route_inference(bridge, 0.7f, 0.8f);
    EXPECT_EQ(ret, 0);

    reasoning_thalamic_stats_t stats;
    EXPECT_EQ(reasoning_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inferences_routed, 1u);
}

TEST_F(ReasoningThalamicBridgeTest, RouteConclusion) {
    int ret = reasoning_thalamic_route_conclusion(bridge, 0.9f, 0.6f);
    EXPECT_EQ(ret, 0);

    reasoning_thalamic_stats_t stats;
    EXPECT_EQ(reasoning_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.conclusions, 1u);
}

/* ============================================================================
 * Working Memory Thalamic Bridge Tests
 * ============================================================================ */

class WorkingMemoryThalamicBridgeTest : public ::testing::Test {
protected:
    working_memory_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        working_memory_thalamic_config_t config = working_memory_thalamic_default_config();
        bridge = working_memory_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            working_memory_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(WorkingMemoryThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(WorkingMemoryThalamicBridgeTest, RouteEncode) {
    int ret = working_memory_thalamic_route_encode(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);

    working_memory_thalamic_stats_t stats;
    EXPECT_EQ(working_memory_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.encodings, 1u);
}

TEST_F(WorkingMemoryThalamicBridgeTest, RouteUpdate) {
    int ret = working_memory_thalamic_route_update(bridge, 0.7f, 0.5f);
    EXPECT_EQ(ret, 0);

    working_memory_thalamic_stats_t stats;
    EXPECT_EQ(working_memory_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.updates, 1u);
}

/* ============================================================================
 * Theory of Mind Thalamic Bridge Tests
 * ============================================================================ */

class TomThalamicBridgeTest : public ::testing::Test {
protected:
    tom_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        tom_thalamic_config_t config = tom_thalamic_default_config();
        bridge = tom_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            tom_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(TomThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(TomThalamicBridgeTest, RouteBeliefAttribution) {
    int ret = tom_thalamic_route_belief_attribution(bridge, 0.7f, 0.8f);
    EXPECT_EQ(ret, 0);

    tom_thalamic_stats_t stats;
    EXPECT_EQ(tom_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.belief_attributions, 1u);
}

TEST_F(TomThalamicBridgeTest, RoutePerspective) {
    int ret = tom_thalamic_route_perspective(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);

    tom_thalamic_stats_t stats;
    EXPECT_EQ(tom_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.perspective_takes, 1u);
}

/* ============================================================================
 * Wellbeing Thalamic Bridge Tests
 * ============================================================================ */

class WellbeingThalamicBridgeTest : public ::testing::Test {
protected:
    wellbeing_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        wellbeing_thalamic_config_t config = wellbeing_thalamic_default_config();
        bridge = wellbeing_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            wellbeing_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(WellbeingThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(WellbeingThalamicBridgeTest, RouteStatus) {
    int ret = wellbeing_thalamic_route_status(bridge, 0.8f, 0.9f);
    EXPECT_EQ(ret, 0);

    wellbeing_thalamic_stats_t stats;
    EXPECT_EQ(wellbeing_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.status_updates, 1u);
}

TEST_F(WellbeingThalamicBridgeTest, RouteThreat) {
    int ret = wellbeing_thalamic_route_threat(bridge, 0.7f, 0.9f);
    EXPECT_EQ(ret, 0);

    wellbeing_thalamic_stats_t stats;
    EXPECT_EQ(wellbeing_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.threats_signaled, 1u);
}

/* ============================================================================
 * Introspection Thalamic Bridge Tests
 * ============================================================================ */

class IntrospectionThalamicBridgeTest : public ::testing::Test {
protected:
    introspection_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        introspection_thalamic_config_t config = introspection_thalamic_default_config();
        bridge = introspection_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            introspection_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(IntrospectionThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(IntrospectionThalamicBridgeTest, RouteMonitor) {
    int ret = introspection_thalamic_route_monitor(bridge, 0.6f, 0.5f);
    EXPECT_EQ(ret, 0);

    introspection_thalamic_stats_t stats;
    EXPECT_EQ(introspection_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.monitors_routed, 1u);
}

TEST_F(IntrospectionThalamicBridgeTest, RouteReflection) {
    int ret = introspection_thalamic_route_reflection(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);

    introspection_thalamic_stats_t stats;
    EXPECT_EQ(introspection_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.reflections, 1u);
}

/* ============================================================================
 * Self Model Thalamic Bridge Tests
 * ============================================================================ */

class SelfModelThalamicBridgeTest : public ::testing::Test {
protected:
    self_model_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        self_model_thalamic_config_t config = self_model_thalamic_default_config();
        bridge = self_model_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            self_model_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(SelfModelThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SelfModelThalamicBridgeTest, RouteUpdate) {
    int ret = self_model_thalamic_route_update(bridge, 0.8f, 0.5f);
    EXPECT_EQ(ret, 0);

    self_model_thalamic_stats_t stats;
    EXPECT_EQ(self_model_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.updates_routed, 1u);
}

TEST_F(SelfModelThalamicBridgeTest, RouteConflict) {
    int ret = self_model_thalamic_route_conflict(bridge, 0.6f, 0.8f);
    EXPECT_EQ(ret, 0);

    self_model_thalamic_stats_t stats;
    EXPECT_EQ(self_model_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.conflicts_detected, 1u);
}

/* ============================================================================
 * Epistemic Thalamic Bridge Tests
 * ============================================================================ */

class EpistemicThalamicBridgeTest : public ::testing::Test {
protected:
    epistemic_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        epistemic_thalamic_config_t config = epistemic_thalamic_default_config();
        bridge = epistemic_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            epistemic_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EpistemicThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EpistemicThalamicBridgeTest, RouteUncertainty) {
    int ret = epistemic_thalamic_route_uncertainty(bridge, 0.7f, 0.6f);
    EXPECT_EQ(ret, 0);

    epistemic_thalamic_stats_t stats;
    EXPECT_EQ(epistemic_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.uncertainties_routed, 1u);
}

TEST_F(EpistemicThalamicBridgeTest, RouteInquiry) {
    int ret = epistemic_thalamic_route_inquiry(bridge, 0.8f, 0.5f);
    EXPECT_EQ(ret, 0);

    epistemic_thalamic_stats_t stats;
    EXPECT_EQ(epistemic_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inquiries_routed, 1u);
}

/* ============================================================================
 * Symbolic Logic Thalamic Bridge Tests
 * ============================================================================ */

class SymbolicLogicThalamicBridgeTest : public ::testing::Test {
protected:
    symbolic_logic_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        symbolic_logic_thalamic_config_t config = symbolic_logic_thalamic_default_config();
        bridge = symbolic_logic_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            symbolic_logic_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(SymbolicLogicThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SymbolicLogicThalamicBridgeTest, ApplyRule) {
    int ret = symbolic_logic_thalamic_apply_rule(bridge, 0.6f, 0.5f);
    EXPECT_EQ(ret, 0);

    symbolic_logic_thalamic_stats_t stats;
    EXPECT_EQ(symbolic_logic_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.rules_applied, 1u);
}

/* ============================================================================
 * Shadow Emotions Thalamic Bridge Tests
 * ============================================================================ */

class ShadowEmotionsThalamicBridgeTest : public ::testing::Test {
protected:
    shadow_emotions_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        shadow_emotions_thalamic_config_t config = shadow_emotions_thalamic_default_config();
        bridge = shadow_emotions_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            shadow_emotions_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ShadowEmotionsThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ShadowEmotionsThalamicBridgeTest, RouteEmergence) {
    int ret = shadow_emotions_thalamic_route_emergence(bridge, 0.7f, 0.6f);
    EXPECT_EQ(ret, 0);

    shadow_emotions_thalamic_stats_t stats;
    EXPECT_EQ(shadow_emotions_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.emergences, 1u);
}

/* ============================================================================
 * Emotional Tagging Thalamic Bridge Tests
 * ============================================================================ */

class EmotionalTaggingThalamicBridgeTest : public ::testing::Test {
protected:
    emotional_tagging_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotional_tagging_thalamic_config_t config = emotional_tagging_thalamic_default_config();
        bridge = emotional_tagging_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            emotional_tagging_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(EmotionalTaggingThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionalTaggingThalamicBridgeTest, ApplyTag) {
    int ret = emotional_tagging_thalamic_apply_tag(bridge, 0.7f, 0.3f);
    EXPECT_EQ(ret, 0);

    emotional_tagging_thalamic_stats_t stats;
    EXPECT_EQ(emotional_tagging_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.tags_applied, 1u);
}

/* ============================================================================
 * Explanations Thalamic Bridge Tests
 * ============================================================================ */

class ExplanationsThalamicBridgeTest : public ::testing::Test {
protected:
    explanations_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        explanations_thalamic_config_t config = explanations_thalamic_default_config();
        bridge = explanations_thalamic_bridge_create(nullptr, nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            explanations_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(ExplanationsThalamicBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ExplanationsThalamicBridgeTest, RouteCausal) {
    int ret = explanations_thalamic_route_causal(bridge, 0.6f, 0.5f);
    EXPECT_EQ(ret, 0);

    explanations_thalamic_stats_t stats;
    EXPECT_EQ(explanations_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.causal_explanations, 1u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// test_empathy_network_api.cpp
// ============================================================================
// Unit tests for the three previously-phantom empathy_network APIs:
//   empathy_network_observe
//   empathy_network_get_state
//   empathy_network_predict_impact
// ============================================================================

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/ethics/nimcp_ethics.h"
#include "networking/protocol/nimcp_protocol.h"
}

namespace {

empathy_config_t make_cfg() {
    empathy_config_t cfg = {};
    cfg.mirror_network        = nullptr;
    cfg.observation_window_ms = 1000;
    cfg.empathy_threshold     = 0.5f;
    return cfg;
}

event_packet_t make_packet(uint32_t src, uint16_t conf, uint8_t flags) {
    event_packet_t p = {};
    p.version_flags  = flags;
    p.source_node_id = src;
    p.confidence     = conf;
    p.feature_low    = 0x1234;
    p.feature_high   = 0x5678;
    p.hop_count      = 1;
    p.timestamp      = 0;
    return p;
}

// ----------------------------------------------------------------------------
// observe
// ----------------------------------------------------------------------------

TEST(EmpathyNetworkObserve, NullInputsReturnFalse) {
    event_packet_t p = make_packet(1, 40000, EVENT_FLAG_EXCITATORY);
    EXPECT_FALSE(empathy_network_observe(nullptr, &p, 100));

    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);
    EXPECT_FALSE(empathy_network_observe(n, nullptr, 100));
    empathy_network_destroy(n);
}

TEST(EmpathyNetworkObserve, HighConfidenceExcitatoryTriggers) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    // confidence=60000/65535 ~ 0.91 > 0.5 → triggers
    event_packet_t p = make_packet(42, 60000, EVENT_FLAG_EXCITATORY);
    EXPECT_TRUE(empathy_network_observe(n, &p, 1000));

    empathy_network_destroy(n);
}

TEST(EmpathyNetworkObserve, LowConfidenceDoesNotTrigger) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    // confidence=10000/65535 ~ 0.15 < 0.5 → no trigger, but observation stored
    event_packet_t p = make_packet(42, 10000, EVENT_FLAG_EXCITATORY);
    EXPECT_FALSE(empathy_network_observe(n, &p, 1000));

    empathy_state_t state = {};
    EXPECT_TRUE(empathy_network_get_state(n, &state));
    EXPECT_EQ(state.observed_node_id, 42u);

    empathy_network_destroy(n);
}

TEST(EmpathyNetworkObserve, InhibitoryFlipsSignOfActivation) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    event_packet_t p = make_packet(7, 60000, EVENT_FLAG_INHIBITORY);
    EXPECT_TRUE(empathy_network_observe(n, &p, 500));

    empathy_state_t s = {};
    ASSERT_TRUE(empathy_network_get_state(n, &s));
    EXPECT_LT(s.mirror_activation, 0.0f);

    empathy_network_destroy(n);
}

// ----------------------------------------------------------------------------
// get_state
// ----------------------------------------------------------------------------

TEST(EmpathyNetworkGetState, NoObservationsReturnsFalse) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    empathy_state_t s = {};
    EXPECT_FALSE(empathy_network_get_state(n, &s));
    empathy_network_destroy(n);
}

TEST(EmpathyNetworkGetState, ReturnsMostRecentTimestamp) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    event_packet_t p1 = make_packet(1, 40000, EVENT_FLAG_EXCITATORY);
    event_packet_t p2 = make_packet(2, 40000, EVENT_FLAG_EXCITATORY);
    event_packet_t p3 = make_packet(3, 40000, EVENT_FLAG_EXCITATORY);
    empathy_network_observe(n, &p1, 100);
    empathy_network_observe(n, &p3, 300);   // newest
    empathy_network_observe(n, &p2, 200);

    empathy_state_t s = {};
    ASSERT_TRUE(empathy_network_get_state(n, &s));
    EXPECT_EQ(s.observed_node_id, 3u);
    EXPECT_EQ(s.timestamp, 300u);

    empathy_network_destroy(n);
}

// ----------------------------------------------------------------------------
// predict_impact
// ----------------------------------------------------------------------------

TEST(EmpathyNetworkPredictImpact, NullReturnsZero) {
    event_packet_t p = make_packet(1, 40000, EVENT_FLAG_EXCITATORY);
    EXPECT_FLOAT_EQ(empathy_network_predict_impact(nullptr, &p), 0.0f);

    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);
    EXPECT_FLOAT_EQ(empathy_network_predict_impact(n, nullptr), 0.0f);
    empathy_network_destroy(n);
}

TEST(EmpathyNetworkPredictImpact, ReturnsFloatFromPerspectiveNetwork) {
    empathy_config_t cfg = make_cfg();
    empathy_network_t n = empathy_network_create(&cfg);
    ASSERT_NE(n, nullptr);

    // With a freshly-initialized perspective_network the output will be ~0
    // but the call must complete without crashing and return a finite value.
    event_packet_t p = make_packet(1, 40000, EVENT_FLAG_EXCITATORY);
    float impact = empathy_network_predict_impact(n, &p);
    EXPECT_TRUE(std::isfinite(impact));
    EXPECT_GE(impact, -1.5f);
    EXPECT_LE(impact,  1.5f);

    empathy_network_destroy(n);
}

}  // namespace

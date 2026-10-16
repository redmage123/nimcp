/**
 * @file test_security_fep_bridges.cpp
 * @brief Unit tests for all Security-FEP Bridge modules
 *
 * WHAT: Comprehensive tests for security-FEP bidirectional integrations
 * WHY:  Ensure threat detection and anomaly handling integrate with FEP predictions
 * HOW:  Test lifecycle, effects, and security-specific FEP modulations
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_anomaly_detector_fep_bridge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_blood_brain_barrier_fep_bridge.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_pattern_db_fep_bridge.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_rate_limiter_fep_bridge.h"
#include "security/nimcp_security_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class SecurityFepBridgesTestBase : public ::testing::Test {
protected:
    fep_system_t* fep = nullptr;

    void SetUp() override {
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
    }
};

/* ============================================================================
 * Anomaly Detector FEP Bridge Tests
 * ============================================================================ */

class AnomalyDetectorFepBridgeTest : public SecurityFepBridgesTestBase {
protected:
    anomaly_fep_bridge_t* bridge = nullptr;
    nimcp_anomaly_detector_t detector = nullptr;

    void SetUp() override {
        SecurityFepBridgesTestBase::SetUp();
        nimcp_anomaly_config_t detector_config = nimcp_anomaly_detector_default_config();
        detector = nimcp_anomaly_detector_create(&detector_config);
        ASSERT_NE(detector, nullptr);
        anomaly_fep_config_t config;
        anomaly_fep_default_config(&config);
        bridge = anomaly_fep_create(&config, detector, fep);
    }

    void TearDown() override {
        if (bridge) {
            anomaly_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (detector) {
            nimcp_anomaly_detector_destroy(detector);
            detector = nullptr;
        }
        SecurityFepBridgesTestBase::TearDown();
    }
};

TEST_F(AnomalyDetectorFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AnomalyDetectorFepBridgeTest, CreateWithNullConfig) {
    /* NULL config uses defaults, bridge is created successfully */
    anomaly_fep_bridge_t* br = anomaly_fep_create(nullptr, detector, fep);
    EXPECT_NE(br, nullptr);
    if (br) anomaly_fep_destroy(br);
}

TEST_F(AnomalyDetectorFepBridgeTest, CreateWithNullDetector) {
    anomaly_fep_config_t config;
    anomaly_fep_default_config(&config);
    anomaly_fep_bridge_t* br = anomaly_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(AnomalyDetectorFepBridgeTest, CreateWithNullFep) {
    anomaly_fep_config_t config;
    anomaly_fep_default_config(&config);
    anomaly_fep_bridge_t* br = anomaly_fep_create(&config, detector, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(AnomalyDetectorFepBridgeTest, DestroyNull) {
    anomaly_fep_destroy(nullptr);
}

TEST_F(AnomalyDetectorFepBridgeTest, DefaultConfig) {
    anomaly_fep_config_t config;
    int ret = anomaly_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.anomaly_fe_threshold, 0.0f);
    EXPECT_TRUE(config.use_fep_scoring);
}

TEST_F(AnomalyDetectorFepBridgeTest, Update) {
    int ret = anomaly_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(AnomalyDetectorFepBridgeTest, GetEffects) {
    anomaly_fep_update(bridge);
    anomaly_fep_effects_t effects;
    int ret = anomaly_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(AnomalyDetectorFepBridgeTest, BioAsync) {
    /* Initial state should be disconnected */
    EXPECT_FALSE(anomaly_fep_is_bio_async_connected(bridge));
    /* Try to connect - may fail if bio-async router not available */
    anomaly_fep_connect_bio_async(bridge);
    /* Connection state depends on router availability - just verify disconnect works */
    anomaly_fep_disconnect_bio_async(bridge);
    EXPECT_FALSE(anomaly_fep_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Blood Brain Barrier FEP Bridge Tests
 * ============================================================================ */

class BloodBrainBarrierFepBridgeTest : public SecurityFepBridgesTestBase {
protected:
    bbb_fep_bridge_t* bridge = nullptr;
    bbb_system_t bbb = nullptr;

    void SetUp() override {
        SecurityFepBridgesTestBase::SetUp();
        bbb_config_t bbb_config = bbb_default_config();
        bbb = bbb_system_create(&bbb_config);
        ASSERT_NE(bbb, nullptr);
        bbb_fep_config_t config;
        bbb_fep_default_config(&config);
        bridge = bbb_fep_create(&config, bbb, fep);
    }

    void TearDown() override {
        if (bridge) {
            bbb_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }
        SecurityFepBridgesTestBase::TearDown();
    }
};

TEST_F(BloodBrainBarrierFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BloodBrainBarrierFepBridgeTest, CreateWithNullConfig) {
    /* NULL config uses defaults, bridge is created successfully */
    bbb_fep_bridge_t* br = bbb_fep_create(nullptr, bbb, fep);
    EXPECT_NE(br, nullptr);
    if (br) bbb_fep_destroy(br);
}

TEST_F(BloodBrainBarrierFepBridgeTest, CreateWithNullBbb) {
    bbb_fep_config_t config;
    bbb_fep_default_config(&config);
    bbb_fep_bridge_t* br = bbb_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BloodBrainBarrierFepBridgeTest, CreateWithNullFep) {
    bbb_fep_config_t config;
    bbb_fep_default_config(&config);
    bbb_fep_bridge_t* br = bbb_fep_create(&config, bbb, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BloodBrainBarrierFepBridgeTest, DestroyNull) {
    bbb_fep_destroy(nullptr);
}

TEST_F(BloodBrainBarrierFepBridgeTest, DefaultConfig) {
    bbb_fep_config_t config;
    int ret = bbb_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.threat_free_energy_threshold, 0.0f);
}

TEST_F(BloodBrainBarrierFepBridgeTest, Update) {
    int ret = bbb_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BloodBrainBarrierFepBridgeTest, GetEffects) {
    bbb_fep_update(bridge);
    bbb_fep_effects_t effects;
    int ret = bbb_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(BloodBrainBarrierFepBridgeTest, BioAsync) {
    /* Initial state should be disconnected */
    EXPECT_FALSE(bbb_fep_is_bio_async_connected(bridge));
    /* Try to connect - may fail if bio-async router not available */
    bbb_fep_connect_bio_async(bridge);
    /* Connection state depends on router availability - just verify disconnect works */
    bbb_fep_disconnect_bio_async(bridge);
    EXPECT_FALSE(bbb_fep_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Pattern DB FEP Bridge Tests
 * ============================================================================ */

class PatternDbFepBridgeTest : public SecurityFepBridgesTestBase {
protected:
    pattern_fep_bridge_t* bridge = nullptr;
    nimcp_pattern_db_t pattern_db = nullptr;

    void SetUp() override {
        SecurityFepBridgesTestBase::SetUp();
        nimcp_pattern_db_config_t db_config = nimcp_pattern_db_default_config();
        pattern_db = nimcp_pattern_db_create(&db_config);
        ASSERT_NE(pattern_db, nullptr);
        pattern_fep_config_t config;
        pattern_fep_default_config(&config);
        bridge = pattern_fep_create(&config, pattern_db, fep);
    }

    void TearDown() override {
        if (bridge) {
            pattern_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (pattern_db) {
            nimcp_pattern_db_destroy(pattern_db);
            pattern_db = nullptr;
        }
        SecurityFepBridgesTestBase::TearDown();
    }
};

TEST_F(PatternDbFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PatternDbFepBridgeTest, CreateWithNullConfig) {
    /* NULL config uses defaults, bridge is created successfully */
    pattern_fep_bridge_t* br = pattern_fep_create(nullptr, pattern_db, fep);
    EXPECT_NE(br, nullptr);
    if (br) pattern_fep_destroy(br);
}

TEST_F(PatternDbFepBridgeTest, CreateWithNullPatternDb) {
    pattern_fep_config_t config;
    pattern_fep_default_config(&config);
    pattern_fep_bridge_t* br = pattern_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PatternDbFepBridgeTest, CreateWithNullFep) {
    pattern_fep_config_t config;
    pattern_fep_default_config(&config);
    pattern_fep_bridge_t* br = pattern_fep_create(&config, pattern_db, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PatternDbFepBridgeTest, Update) {
    int ret = pattern_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PatternDbFepBridgeTest, GetEffects) {
    pattern_fep_update(bridge);
    pattern_fep_effects_t effects;
    int ret = pattern_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Rate Limiter FEP Bridge Tests
 * ============================================================================ */

class RateLimiterFepBridgeTest : public SecurityFepBridgesTestBase {
protected:
    rate_fep_bridge_t* bridge = nullptr;
    nimcp_rate_limiter_t limiter = nullptr;

    void SetUp() override {
        SecurityFepBridgesTestBase::SetUp();
        nimcp_rate_limit_config_t limiter_config = nimcp_rate_limiter_default_config();
        limiter = nimcp_rate_limiter_create(&limiter_config);
        ASSERT_NE(limiter, nullptr);
        rate_fep_config_t config;
        rate_fep_default_config(&config);
        bridge = rate_fep_create(&config, limiter, fep);
    }

    void TearDown() override {
        if (bridge) {
            rate_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (limiter) {
            nimcp_rate_limiter_destroy(limiter);
            limiter = nullptr;
        }
        SecurityFepBridgesTestBase::TearDown();
    }
};

TEST_F(RateLimiterFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(RateLimiterFepBridgeTest, CreateWithNullConfig) {
    /* NULL config uses defaults, bridge is created successfully */
    rate_fep_bridge_t* br = rate_fep_create(nullptr, limiter, fep);
    EXPECT_NE(br, nullptr);
    if (br) rate_fep_destroy(br);
}

TEST_F(RateLimiterFepBridgeTest, CreateWithNullLimiter) {
    rate_fep_config_t config;
    rate_fep_default_config(&config);
    rate_fep_bridge_t* br = rate_fep_create(&config, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(RateLimiterFepBridgeTest, CreateWithNullFep) {
    rate_fep_config_t config;
    rate_fep_default_config(&config);
    rate_fep_bridge_t* br = rate_fep_create(&config, limiter, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(RateLimiterFepBridgeTest, Update) {
    int ret = rate_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(RateLimiterFepBridgeTest, GetEffects) {
    rate_fep_update(bridge);
    rate_fep_effects_t effects;
    int ret = rate_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Security FEP Bridge Tests
 * ============================================================================ */

class SecurityFepBridgeTest : public SecurityFepBridgesTestBase {
protected:
    security_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        SecurityFepBridgesTestBase::SetUp();
        security_fep_config_t config;
        security_fep_default_config(&config);
        bridge = security_fep_create(&config, fep);
    }

    void TearDown() override {
        if (bridge) {
            security_fep_destroy(bridge);
            bridge = nullptr;
        }
        SecurityFepBridgesTestBase::TearDown();
    }
};

TEST_F(SecurityFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SecurityFepBridgeTest, CreateWithNullConfig) {
    /* NULL config uses defaults, bridge is created successfully */
    security_fep_bridge_t* br = security_fep_create(nullptr, fep);
    EXPECT_NE(br, nullptr);
    if (br) security_fep_destroy(br);
}

TEST_F(SecurityFepBridgeTest, CreateWithNullFep) {
    security_fep_config_t config;
    security_fep_default_config(&config);
    security_fep_bridge_t* br = security_fep_create(&config, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SecurityFepBridgeTest, Update) {
    int ret = security_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityFepBridgeTest, GetEffects) {
    security_fep_update(bridge);
    security_fep_effects_t effects;
    int ret = security_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityFepBridgeTest, GetStats) {
    security_fep_stats_t stats;
    int ret = security_fep_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityFepBridgeTest, BioAsync) {
    /* Initial state should be disconnected */
    EXPECT_FALSE(security_fep_is_bio_async_connected(bridge));
    /* Try to connect - may fail if bio-async router not available */
    security_fep_connect_bio_async(bridge);
    /* Connection state depends on router availability - just verify disconnect works */
    security_fep_disconnect_bio_async(bridge);
    EXPECT_FALSE(security_fep_is_bio_async_connected(bridge));
}

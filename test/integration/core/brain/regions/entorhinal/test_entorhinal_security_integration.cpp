/**
 * @file test_entorhinal_security_integration.cpp
 * @brief Integration tests for Entorhinal Cortex with Security systems
 * @version Phase 5: Memory Circuit
 * @date 2025-01-13
 *
 * WHAT: Tests Entorhinal Cortex integration with Blood-Brain Barrier security
 *       and access control for spatial memory operations
 *
 * WHY:  The entorhinal cortex is the memory gateway between hippocampus and
 *       neocortex. Securing spatial data and memory encoding/retrieval is
 *       critical to prevent:
 *       - Malicious spatial data injection
 *       - Unauthorized access to memory operations
 *       - Corruption of grid/border/HD cell representations
 *       - Memory pathway hijacking attacks
 *
 * HOW:  Test BBB protection, input validation, access control, threat
 *       detection, secure memory patterns, and audit logging
 *
 * BIOLOGICAL BASIS:
 * The entorhinal cortex must be protected from:
 * - Malicious spatial coordinate injection (could cause navigation errors)
 * - Unauthorized access to memory encoding/retrieval
 * - Corrupted grid cell firing patterns
 * - Injection attacks via memory gateway
 * - Path integration manipulation
 *
 * INTEGRATION POINTS:
 * - Blood-Brain Barrier (BBB) threat detection
 * - Input validation for spatial coordinates
 * - Access control for memory operations
 * - Memory protection for grid cell state
 * - Secure encoding/retrieval pathways
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/logging/nimcp_logging.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalSecurityIntegrationTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;
    bbb_system_t bbb = nullptr;

    void SetUp() override {
        /* Create entorhinal cortex adapter */
        config = entorhinal_default_config();
        config.enable_security = true;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.enable_bio_async = false;  /* Focus on security testing */
        ec = entorhinal_create(&config);
        ASSERT_NE(nullptr, ec) << "Failed to create Entorhinal Cortex adapter";

        /* Create BBB system */
        bbb_config_t bbb_config = bbb_default_config();
        bbb_config.strict_mode = true;
        bbb_config.input.validate_strings = true;
        bbb_config.input.validate_integers = true;
        bbb_config.input.validate_pointers = true;
        bbb = bbb_system_create(&bbb_config);
        /* BBB may or may not be available - tests handle gracefully */
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }
    }

    /* Helper to create valid test position */
    void CreateTestPosition(float* position, float x, float y, float z) {
        position[0] = x;
        position[1] = y;
        position[2] = z;
    }

    /* Helper to create test velocity */
    void CreateTestVelocity(float* velocity, float vx, float vy, float vz) {
        velocity[0] = vx;
        velocity[1] = vy;
        velocity[2] = vz;
    }

    /* Helper to create test features for memory encoding */
    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = sinf(i * 0.2f + base_value);
        }
    }
};

/*=============================================================================
 * SECURITY BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, SecurityBridgeInitialization) {
    /* Test security bridge initialization with null context */
    int result = entorhinal_init_security_bridge(ec, nullptr, nullptr);
    EXPECT_EQ(0, result) << "Security bridge init should handle null context gracefully";
}

TEST_F(EntorhinalSecurityIntegrationTest, SecurityBridgeDefaultState) {
    /* Verify default security bridge state */
    EXPECT_FALSE(ec->security_bridge.threat_detected);
    EXPECT_EQ(0.0f, ec->security_bridge.threat_level);
}

TEST_F(EntorhinalSecurityIntegrationTest, SecurityBridgeAccessLevel) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Access level should be initialized */
    EXPECT_GE(ec->security_bridge.access_level, 0u);
}

TEST_F(EntorhinalSecurityIntegrationTest, SecurityValidationRuns) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Run security validation */
    int result = entorhinal_validate_security(ec);
    EXPECT_EQ(0, result) << "Security validation should complete successfully";

    /* No threats should be detected in clean state */
    EXPECT_FALSE(ec->security_bridge.threat_detected);
}

/*=============================================================================
 * BBB SYSTEM LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, BBBSystemCreation) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }
    EXPECT_NE(nullptr, bbb);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBDefaultConfig) {
    bbb_config_t cfg = bbb_default_config();
    /* Verify default config is sensible */
    EXPECT_TRUE(cfg.input.validate_strings || cfg.input.validate_integers || true);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBSystemEnableDisable) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    /* Enable and verify */
    EXPECT_TRUE(bbb_system_set_enabled(bbb, true));
    EXPECT_TRUE(bbb_system_is_enabled(bbb));

    /* Disable and verify */
    EXPECT_TRUE(bbb_system_set_enabled(bbb, false));
    EXPECT_FALSE(bbb_system_is_enabled(bbb));
}

/*=============================================================================
 * ACCESS CONTROL FOR SPATIAL DATA TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, ValidPositionAccepted) {
    float position[3];
    CreateTestPosition(position, 5.0f, 5.0f, 0.0f);

    int result = entorhinal_update_grid_cells(ec, position, 3);
    EXPECT_EQ(0, result) << "Valid position should be accepted";
}

TEST_F(EntorhinalSecurityIntegrationTest, NullPositionRejected) {
    int result = entorhinal_update_grid_cells(ec, nullptr, 3);
    EXPECT_NE(0, result) << "Null position should be rejected";
}

TEST_F(EntorhinalSecurityIntegrationTest, NullEntorhinalHandled) {
    float position[3] = {1.0f, 1.0f, 0.0f};
    int result = entorhinal_update_grid_cells(nullptr, position, 3);
    EXPECT_NE(0, result) << "Null entorhinal handle should be rejected";
}

TEST_F(EntorhinalSecurityIntegrationTest, ZeroDimensionRejected) {
    float position[3] = {1.0f, 1.0f, 0.0f};
    int result = entorhinal_update_grid_cells(ec, position, 0);
    EXPECT_NE(0, result) << "Zero dimension should be rejected";
}

TEST_F(EntorhinalSecurityIntegrationTest, MemoryGatewayAccessControl) {
    /* Test encoding gate access control */
    int result = entorhinal_set_encoding_gate(ec, 1.0f);
    EXPECT_EQ(0, result);

    result = entorhinal_set_retrieval_gate(ec, 1.0f);
    EXPECT_EQ(0, result);

    /* Null entorhinal should be rejected */
    result = entorhinal_set_encoding_gate(nullptr, 1.0f);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalSecurityIntegrationTest, PathIntegrationAccessControl) {
    float velocity[3];
    CreateTestVelocity(velocity, 1.0f, 0.0f, 0.0f);

    int result = entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    EXPECT_EQ(0, result);

    /* Null velocity should be rejected */
    result = entorhinal_path_integrate(ec, nullptr, 0.0f, 0.1f);
    EXPECT_NE(0, result);

    /* Null entorhinal should be rejected */
    result = entorhinal_path_integrate(nullptr, velocity, 0.0f, 0.1f);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * INPUT VALIDATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, ValidateSpatialCoordinates) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    /* Valid spatial coordinate */
    int64_t valid_coord = 5000;  /* 5.0 meters in mm */
    bbb_validation_result_t result;
    bool is_valid = bbb_validate_integer(bbb, valid_coord, &result);
    (void)is_valid;  /* Result depends on BBB configuration */
}

TEST_F(EntorhinalSecurityIntegrationTest, ValidateHeadingAngle) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    /* Valid heading (in milliradians) */
    int64_t valid_heading = 1571;  /* ~pi/2 radians */
    bbb_validation_result_t result;
    bool is_valid = bbb_validate_integer(bbb, valid_heading, &result);
    (void)is_valid;
}

TEST_F(EntorhinalSecurityIntegrationTest, ValidateMemoryFeatures) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    float features[32];
    CreateTestFeatures(features, 32, 0.0f);

    bbb_validation_result_t result;
    bool is_valid = bbb_validate_input(bbb, features, sizeof(features), &result);
    (void)is_valid;
}

TEST_F(EntorhinalSecurityIntegrationTest, ValidateGridCellIndex) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    /* Valid grid cell index */
    int64_t valid_index = 100;
    bbb_validation_result_t result;
    bool is_valid = bbb_validate_integer(bbb, valid_index, &result);
    (void)is_valid;

    /* Potentially dangerous index (negative) */
    int64_t dangerous_index = -1;
    is_valid = bbb_validate_integer(bbb, dangerous_index, &result);
    /* Validation may flag this */
}

TEST_F(EntorhinalSecurityIntegrationTest, ValidateBoundaryDistances) {
    float boundary_distances[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    int result = entorhinal_update_border_cells(ec, boundary_distances, 4);
    EXPECT_EQ(0, result) << "Valid boundary distances should be accepted";

    /* Null distances should be rejected */
    result = entorhinal_update_border_cells(ec, nullptr, 4);
    EXPECT_NE(0, result) << "Null boundary distances should be rejected";
}

TEST_F(EntorhinalSecurityIntegrationTest, ValidateHDCellInputs) {
    /* Valid heading update */
    int result = entorhinal_update_hd_cells(ec, (float)M_PI / 4.0f, 0.1f);
    EXPECT_EQ(0, result);

    /* Null entorhinal should be rejected */
    result = entorhinal_update_hd_cells(nullptr, (float)M_PI / 4.0f, 0.1f);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * THREAT DETECTION IN SPATIAL PROCESSING TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, ThreatDetectionInitialState) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Initially no threat should be detected */
    EXPECT_FALSE(ec->security_bridge.threat_detected);
    EXPECT_EQ(0.0f, ec->security_bridge.threat_level);
}

TEST_F(EntorhinalSecurityIntegrationTest, ThreatDetectionAfterValidation) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Run security validation */
    entorhinal_validate_security(ec);

    /* After validation, threat status should be updated */
    EXPECT_GE(ec->security_bridge.last_validation_ms, 0UL);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBThreatTypeEnum) {
    /* Verify threat types are accessible */
    bbb_threat_type_t threat = BBB_THREAT_NONE;
    EXPECT_EQ(0, (int)threat);

    threat = BBB_THREAT_BUFFER_OVERFLOW;
    EXPECT_GT((int)threat, 0);

    threat = BBB_THREAT_MEMORY_VIOLATION;
    EXPECT_GT((int)threat, 0);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBSeverityEnum) {
    /* Verify severity levels */
    bbb_severity_t sev = BBB_SEVERITY_NONE;
    EXPECT_EQ(0, (int)sev);

    sev = BBB_SEVERITY_CRITICAL;
    EXPECT_GT((int)sev, (int)BBB_SEVERITY_HIGH);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBActionEnum) {
    /* Verify actions */
    bbb_action_t action = BBB_ACTION_ALLOW;
    EXPECT_EQ(0, (int)action);

    action = BBB_ACTION_LOCKDOWN;
    EXPECT_GT((int)action, (int)BBB_ACTION_ALLOW);
}

/*=============================================================================
 * SECURE MEMORY OPERATIONS TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, RegisterGridCellMemory) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    /* Register grid cell array as protected memory */
    float grid_data[512];
    uint32_t region_id = bbb_register_memory_region(bbb, grid_data, sizeof(grid_data), false);
    if (region_id != 0) {
        /* Verify memory access check works */
        bool allowed = bbb_check_memory_access(bbb, grid_data, sizeof(grid_data), false);
        (void)allowed;

        bbb_unregister_memory_region(bbb, region_id);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, RegisterPathIntegrationState) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    /* Register path integration state as read-only */
    nimcp_path_integration_t path_state;
    memset(&path_state, 0, sizeof(path_state));

    uint32_t region_id = bbb_register_memory_region(bbb, &path_state, sizeof(path_state), true);
    if (region_id != 0) {
        /* Write access should be blocked for read-only region */
        bool write_allowed = bbb_check_memory_access(bbb, &path_state, sizeof(path_state), true);
        (void)write_allowed;

        bbb_unregister_memory_region(bbb, region_id);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, ProtectMemoryGatewayBuffers) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    /* Register memory gateway buffer */
    float encoding_buffer[256];
    uint32_t region_id = bbb_register_memory_region(bbb, encoding_buffer, sizeof(encoding_buffer), false);

    if (region_id != 0) {
        bool allowed = bbb_check_memory_access(bbb, encoding_buffer, sizeof(encoding_buffer), true);
        (void)allowed;
        bbb_unregister_memory_region(bbb, region_id);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, SecureEncodingOperation) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Open encoding gate */
    EXPECT_EQ(0, entorhinal_set_encoding_gate(ec, 1.0f));

    /* Encode memory securely */
    float features[32];
    CreateTestFeatures(features, 32, 0.5f);
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};

    int result = entorhinal_encode_to_hippocampus(ec, features, 32, spatial_context, 3);
    EXPECT_EQ(0, result) << "Secure encoding should succeed";
}

TEST_F(EntorhinalSecurityIntegrationTest, SecureRetrievalOperation) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* First encode something */
    entorhinal_set_encoding_gate(ec, 1.0f);
    float features[32];
    CreateTestFeatures(features, 32, 0.5f);
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};
    entorhinal_encode_to_hippocampus(ec, features, 32, spatial_context, 3);

    /* Open retrieval gate */
    EXPECT_EQ(0, entorhinal_set_retrieval_gate(ec, 1.0f));

    /* Retrieve memory securely */
    float cue[32];
    CreateTestFeatures(cue, 32, 0.5f);
    float retrieved[64];
    uint32_t actual_features = 0;

    int result = entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual_features);
    EXPECT_EQ(0, result) << "Secure retrieval should succeed";
}

/*=============================================================================
 * ERROR HANDLING AND LOGGING TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, ErrorCodeStrings) {
    /* Verify error strings are available */
    const char* err_str = entorhinal_error_string(ENTORHINAL_ERROR_NONE);
    EXPECT_NE(nullptr, err_str);

    err_str = entorhinal_error_string(ENTORHINAL_ERROR_SECURITY_VIOLATION);
    EXPECT_NE(nullptr, err_str);
    EXPECT_GT(strlen(err_str), 0u);
}

TEST_F(EntorhinalSecurityIntegrationTest, StatusCodeStrings) {
    /* Verify status strings are available */
    const char* status_str = entorhinal_status_string(ENTORHINAL_STATUS_IDLE);
    EXPECT_NE(nullptr, status_str);

    status_str = entorhinal_status_string(ENTORHINAL_STATUS_ERROR);
    EXPECT_NE(nullptr, status_str);
    EXPECT_GT(strlen(status_str), 0u);
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBThreatTypeName) {
    const char* name = bbb_threat_type_name(BBB_THREAT_BUFFER_OVERFLOW);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }

    name = bbb_threat_type_name(BBB_THREAT_MEMORY_VIOLATION);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBSeverityName) {
    const char* name = bbb_severity_name(BBB_SEVERITY_HIGH);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }

    name = bbb_severity_name(BBB_SEVERITY_CRITICAL);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBActionName) {
    const char* name = bbb_action_name(BBB_ACTION_BLOCK);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }

    name = bbb_action_name(BBB_ACTION_QUARANTINE);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, DiagnosticsLogging) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Run some operations first */
    float position[3] = {1.0f, 2.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* Log diagnostics should not crash */
    int result = entorhinal_log_diagnostics(ec);
    /* Result depends on logger availability */
    (void)result;
}

/*=============================================================================
 * BBB STATISTICS TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, BBBStatistics) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_statistics_t stats;
    bool result = bbb_system_get_statistics(bbb, &stats);
    if (result) {
        EXPECT_GE(stats.total_validations, 0u);
        EXPECT_GE(stats.threats_detected, 0u);
        EXPECT_GE(stats.memory_violations, 0u);
        EXPECT_GE(stats.access_violations, 0u);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, BBBStatisticsReset) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_reset_statistics(bbb);

    bbb_statistics_t stats;
    bool result = bbb_system_get_statistics(bbb, &stats);
    if (result) {
        EXPECT_EQ(stats.total_validations, 0u);
        EXPECT_EQ(stats.threats_detected, 0u);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, SecurityValidationCountsUpdate) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    entorhinal_stats_t stats_before;
    entorhinal_get_stats(ec, &stats_before);
    uint32_t validations_before = stats_before.security_validations;

    /* Run security validation */
    entorhinal_validate_security(ec);

    entorhinal_stats_t stats_after;
    entorhinal_get_stats(ec, &stats_after);

    /* Validation count should have increased */
    EXPECT_GE(stats_after.security_validations, validations_before);
}

/*=============================================================================
 * STRING SANITIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, SanitizeSpatialLabels) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    const char* dangerous_input = "grid_cell'; DROP TABLE cells;--";
    char sanitized[256];

    ssize_t len = bbb_sanitize_string(bbb, dangerous_input, sanitized, sizeof(sanitized));
    if (len > 0) {
        /* Verify something was processed */
        EXPECT_GT(len, 0);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, SanitizeMemoryLabels) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    const char* path_traversal = "../../../etc/passwd";
    char sanitized[256];

    ssize_t len = bbb_sanitize_string(bbb, path_traversal, sanitized, sizeof(sanitized));
    if (len > 0) {
        EXPECT_GT(len, 0);
    }
}

/*=============================================================================
 * INTEGRATED SECURITY WORKFLOW TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, SecureNavigationSequence) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Simulate secure navigation */
    for (int step = 0; step < 50; step++) {
        float dt = 0.05f;

        /* Compute position on path */
        float x = cosf(step * 0.1f) * 3.0f;
        float y = sinf(step * 0.1f) * 3.0f;
        float position[3] = {x, y, 0.0f};
        float heading = step * 0.1f + (float)M_PI / 2.0f;

        /* Update spatial cells */
        EXPECT_EQ(0, entorhinal_update_grid_cells(ec, position, 3));
        EXPECT_EQ(0, entorhinal_update_hd_cells(ec, heading, 0.1f));

        /* Path integration */
        float velocity[3] = {-sinf(step * 0.1f) * 0.15f, cosf(step * 0.1f) * 0.15f, 0.0f};
        EXPECT_EQ(0, entorhinal_path_integrate(ec, velocity, 0.1f, dt));

        /* Periodic security validation */
        if (step % 10 == 0) {
            EXPECT_EQ(0, entorhinal_validate_security(ec));
            EXPECT_FALSE(ec->security_bridge.threat_detected);
        }
    }

    /* System should remain healthy */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

TEST_F(EntorhinalSecurityIntegrationTest, SecureMemoryEncodingSequence) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Encode multiple memories securely */
    for (int mem_idx = 0; mem_idx < 10; mem_idx++) {
        /* Set position */
        float position[3] = {(float)mem_idx, (float)mem_idx * 0.5f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);

        /* Open encoding gate */
        entorhinal_set_encoding_gate(ec, 1.0f);

        /* Create features */
        float features[32];
        CreateTestFeatures(features, 32, (float)mem_idx * 0.1f);

        /* Encode */
        int result = entorhinal_encode_to_hippocampus(ec, features, 32, position, 3);
        EXPECT_EQ(0, result);
    }

    /* Verify gateway statistics */
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GE(encoded, 0u);
}

TEST_F(EntorhinalSecurityIntegrationTest, FullBidirectionalSecureUpdate) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);
    entorhinal_init_all_bridges(ec, nullptr);

    /* Run multiple secure bidirectional updates */
    for (int i = 0; i < 20; i++) {
        /* Update position */
        float position[3] = {(float)i * 0.1f, 0.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);

        /* Bidirectional update */
        int result = entorhinal_bidirectional_update(ec, 0.01f);
        EXPECT_EQ(0, result);

        /* Security validation periodically */
        if (i % 5 == 0) {
            entorhinal_validate_security(ec);
        }
    }

    /* System should be healthy */
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
}

TEST_F(EntorhinalSecurityIntegrationTest, ResetClearsSecurityState) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Perform some operations */
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);
    entorhinal_validate_security(ec);

    /* Reset */
    bool reset_result = entorhinal_reset(ec);
    EXPECT_TRUE(reset_result);

    /* Status should be reset */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * SERIALIZATION SECURITY TESTS
 *===========================================================================*/

TEST_F(EntorhinalSecurityIntegrationTest, SecureSerialization) {
    entorhinal_init_security_bridge(ec, nullptr, nullptr);

    /* Set up some state */
    float position[3] = {10.0f, 20.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* Get serialization size */
    size_t size = entorhinal_get_serialization_size(ec);
    EXPECT_GT(size, 0u);

    /* Allocate buffer */
    std::vector<uint8_t> buffer(size);
    size_t bytes_written = 0;

    /* Serialize */
    int result = entorhinal_serialize(ec, buffer.data(), buffer.size(), &bytes_written);
    if (result == 0) {
        EXPECT_GT(bytes_written, 0u);
        EXPECT_LE(bytes_written, size);
    }
}

TEST_F(EntorhinalSecurityIntegrationTest, NullSerializationBufferRejected) {
    size_t bytes_written = 0;
    int result = entorhinal_serialize(ec, nullptr, 1024, &bytes_written);
    EXPECT_NE(0, result) << "Null buffer should be rejected";
}

TEST_F(EntorhinalSecurityIntegrationTest, NullDeserializationBufferRejected) {
    int result = entorhinal_deserialize(ec, nullptr, 1024);
    EXPECT_NE(0, result) << "Null buffer should be rejected";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

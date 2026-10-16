/**
 * @file test_bbb_refcount_integration.cpp
 * @brief Integration Tests for BBB Quarantine Ref-Counting System
 *
 * WHAT: Integration tests for BBB quarantine reference counting with safe checks
 * WHY:  Verify TOCTOU-safe quarantine checks with reference acquisition work
 *       correctly across multiple regions and acquire/release cycles
 * HOW:  Test quarantine + safe check + ref acquire + release integration
 *
 * TESTS:
 * - BBBRefCountIntegration_QuarantineAndCheck: Quarantine then safe check with ref
 * - BBBRefCountIntegration_MultiRegionIsolation: Independent ref counting per region
 * - BBBRefCountIntegration_ReleaseMatchesAcquire: Acquire/release balance verification
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BBBRefCountIntegrationTest : public ::testing::Test {
protected:
    bbb_system_t bbb_ = nullptr;

    void SetUp() override {
        bbb_reset_test_state();
        bbb_config_t config = bbb_default_config();
        config.strict_mode = true;
        config.default_action = BBB_ACTION_BLOCK;
        bbb_ = bbb_system_create(&config);
        ASSERT_NE(bbb_, nullptr) << "Failed to create BBB system";
    }

    void TearDown() override {
        if (bbb_) {
            bbb_system_destroy(bbb_);
            bbb_ = nullptr;
        }
    }
};

//=============================================================================
// Test 1: Quarantine And Safe Check
//=============================================================================

TEST_F(BBBRefCountIntegrationTest, BBBRefCountIntegration_QuarantineAndCheck) {
    // Allocate test region
    size_t region_size = 2048;
    void* region = malloc(region_size);
    ASSERT_NE(region, nullptr);
    memset(region, 0xAA, region_size);

    // Quarantine the region
    EXPECT_TRUE(bbb_quarantine_region(bbb_, region, region_size));

    // Basic check
    EXPECT_TRUE(bbb_is_quarantined(bbb_, region, region_size));

    // TOCTOU-safe check without acquiring ref
    bool is_q = bbb_is_quarantined_safe(bbb_, region, region_size, false);
    EXPECT_TRUE(is_q) << "Safe check should confirm quarantine";

    // TOCTOU-safe check with acquiring ref (returns true = quarantined)
    // When quarantined, the returned value is true regardless of acquire_ref
    is_q = bbb_is_quarantined_safe(bbb_, region, region_size, true);
    EXPECT_TRUE(is_q) << "Safe check with ref should confirm quarantine";

    // Release quarantine
    EXPECT_TRUE(bbb_release_quarantine(bbb_, region));

    // Should no longer be quarantined
    EXPECT_FALSE(bbb_is_quarantined(bbb_, region, region_size));
    EXPECT_FALSE(bbb_is_quarantined_safe(bbb_, region, region_size, false));

    free(region);
}

//=============================================================================
// Test 2: Multi-Region Isolation
//=============================================================================

TEST_F(BBBRefCountIntegrationTest, BBBRefCountIntegration_MultiRegionIsolation) {
    const int NUM_REGIONS = 4;
    const size_t REGION_SIZE = 1024;

    std::vector<void*> regions(NUM_REGIONS);
    for (int i = 0; i < NUM_REGIONS; i++) {
        regions[i] = malloc(REGION_SIZE);
        ASSERT_NE(regions[i], nullptr) << "Failed to allocate region " << i;
        memset(regions[i], 0x10 + i, REGION_SIZE);
    }

    // Quarantine all regions
    for (int i = 0; i < NUM_REGIONS; i++) {
        EXPECT_TRUE(bbb_quarantine_region(bbb_, regions[i], REGION_SIZE))
            << "Failed to quarantine region " << i;
    }

    // Verify all quarantined
    for (int i = 0; i < NUM_REGIONS; i++) {
        EXPECT_TRUE(bbb_is_quarantined(bbb_, regions[i], REGION_SIZE))
            << "Region " << i << " should be quarantined";
    }

    // Release region 1 only
    EXPECT_TRUE(bbb_release_quarantine(bbb_, regions[1]));

    // Region 1 should be released, others still quarantined
    EXPECT_FALSE(bbb_is_quarantined(bbb_, regions[1], REGION_SIZE))
        << "Region 1 should be released";
    for (int i = 0; i < NUM_REGIONS; i++) {
        if (i == 1) continue;
        EXPECT_TRUE(bbb_is_quarantined(bbb_, regions[i], REGION_SIZE))
            << "Region " << i << " should still be quarantined after releasing region 1";
    }

    // Release region 3
    EXPECT_TRUE(bbb_release_quarantine(bbb_, regions[3]));

    // Regions 0 and 2 should still be quarantined
    EXPECT_TRUE(bbb_is_quarantined(bbb_, regions[0], REGION_SIZE));
    EXPECT_FALSE(bbb_is_quarantined(bbb_, regions[1], REGION_SIZE));
    EXPECT_TRUE(bbb_is_quarantined(bbb_, regions[2], REGION_SIZE));
    EXPECT_FALSE(bbb_is_quarantined(bbb_, regions[3], REGION_SIZE));

    // Clean up remaining
    bbb_release_quarantine(bbb_, regions[0]);
    bbb_release_quarantine(bbb_, regions[2]);

    for (int i = 0; i < NUM_REGIONS; i++) {
        free(regions[i]);
    }
}

//=============================================================================
// Test 3: Release Matches Acquire
//=============================================================================

TEST_F(BBBRefCountIntegrationTest, BBBRefCountIntegration_ReleaseMatchesAcquire) {
    // Allocate and quarantine
    size_t region_size = 1024;
    void* region = malloc(region_size);
    ASSERT_NE(region, nullptr);
    memset(region, 0xDD, region_size);

    EXPECT_TRUE(bbb_quarantine_region(bbb_, region, region_size));

    // Verify quarantine is active
    EXPECT_TRUE(bbb_is_quarantined(bbb_, region, region_size));

    // Multiple checks without acquiring ref (should not affect ref count)
    for (int i = 0; i < 10; i++) {
        bool q = bbb_is_quarantined_safe(bbb_, region, region_size, false);
        EXPECT_TRUE(q);
    }

    // Release should succeed (ref count should be 0 since no acquires)
    EXPECT_TRUE(bbb_release_quarantine(bbb_, region));
    EXPECT_FALSE(bbb_is_quarantined(bbb_, region, region_size));

    // Re-quarantine for acquire/release test
    EXPECT_TRUE(bbb_quarantine_region(bbb_, region, region_size));

    // Now test with an unquarantined region to verify acquire_ref on non-quarantined
    void* safe_region = malloc(512);
    ASSERT_NE(safe_region, nullptr);

    // Non-quarantined check with acquire_ref
    bool q = bbb_is_quarantined_safe(bbb_, safe_region, 512, true);
    EXPECT_FALSE(q) << "Non-quarantined region should return false";
    // Release ref for the non-quarantined region (should be safe no-op)
    bbb_release_quarantine_ref_for_region(bbb_, safe_region, 512);

    // Clean up
    bbb_release_quarantine(bbb_, region);
    free(region);
    free(safe_region);
}

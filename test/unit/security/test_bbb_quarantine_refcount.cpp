/**
 * @file test_bbb_quarantine_refcount.cpp
 * @brief Unit tests for BBB quarantine reference counting fixes (P0-1, P0-2, P0-3, P1-1)
 *
 * WHAT: Verify correct ref_count behavior after P0/P1 security fixes
 * WHY:  The original code had TOCTOU ref count over-increment, mismatched
 *       inc/dec, underflow race, and false-positive exception bugs
 * HOW:  GTest test cases exercising quarantine ref_count operations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

extern "C" {
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BBBQuarantineRefCountTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bbb_reset_test_state();
        config_ = bbb_default_config();
        system_ = bbb_system_create(&config_);
        ASSERT_NE(system_, nullptr);
    }

    void TearDown() override
    {
        if (system_) {
            bbb_system_destroy(system_);
            system_ = nullptr;
        }
    }

    bbb_config_t config_;
    bbb_system_t system_;
};

//=============================================================================
// P0-1: Acquire only increments matching region
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, AcquireOnlyIncrementsMatchingRegion)
{
    /* WHAT: Quarantine region A, verify acquiring ref for region B
     *       does NOT increment A's ref_count
     * WHY:  P0-1 fix - old code incremented ALL active quarantine entries */

    char region_a[256];
    char region_b[256];

    /* Quarantine region A */
    ASSERT_TRUE(bbb_quarantine_region(system_, region_a, sizeof(region_a)));

    /* Check region B with acquire_ref=true - region B is NOT quarantined */
    bool is_quarantined = bbb_is_quarantined_safe(system_, region_b, sizeof(region_b), true);
    EXPECT_FALSE(is_quarantined);

    /* Region A should still be releasable (ref_count should be 0) since
     * checking region B should NOT have incremented A's ref_count */
    bool released = bbb_release_quarantine(system_, region_a);
    EXPECT_TRUE(released) << "Region A should be releasable because checking region B "
                             "should not have incremented A's ref_count (P0-1 fix)";
}

//=============================================================================
// P0-2: Release only decrements matching region
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, ReleaseOnlyDecrementsMatchingRegion)
{
    /* WHAT: Quarantine two regions, release ref for one, verify other is unaffected
     * WHY:  P0-2 fix - old code decremented ALL active quarantine entries */

    char region_a[256];
    char region_b[256];

    /* Quarantine both regions */
    ASSERT_TRUE(bbb_quarantine_region(system_, region_a, sizeof(region_a)));
    ASSERT_TRUE(bbb_quarantine_region(system_, region_b, sizeof(region_b)));

    /* Acquire ref on region A by checking it (it IS quarantined, acquire_ref=true) */
    bool is_quarantined_a = bbb_is_quarantined_safe(system_, region_a, sizeof(region_a), true);
    EXPECT_TRUE(is_quarantined_a);

    /* Acquire ref on region B */
    bool is_quarantined_b = bbb_is_quarantined_safe(system_, region_b, sizeof(region_b), true);
    EXPECT_TRUE(is_quarantined_b);

    /* Release ref only for region A */
    bbb_release_quarantine_ref_for_region(system_, region_a, sizeof(region_a));

    /* Region A should now be releasable (ref_count back to 0) */
    bool released_a = bbb_release_quarantine(system_, region_a);
    EXPECT_TRUE(released_a) << "Region A should be releasable after targeted ref release";

    /* Region B should NOT be releasable (ref_count still > 0) */
    bool released_b = bbb_release_quarantine(system_, region_b);
    EXPECT_FALSE(released_b) << "Region B should NOT be releasable because its ref "
                                "was not released (P0-2 fix: only matching region decremented)";

    /* Clean up: release region B's ref so it can be cleaned up by destroy */
    bbb_release_quarantine_ref_for_region(system_, region_b, sizeof(region_b));
}

//=============================================================================
// P1-1: No underflow
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, NoUnderflow)
{
    /* WHAT: Release when ref_count is 0 should not go negative
     * WHY:  P1-1 fix - old code did atomic_fetch_sub before checking,
     *       creating a window where negative values were visible */

    char region[256];

    /* Quarantine region (ref_count starts at 0) */
    ASSERT_TRUE(bbb_quarantine_region(system_, region, sizeof(region)));

    /* Try to release a ref that was never acquired - should not underflow */
    bbb_release_quarantine_ref_for_region(system_, region, sizeof(region));

    /* Region should still be releasable (ref_count should be 0, not negative) */
    bool released = bbb_release_quarantine(system_, region);
    EXPECT_TRUE(released) << "Region should be releasable - ref_count should not "
                             "have gone negative (P1-1 underflow fix)";
}

//=============================================================================
// P0-3: Normal flow - no exception on non-quarantined check
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, NormalFlowNoException)
{
    /* WHAT: bbb_is_quarantined_safe returns false without throwing when
     *       region is not quarantined
     * WHY:  P0-3 fix - old code unconditionally threw NIMCP_THROW_TO_IMMUNE
     *       after the loop, treating "not quarantined" as an error */

    char region[256];

    /* Check a region that is NOT quarantined */
    bool is_quarantined = bbb_is_quarantined_safe(system_, region, sizeof(region), false);
    EXPECT_FALSE(is_quarantined) << "Non-quarantined region should return false";

    /* Also test the non-safe version */
    bool is_quarantined2 = bbb_is_quarantined(system_, region, sizeof(region));
    EXPECT_FALSE(is_quarantined2) << "Non-quarantined region should return false";

    /* If we get here without crashing/aborting, the exception was not thrown.
     * The test framework would catch any unexpected behavior. */
    SUCCEED();
}

//=============================================================================
// Acquire ref on overlap
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, AcquireRefOnOverlap)
{
    /* WHAT: When checking a quarantined region with acquire_ref=true,
     *       the matching region's ref_count should increment
     * WHY:  Verify the P0-1 fix correctly increments the matching region */

    char region[256];

    /* Quarantine the region */
    ASSERT_TRUE(bbb_quarantine_region(system_, region, sizeof(region)));

    /* Check with acquire_ref=true - region IS quarantined */
    bool is_quarantined = bbb_is_quarantined_safe(system_, region, sizeof(region), true);
    EXPECT_TRUE(is_quarantined);

    /* Now the region should have ref_count > 0, so release should fail */
    bool released = bbb_release_quarantine(system_, region);
    EXPECT_FALSE(released) << "Region should not be releasable while ref is held";

    /* Release the ref */
    bbb_release_quarantine_ref_for_region(system_, region, sizeof(region));

    /* Now release should succeed */
    released = bbb_release_quarantine(system_, region);
    EXPECT_TRUE(released) << "Region should be releasable after ref is released";
}

//=============================================================================
// Multiple regions maintain independent ref counts
//=============================================================================

TEST_F(BBBQuarantineRefCountTest, MultipleRegionsIndependent)
{
    /* WHAT: Multiple quarantine regions maintain independent ref counts
     * WHY:  Verify that ref_count operations on one region don't affect others */

    char region_a[128];
    char region_b[128];
    char region_c[128];

    /* Quarantine all three regions */
    ASSERT_TRUE(bbb_quarantine_region(system_, region_a, sizeof(region_a)));
    ASSERT_TRUE(bbb_quarantine_region(system_, region_b, sizeof(region_b)));
    ASSERT_TRUE(bbb_quarantine_region(system_, region_c, sizeof(region_c)));

    /* Acquire refs on A and C but not B */
    bool qa = bbb_is_quarantined_safe(system_, region_a, sizeof(region_a), true);
    EXPECT_TRUE(qa);
    bool qc = bbb_is_quarantined_safe(system_, region_c, sizeof(region_c), true);
    EXPECT_TRUE(qc);

    /* B should be releasable (no refs acquired) */
    bool released_b = bbb_release_quarantine(system_, region_b);
    EXPECT_TRUE(released_b) << "Region B should be releasable (no refs held)";

    /* A should NOT be releasable (ref held) */
    bool released_a = bbb_release_quarantine(system_, region_a);
    EXPECT_FALSE(released_a) << "Region A should NOT be releasable (ref held)";

    /* C should NOT be releasable (ref held) */
    bool released_c = bbb_release_quarantine(system_, region_c);
    EXPECT_FALSE(released_c) << "Region C should NOT be releasable (ref held)";

    /* Release A's ref, then release A */
    bbb_release_quarantine_ref_for_region(system_, region_a, sizeof(region_a));
    released_a = bbb_release_quarantine(system_, region_a);
    EXPECT_TRUE(released_a) << "Region A releasable after ref released";

    /* C should STILL not be releasable */
    released_c = bbb_release_quarantine(system_, region_c);
    EXPECT_FALSE(released_c) << "Region C still has ref held";

    /* Release C's ref, then release C */
    bbb_release_quarantine_ref_for_region(system_, region_c, sizeof(region_c));
    released_c = bbb_release_quarantine(system_, region_c);
    EXPECT_TRUE(released_c) << "Region C releasable after ref released";
}

}  // anonymous namespace

/**
 * @file test_wave8bc_region_ticks.cpp
 * @brief Unit tests for Wave 8B-c region tick drivers.
 *
 * Covers the 4 new tick drivers wired in Wave 8B-c:
 *   - brain_tick_hypothalamus  (drives/homeostasis)
 *   - brain_tick_entorhinal    (PARTIAL — no brain owner)
 *   - brain_tick_cerebellum    (motor-error / bio-msg drain)
 *   - brain_tick_basal_ganglia (action selection / reward gating)
 *
 * Verifies:
 *   - Each tick is NULL-safe (no crash on nullptr brain).
 *   - Each tick is no-op-safe on a freshly created brain whose region
 *     adapters may be NULL (validates internal null-guards).
 *   - Each new BRAIN_CYCLE_* enum has a name + default interval.
 *   - The 4 cycle types live within [0, BRAIN_CYCLE_COUNT).
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "core/brain/nimcp_brain_tick_hypothalamus.h"
#include "core/brain/nimcp_brain_tick_entorhinal.h"
#include "core/brain/nimcp_brain_tick_cerebellum.h"
#include "core/brain/nimcp_brain_tick_basal_ganglia.h"

class Wave8bcRegionTicksTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // KG-enabled tiny brain. Region adapters may or may not be created
        // depending on init mode — the tick drivers must null-guard either
        // way. We do NOT require the adapters to be non-null; we only
        // require the ticks to be safe in both cases.
        brain = brain_create("wave8bc_tick_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION,
                             /*inputs=*/8, /*outputs=*/4);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// -----------------------------------------------------------------------
// NULL-safety: each driver must tolerate a NULL brain.
// -----------------------------------------------------------------------

TEST(Wave8bcRegionTicksNull, HypothalamusNullBrain) {
    // Must not crash; no return value.
    brain_tick_hypothalamus(nullptr, 16.0f);
    SUCCEED();
}

TEST(Wave8bcRegionTicksNull, EntorhinalNullBrain) {
    brain_tick_entorhinal(nullptr, 16.0f);
    SUCCEED();
}

TEST(Wave8bcRegionTicksNull, CerebellumNullBrain) {
    brain_tick_cerebellum(nullptr, 16.0f);
    SUCCEED();
}

TEST(Wave8bcRegionTicksNull, BasalGangliaNullBrain) {
    brain_tick_basal_ganglia(nullptr, 16.0f);
    SUCCEED();
}

// -----------------------------------------------------------------------
// Real brain: each driver must be a safe no-op when the underlying region
// adapter is missing. The TINY init mode is allowed to leave any of these
// region adapters NULL — the tick driver must detect that and return.
// -----------------------------------------------------------------------

TEST_F(Wave8bcRegionTicksTest, HypothalamusTickNoCrashOnTinyBrain) {
    // Repeat to verify the call is idempotent.
    for (int i = 0; i < 4; ++i) {
        brain_tick_hypothalamus(brain, 16.0f);
    }
    SUCCEED();
}

TEST_F(Wave8bcRegionTicksTest, EntorhinalTickNoCrashOnTinyBrain) {
    for (int i = 0; i < 4; ++i) {
        brain_tick_entorhinal(brain, 16.0f);
    }
    SUCCEED();
}

TEST_F(Wave8bcRegionTicksTest, CerebellumTickNoCrashOnTinyBrain) {
    for (int i = 0; i < 4; ++i) {
        brain_tick_cerebellum(brain, 16.0f);
    }
    SUCCEED();
}

TEST_F(Wave8bcRegionTicksTest, BasalGangliaTickNoCrashOnTinyBrain) {
    for (int i = 0; i < 4; ++i) {
        brain_tick_basal_ganglia(brain, 16.0f);
    }
    SUCCEED();
}

// -----------------------------------------------------------------------
// Coordinator enum extension: each new BRAIN_CYCLE_* must have a name and
// a sensible default interval. Verifies the 3 switch statements in
// nimcp_brain_cycle_coordinator.c were extended for the new enum values.
// -----------------------------------------------------------------------

TEST(Wave8bcCycleEnumTest, NewEnumsHaveNames) {
    EXPECT_STREQ(brain_cycle_type_name(BRAIN_CYCLE_HYPOTHALAMUS),  "hypothalamus");
    EXPECT_STREQ(brain_cycle_type_name(BRAIN_CYCLE_ENTORHINAL),    "entorhinal");
    EXPECT_STREQ(brain_cycle_type_name(BRAIN_CYCLE_CEREBELLUM),    "cerebellum");
    EXPECT_STREQ(brain_cycle_type_name(BRAIN_CYCLE_BASAL_GANGLIA), "basal_ganglia");
}

TEST(Wave8bcCycleEnumTest, NewEnumsHaveDefaultIntervals) {
    // Hypothalamus is a slow drive at 100ms; the others are fast at 16ms.
    EXPECT_EQ(brain_cycle_get_default_interval_us(BRAIN_CYCLE_HYPOTHALAMUS),  100000ull);
    EXPECT_EQ(brain_cycle_get_default_interval_us(BRAIN_CYCLE_ENTORHINAL),     16000ull);
    EXPECT_EQ(brain_cycle_get_default_interval_us(BRAIN_CYCLE_CEREBELLUM),     16000ull);
    EXPECT_EQ(brain_cycle_get_default_interval_us(BRAIN_CYCLE_BASAL_GANGLIA),  16000ull);
}

TEST(Wave8bcCycleEnumTest, NewEnumsHaveCategories) {
    // Hypothalamus is MEDIUM (100ms = 1/10 sec); the others are FAST.
    EXPECT_EQ(brain_cycle_get_category(BRAIN_CYCLE_HYPOTHALAMUS),
              BRAIN_CYCLE_CATEGORY_MEDIUM);
    EXPECT_EQ(brain_cycle_get_category(BRAIN_CYCLE_ENTORHINAL),
              BRAIN_CYCLE_CATEGORY_FAST);
    EXPECT_EQ(brain_cycle_get_category(BRAIN_CYCLE_CEREBELLUM),
              BRAIN_CYCLE_CATEGORY_FAST);
    EXPECT_EQ(brain_cycle_get_category(BRAIN_CYCLE_BASAL_GANGLIA),
              BRAIN_CYCLE_CATEGORY_FAST);
}

TEST(Wave8bcCycleEnumTest, NewEnumsWithinCount) {
    EXPECT_LT(BRAIN_CYCLE_HYPOTHALAMUS,  BRAIN_CYCLE_COUNT);
    EXPECT_LT(BRAIN_CYCLE_ENTORHINAL,    BRAIN_CYCLE_COUNT);
    EXPECT_LT(BRAIN_CYCLE_CEREBELLUM,    BRAIN_CYCLE_COUNT);
    EXPECT_LT(BRAIN_CYCLE_BASAL_GANGLIA, BRAIN_CYCLE_COUNT);
}

// -----------------------------------------------------------------------
// Coordinator status: when a brain has its cycle coordinator enabled,
// the 4 new cycles should be queryable via get_status. We tolerate
// either "registered" or "unregistered" — TINY brains may skip the
// region_cycles init wave entirely. The contract under test is just
// that get_status accepts the new enums without error.
// -----------------------------------------------------------------------

TEST_F(Wave8bcRegionTicksTest, CoordinatorAcceptsNewEnumQueries) {
    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord) {
        GTEST_SKIP() << "TINY brain did not create cycle_coordinator";
    }

    brain_cycle_status_t status;
    // get_status returns 0 on success or -1 on lookup failure. Either is
    // acceptable here — we only verify the call does not crash on the new
    // enum values.
    (void)brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_HYPOTHALAMUS, &status);
    (void)brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_ENTORHINAL, &status);
    (void)brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_CEREBELLUM, &status);
    (void)brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_BASAL_GANGLIA, &status);
    SUCCEED();
}

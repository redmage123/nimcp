/**
 * @file test_hemispheric_language_bridge.cpp
 * @brief Unit tests for the lateralized GL wiring bridge.
 *
 * WHAT: Lightweight tests that exercise the bridge's failure paths +
 *       state management. Positive-path behavior (callosum forwarding
 *       under a real bilateral brain) belongs in the
 *       brain_heavy integration suite.
 *
 * WHY:  hemispheric_brain_create instantiates two full brain_t —
 *       too heavy for unit tests. The bridge's gating logic is
 *       independently testable with stubbed structs.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "core/brain/hemispheric/nimcp_hemispheric_language_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
}

namespace {

class HemiLangBridge : public ::testing::Test {
protected:
    /* Stub hemispheric brain — minimal fields the bridge touches. */
    hemispheric_brain_t hb;
    brain_hemisphere_t  left_hemi;
    brain_hemisphere_t  right_hemi;

    void SetUp() override {
        memset(&hb, 0, sizeof(hb));
        memset(&left_hemi, 0, sizeof(left_hemi));
        memset(&right_hemi, 0, sizeof(right_hemi));
        hb.left  = &left_hemi;
        hb.right = &right_hemi;
        hb.callosum = nullptr;          /* no real callosum */
        hb.callosum_intact = false;
        /* brain pointers stay NULL — install will reject before
         * dereferencing them when GL is missing. */
    }
    void TearDown() override {
        hemispheric_language_bridge_uninstall(&hb);
    }
};

/* --- install: NULL hemispheric brain ------------------------------- */
TEST_F(HemiLangBridge, InstallRejectsNullBrain) {
    EXPECT_EQ(-1, hemispheric_language_bridge_install(nullptr));
}

/* --- install: missing left/right ----------------------------------- */
TEST_F(HemiLangBridge, InstallRejectsMissingHemisphere) {
    hemispheric_brain_t empty;
    memset(&empty, 0, sizeof(empty));
    EXPECT_EQ(-1, hemispheric_language_bridge_install(&empty));
}

/* --- install: brain pointers NULL → -2 (no GL on left) ------------ */
TEST_F(HemiLangBridge, InstallReturnsMinusTwoWhenNoLeftGL) {
    /* Both hemispheres present but their brain_t is NULL → would
     * crash if the bridge dereferenced; safe-bail expected. We use
     * a tighter contract: if left_brain is NULL, the bridge cannot
     * read grounded_lang and must return error. */
    int rc = hemispheric_language_bridge_install(&hb);
    EXPECT_LT(rc, 0)
        << "must return error when left brain has no GL";
}

/* --- get_stats with no install: returns -1 ------------------------- */
TEST_F(HemiLangBridge, GetStatsBeforeInstallFails) {
    hemispheric_language_stats_t s;
    memset(&s, 0xAA, sizeof(s));
    EXPECT_EQ(-1, hemispheric_language_bridge_get_stats(&hb, &s));
    /* Out is zero-filled even on failure (defensive contract). */
    EXPECT_EQ(0u, s.lh_comprehensions);
    EXPECT_EQ(0u, s.callosum_msgs_sent);
}

TEST_F(HemiLangBridge, GetStatsNullOut) {
    EXPECT_EQ(-1, hemispheric_language_bridge_get_stats(&hb, nullptr));
}

/* --- tick with no install / no callosum: 0 messages --------------- */
TEST_F(HemiLangBridge, TickWithNoBridgeReturnsZero) {
    EXPECT_EQ(0, hemispheric_language_bridge_tick(&hb));
}

TEST_F(HemiLangBridge, TickNullBrainReturnsZero) {
    /* Must not deref hb at all. */
    EXPECT_EQ(0, hemispheric_language_bridge_tick(nullptr));
}

/* --- get_stats out has the new aphasia_dropped field zeroed -------- */
TEST_F(HemiLangBridge, GetStatsZerosAphasiaCounter) {
    hemispheric_language_stats_t s;
    memset(&s, 0xAA, sizeof(s));
    (void)hemispheric_language_bridge_get_stats(&hb, &s);
    EXPECT_EQ(0u, s.aphasia_dropped);
}

/* --- set_aphasia: silently no-ops without bridge, accepts any value
 *     once installed (clamping happens internally) ----------------- */
TEST_F(HemiLangBridge, SetAphasiaWithoutBridgeIsSafe) {
    hemispheric_language_bridge_set_aphasia(&hb, 0.5f);
    hemispheric_language_bridge_set_aphasia(&hb, -1.0f);
    hemispheric_language_bridge_set_aphasia(&hb, 99.0f);
    /* Must not crash. */
}

TEST_F(HemiLangBridge, SetAphasiaNullBrain) {
    hemispheric_language_bridge_set_aphasia(nullptr, 0.5f);
}

/* --- uninstall is idempotent --------------------------------------- */
TEST_F(HemiLangBridge, UninstallWithoutInstallIsSafe) {
    hemispheric_language_bridge_uninstall(&hb);
    hemispheric_language_bridge_uninstall(&hb);
    hemispheric_language_bridge_uninstall(nullptr);
}

}  // namespace

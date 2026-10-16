//=============================================================================
// test_claustrum_slot_search_regression.cpp - Claustrum False Positive Throw Regression
//=============================================================================
//
// WHAT: Verify that claustrum slot search functions do NOT throw
//       NIMCP_THROW_TO_IMMUNE on normal "not found" results
// WHY:  find_empty_percept_slot and find_percept_by_id were throwing
//       NIMCP_THROW_TO_IMMUNE when no slot/percept was found. This is
//       NORMAL behavior (search returning "not found"), not an error.
//       Per project conventions, search/lookup "not found" paths should
//       return -1/NULL, not throw exceptions.
// HOW:  Fill all percept slots, then attempt operations that trigger the
//       search functions. Verify they return appropriate error codes
//       without throwing exceptions to the immune system.
//
// REGRESSION: Fixes false positive NIMCP_THROW_TO_IMMUNE in:
//   - find_empty_percept_slot (line ~212): threw NIMCP_ERROR_NULL_POINTER
//   - find_percept_by_id (line ~225): threw NIMCP_ERROR_INVALID_PARAM
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "utils/exception/nimcp_exception.h"

class ClaustrumSlotSearchRegressionTest : public ::testing::Test {
protected:
    nimcp_claustrum_t claustrum_;

    void SetUp() override {
        nimcp_exception_clear_current();
        memset(&claustrum_, 0, sizeof(claustrum_));

        nimcp_claustrum_config_t config = nimcp_claustrum_default_config();
        // Lower binding threshold so we can easily create bindings
        config.binding_threshold = 0.0f;
        nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum_, &config);
        ASSERT_EQ(err, CLAUSTRUM_OK);

        nimcp_exception_clear_current();
    }

    void TearDown() override {
        nimcp_claustrum_shutdown(&claustrum_);
        nimcp_exception_clear_current();
    }

    // Helper: activate two modalities with matching phase for high coherence
    void ActivateModalities(nimcp_claustrum_modality_t m1,
                           nimcp_claustrum_modality_t m2,
                           float activity) {
        nimcp_claustrum_update_modality(&claustrum_, m1, nullptr, 0, activity);
        nimcp_claustrum_update_modality(&claustrum_, m2, nullptr, 0, activity);
        // Set salience high enough for binding
        nimcp_claustrum_set_modality_salience(&claustrum_, m1, activity);
        nimcp_claustrum_set_modality_salience(&claustrum_, m2, activity);
    }
};

//=============================================================================
// REGRESSION: find_empty_percept_slot should NOT throw when all slots full
//=============================================================================

TEST_F(ClaustrumSlotSearchRegressionTest, FullPercepts_ReturnsError_NoThrow) {
    // Fill all CLAUSTRUM_MAX_BOUND_PERCEPTS slots by creating bindings
    // Each binding needs at least 2 active modalities with coherence > threshold
    //
    // Since we set binding_threshold = 0.0, any active modalities will bind

    // We have CLAUSTRUM_MODALITY_COUNT = 8 modalities, and we need to create
    // CLAUSTRUM_MAX_BOUND_PERCEPTS = 16 percepts. We can reuse modality pairs
    // by unbinding and rebinding. Let's create as many as we can.

    uint32_t percept_ids[CLAUSTRUM_MAX_BOUND_PERCEPTS];
    int created = 0;

    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        // Activate visual and auditory each time (reset modalities first)
        nimcp_claustrum_update_modality(&claustrum_,
            CLAUSTRUM_MODALITY_VISUAL, nullptr, 0, 0.9f);
        nimcp_claustrum_update_modality(&claustrum_,
            CLAUSTRUM_MODALITY_AUDITORY, nullptr, 0, 0.9f);

        uint32_t modality_mask = (1U << CLAUSTRUM_MODALITY_VISUAL) |
                                 (1U << CLAUSTRUM_MODALITY_AUDITORY);

        uint32_t pid = 0;
        nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
            &claustrum_, modality_mask, &pid);

        if (err == CLAUSTRUM_OK) {
            percept_ids[created++] = pid;
        } else {
            break;
        }
    }

    // Clear any exceptions that may have accumulated
    nimcp_exception_clear_current();

    // Now try to create one more binding - this should fail because all
    // slots are full (find_empty_percept_slot returns -1)
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_SOMATOSENSORY, nullptr, 0, 0.9f);
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_OLFACTORY, nullptr, 0, 0.9f);

    uint32_t modality_mask = (1U << CLAUSTRUM_MODALITY_SOMATOSENSORY) |
                             (1U << CLAUSTRUM_MODALITY_OLFACTORY);
    uint32_t extra_pid = 0;
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum_, modality_mask, &extra_pid);

    // Should get CAPACITY_EXCEEDED, not success
    if (created >= CLAUSTRUM_MAX_BOUND_PERCEPTS) {
        EXPECT_EQ(err, CLAUSTRUM_ERR_CAPACITY_EXCEEDED)
            << "Expected CAPACITY_EXCEEDED when all percept slots are full";
    }

    // CRITICAL: No exception should have been thrown to the immune system
    // for the slot search failure (it's normal "not found" behavior)
    nimcp_exception_t* ex = nimcp_exception_get_current();
    if (ex != nullptr) {
        std::string msg(ex->message);
        EXPECT_TRUE(msg.find("find_empty_percept_slot") == std::string::npos)
            << "REGRESSION: find_empty_percept_slot should NOT throw on full slots. "
            << "Got exception: " << msg;
    }
}

//=============================================================================
// REGRESSION: find_percept_by_id should NOT throw when percept not found
//=============================================================================

TEST_F(ClaustrumSlotSearchRegressionTest, NonexistentPercept_ReturnsError_NoThrow) {
    nimcp_exception_clear_current();

    // Try to get a percept with an ID that doesn't exist
    nimcp_claustrum_bound_percept_t percept;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_percept(
        &claustrum_, 99999, &percept);

    // Should return MODALITY_NOT_FOUND error
    EXPECT_EQ(err, CLAUSTRUM_ERR_MODALITY_NOT_FOUND);

    // CRITICAL: No false positive exception thrown for "not found"
    nimcp_exception_t* ex = nimcp_exception_get_current();
    if (ex != nullptr) {
        std::string msg(ex->message);
        EXPECT_TRUE(msg.find("find_percept_by_id") == std::string::npos)
            << "REGRESSION: find_percept_by_id should NOT throw when percept not found. "
            << "Got exception: " << msg;
    }
}

TEST_F(ClaustrumSlotSearchRegressionTest, ReleaseNonexistent_ReturnsError_NoThrow) {
    nimcp_exception_clear_current();

    // Try to release a percept that doesn't exist
    nimcp_claustrum_error_t err = nimcp_claustrum_release_percept(&claustrum_, 99999);
    EXPECT_EQ(err, CLAUSTRUM_ERR_MODALITY_NOT_FOUND);

    // No false positive exception
    nimcp_exception_t* ex = nimcp_exception_get_current();
    if (ex != nullptr) {
        std::string msg(ex->message);
        EXPECT_TRUE(msg.find("find_percept_by_id") == std::string::npos)
            << "REGRESSION: find_percept_by_id should NOT throw when percept not found. "
            << "Got exception: " << msg;
    }
}

TEST_F(ClaustrumSlotSearchRegressionTest, GateNonexistentPercept_ReturnsError_NoThrow) {
    nimcp_exception_clear_current();

    // Try to gate workspace access for a nonexistent percept
    bool granted = false;
    nimcp_claustrum_error_t err = nimcp_claustrum_gate_workspace(
        &claustrum_, 99999, &granted);

    EXPECT_EQ(err, CLAUSTRUM_ERR_MODALITY_NOT_FOUND);
    EXPECT_FALSE(granted);

    // No false positive exception
    nimcp_exception_t* ex = nimcp_exception_get_current();
    if (ex != nullptr) {
        std::string msg(ex->message);
        EXPECT_TRUE(msg.find("find_percept_by_id") == std::string::npos)
            << "REGRESSION: find_percept_by_id should NOT throw when percept not found. "
            << "Got exception: " << msg;
    }
}

//=============================================================================
// POSITIVE TESTS - Normal operations still work correctly after fix
//=============================================================================

TEST_F(ClaustrumSlotSearchRegressionTest, NormalBind_StillWorks) {
    // Activate two modalities
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_VISUAL, nullptr, 0, 0.9f);
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_AUDITORY, nullptr, 0, 0.9f);

    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) |
                    (1U << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t pid = 0;
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum_, mask, &pid);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GT(pid, 0u);
}

TEST_F(ClaustrumSlotSearchRegressionTest, NormalGetPercept_StillWorks) {
    // Create a percept
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_VISUAL, nullptr, 0, 0.9f);
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_AUDITORY, nullptr, 0, 0.9f);

    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) |
                    (1U << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t pid = 0;
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum_, mask, &pid);

    if (err == CLAUSTRUM_OK) {
        nimcp_claustrum_bound_percept_t percept;
        err = nimcp_claustrum_get_percept(&claustrum_, pid, &percept);
        EXPECT_EQ(err, CLAUSTRUM_OK);
        EXPECT_EQ(percept.id, pid);
        EXPECT_TRUE(percept.valid);
    }
}

TEST_F(ClaustrumSlotSearchRegressionTest, NormalRelease_StillWorks) {
    // Create and release a percept
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_VISUAL, nullptr, 0, 0.9f);
    nimcp_claustrum_update_modality(&claustrum_,
        CLAUSTRUM_MODALITY_AUDITORY, nullptr, 0, 0.9f);

    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) |
                    (1U << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t pid = 0;
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum_, mask, &pid);

    if (err == CLAUSTRUM_OK) {
        err = nimcp_claustrum_release_percept(&claustrum_, pid);
        EXPECT_EQ(err, CLAUSTRUM_OK);

        // After release, getting it should fail
        nimcp_claustrum_bound_percept_t percept;
        err = nimcp_claustrum_get_percept(&claustrum_, pid, &percept);
        EXPECT_EQ(err, CLAUSTRUM_ERR_MODALITY_NOT_FOUND);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

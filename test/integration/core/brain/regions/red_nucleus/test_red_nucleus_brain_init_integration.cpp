/**
 * @file test_red_nucleus_brain_init_integration.cpp
 * @brief Integration tests for Red Nucleus brain initialization
 *
 * WHAT: Tests Red Nucleus integration with brain factory initialization
 * WHY:  Ensure proper lifecycle and motor coordination integration
 * HOW:  Test creation, configuration, motor commands, and error correction
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/red_nucleus/nimcp_red_nucleus.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class RedNucleusBrainInitTest : public ::testing::Test {
protected:
    nimcp_red_nucleus_t* rn;
    rn_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        rn = NULL;

        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        memset(&config, 0, sizeof(config));
        rn_default_config(&config);
    }

    void TearDown() override {
        if (rn) {
            rn_destroy(rn);
            rn = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper: create and initialize Red Nucleus in one step */
    nimcp_red_nucleus_t* createAndInitRN(const rn_config_t* cfg) {
        nimcp_red_nucleus_t* instance = rn_create(cfg);
        if (instance) {
            rn_init(instance);
        }
        return instance;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(RedNucleusBrainInitTest, CreateWithDefaultConfig) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    /* Red Nucleus uses two-phase init: create allocates, init sets up state */
    int init_result = rn_init(rn);
    EXPECT_EQ(0, init_result);
    EXPECT_TRUE(rn->initialized);
}

TEST_F(RedNucleusBrainInitTest, DestroyNull) {
    rn_destroy(NULL);
}

TEST_F(RedNucleusBrainInitTest, ResetAfterCreate) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    int result = rn_reset(rn);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(rn->initialized);
}

TEST_F(RedNucleusBrainInitTest, MultipleCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        rn = rn_create(&config);
        ASSERT_NE(nullptr, rn) << "Cycle " << i << " failed";
        rn_destroy(rn);
        rn = NULL;
    }
}

/*=============================================================================
 * MOTOR COMMAND TESTS
 *===========================================================================*/

TEST_F(RedNucleusBrainInitTest, IssueCommand) {
    rn = createAndInitRN(&config);
    ASSERT_NE(nullptr, rn);

    rn_motor_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.effector = RN_EFFECTOR_FORELIMB_PROXIMAL;
    cmd.magnitude = 0.5f;
    cmd.duration_ms = 100.0f;

    int result = rn_issue_command(rn, &cmd);
    EXPECT_EQ(0, result);
}

TEST_F(RedNucleusBrainInitTest, GetOutput) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    float output = rn_get_output(rn, RN_EFFECTOR_FORELIMB_PROXIMAL);
    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);
}

TEST_F(RedNucleusBrainInitTest, ClearCommands) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    int result = rn_clear_commands(rn);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * ERROR PROCESSING TESTS
 *===========================================================================*/

TEST_F(RedNucleusBrainInitTest, ProcessError) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    rn_motor_error_t error;
    memset(&error, 0, sizeof(error));
    error.type = RN_ERROR_POSITION;
    error.effector = RN_EFFECTOR_FORELIMB_PROXIMAL;
    error.error_magnitude = 0.3f;

    int result = rn_process_error(rn, &error);
    EXPECT_EQ(0, result);
}

TEST_F(RedNucleusBrainInitTest, GetLearningState) {
    rn = rn_create(&config);
    ASSERT_NE(nullptr, rn);

    rn_learning_state_t state;
    int result = rn_get_learning_state(rn, RN_EFFECTOR_FORELIMB_PROXIMAL, &state);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(RedNucleusBrainInitTest, UpdateCycle) {
    rn = createAndInitRN(&config);
    ASSERT_NE(nullptr, rn);

    for (int i = 0; i < 100; i++) {
        int result = rn_update(rn, 10.0f);
        EXPECT_EQ(0, result);
    }

    EXPECT_TRUE(rn->initialized);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

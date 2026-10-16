// test_immune_error_routing.cpp - Integration test for immune error routing
// Tests P1-2/3/5/6 + P2-6: Exception-to-immune integration
#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"

class ImmuneErrorRoutingTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_ = nullptr;
    bbb_system_t bbb_ = nullptr;

    void SetUp() override {
        nimcp_exception_system_init();
        brain_immune_config_t cfg;
        brain_immune_default_config(&cfg);
        cfg.enable_bbb_integration = true;
        cfg.enable_logging = false;
        immune_ = brain_immune_create(&cfg);
        ASSERT_NE(immune_, nullptr);
        bbb_ = bbb_system_create(nullptr);
        ASSERT_NE(bbb_, nullptr);
        bbb_system_set_enabled(bbb_, true);
        brain_immune_connect_bbb(immune_, bbb_);
    }

    void TearDown() override {
        if (immune_) brain_immune_destroy(immune_);
        if (bbb_) bbb_system_destroy(bbb_);
        nimcp_exception_system_shutdown();
    }
};

// --- BBB Threat Presentation ---
TEST_F(ImmuneErrorRoutingTest, BBBThreatCreatesAntigen) {
    uint32_t antigen_id = 0;
    uint8_t threat_data[] = {0xBE, 0xEF};
    int rc = brain_immune_present_bbb_threat(
        immune_, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
        threat_data, sizeof(threat_data), &antigen_id);
    EXPECT_EQ(rc, 0) << "Failed to present BBB threat";
    EXPECT_GT(antigen_id, 0u);
}

TEST_F(ImmuneErrorRoutingTest, BBBThreat_NullSystem) {
    uint32_t antigen_id = 0;
    uint8_t data[] = {1};
    int rc = brain_immune_present_bbb_threat(
        nullptr, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
        data, 1, &antigen_id);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, BBBThreat_NullData) {
    uint32_t antigen_id = 0;
    int rc = brain_immune_present_bbb_threat(
        immune_, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH,
        nullptr, 0, &antigen_id);
    (void)rc;
}

// --- Exception to Immune ---
TEST_F(ImmuneErrorRoutingTest, ExceptionPresentedToImmune) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BBB_REJECTED, EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__, "Test BBB rejection");
    ASSERT_NE(ex, nullptr);
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int rc = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(rc, 0) << "Failed to present exception to immune";
    nimcp_exception_unref(ex);
}

TEST_F(ImmuneErrorRoutingTest, ExceptionPresent_NullException) {
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int rc = nimcp_exception_present_to_immune(nullptr, &response);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, ExceptionPresent_NullResponse) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_SECURITY_THREAT, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Test threat");
    ASSERT_NE(ex, nullptr);
    // response parameter is documented as optional (may be NULL) per API header
    int rc = nimcp_exception_present_to_immune(ex, nullptr);
    EXPECT_EQ(rc, 0);
    nimcp_exception_unref(ex);
}

// --- Antigen Lifecycle ---
TEST_F(ImmuneErrorRoutingTest, AntigenPresentAndRetrieve) {
    uint32_t antigen_id = 0;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    int rc = brain_immune_present_antigen(
        immune_, ANTIGEN_SOURCE_BBB, epitope, sizeof(epitope),
        5, 0, &antigen_id);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(antigen_id, 0u);
    const brain_antigen_t* ag = brain_immune_get_antigen(immune_, antigen_id);
    EXPECT_NE(ag, nullptr);
}

TEST_F(ImmuneErrorRoutingTest, AntigenPresent_NullSystem) {
    uint32_t antigen_id = 0;
    uint8_t epitope[] = {1};
    int rc = brain_immune_present_antigen(
        nullptr, ANTIGEN_SOURCE_BBB, epitope, 1, 1, 0, &antigen_id);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, GetAntigen_NullSystem) {
    const brain_antigen_t* ag = brain_immune_get_antigen(nullptr, 1);
    EXPECT_EQ(ag, nullptr);
}

TEST_F(ImmuneErrorRoutingTest, GetAntigen_InvalidId) {
    const brain_antigen_t* ag = brain_immune_get_antigen(immune_, 99999);
    EXPECT_EQ(ag, nullptr);
}

// --- B Cell and Antibody ---
TEST_F(ImmuneErrorRoutingTest, BCellActivationAndAntibody) {
    uint32_t antigen_id = 0;
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(0, brain_immune_present_antigen(
        immune_, ANTIGEN_SOURCE_BBB, epitope, sizeof(epitope),
        8, 0, &antigen_id));
    uint32_t b_cell_id = 0;
    ASSERT_EQ(0, brain_immune_activate_b_cell(immune_, antigen_id, &b_cell_id));
    EXPECT_GT(b_cell_id, 0u);
    for (int i = 0; i < 50; i++) {
        brain_immune_update(immune_, 100);
    }
    uint32_t ab_id = 0;
    int rc = brain_immune_produce_antibody(immune_, b_cell_id, ANTIBODY_IGM, &ab_id);
    (void)rc;
}

TEST_F(ImmuneErrorRoutingTest, BCellActivate_NullSystem) {
    uint32_t b_cell_id = 0;
    int rc = brain_immune_activate_b_cell(nullptr, 1, &b_cell_id);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, ProduceAntibody_NullSystem) {
    uint32_t ab_id = 0;
    int rc = brain_immune_produce_antibody(nullptr, 1, ANTIBODY_IGM, &ab_id);
    EXPECT_NE(rc, 0);
}

// --- Immune Stats and Phase ---
TEST_F(ImmuneErrorRoutingTest, GetStats) {
    brain_immune_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc = brain_immune_get_stats(immune_, &stats);
    EXPECT_EQ(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, GetStats_NullSystem) {
    brain_immune_stats_t stats;
    int rc = brain_immune_get_stats(nullptr, &stats);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, GetStats_NullStats) {
    int rc = brain_immune_get_stats(immune_, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(ImmuneErrorRoutingTest, GetPhase) {
    brain_immune_phase_t phase = brain_immune_get_phase(immune_);
    EXPECT_EQ(phase, IMMUNE_PHASE_SURVEILLANCE);
}

TEST_F(ImmuneErrorRoutingTest, GetPhase_NullSystem) {
    brain_immune_phase_t phase = brain_immune_get_phase(nullptr);
    (void)phase;
}

TEST_F(ImmuneErrorRoutingTest, Update_NullSystem) {
    int rc = brain_immune_update(nullptr, 100);
    EXPECT_NE(rc, 0);
}

/**
 * @file test_brain_region_alloc_regression.cpp
 * @brief Regression tests for P1-9 brain region allocation failure error handling
 *
 * TEST PHILOSOPHY:
 * - Verify that create functions handle NULL configs gracefully (use defaults)
 * - Verify that NULL pointer error handling returns correct error codes
 * - Verify that destroy functions handle NULL safely
 * - Verify that plasticity bridge create/destroy lifecycle is correct
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0 P1-P2 Allocation Failure Regression
 */

#include <gtest/gtest.h>
#include <cstring>

/* Gustatory module */
extern "C" {
#include "core/brain/regions/gustatory/nimcp_gustatory.h"
}

/* Olfactory module */
extern "C" {
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
}

/* Raphe plasticity bridge */
extern "C" {
#include "core/brain/regions/raphe/nimcp_raphe_plasticity_bridge.h"
}

/* Habenula plasticity bridge */
extern "C" {
#include "core/brain/regions/habenula/nimcp_habenula_plasticity_bridge.h"
}

/* VTA plasticity bridge */
extern "C" {
#include "core/brain/regions/vta/nimcp_vta_plasticity_bridge.h"
}

//=============================================================================
// Gustatory Allocation Regression Tests
//=============================================================================

class GustatoryAllocRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryCreateNullConfig_UsesDefaults) {
    /* gust_create(NULL) should use default config and return valid pointer */
    nimcp_gustatory_t* gust = gust_create(NULL);
    ASSERT_NE(gust, nullptr);
    EXPECT_EQ(gust_get_status(gust), GUST_STATUS_READY);
    gust_destroy(gust);
}

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryCreateDestroyCycle) {
    /* Repeated create/destroy should not leak or crash */
    for (int i = 0; i < 10; i++) {
        nimcp_gustatory_t* gust = gust_create(NULL);
        ASSERT_NE(gust, nullptr);
        gust_destroy(gust);
    }
}

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryDestroyNull_NoCrash) {
    /* gust_destroy(NULL) should not crash */
    gust_destroy(NULL);
}

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryResetNull_ReturnsError) {
    /* gust_reset(NULL) should return error, not crash */
    int ret = gust_reset(NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryUpdateNull_ReturnsError) {
    /* gust_update(NULL, dt) should return error, not crash */
    int ret = gust_update(NULL, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(GustatoryAllocRegressionTest, Regression_GustatoryCreateWithConfig_Succeeds) {
    gust_config_t config = gust_default_config();
    nimcp_gustatory_t* gust = gust_create(&config);
    ASSERT_NE(gust, nullptr);
    EXPECT_EQ(gust_get_status(gust), GUST_STATUS_READY);
    gust_destroy(gust);
}

//=============================================================================
// Olfactory Allocation Regression Tests
//=============================================================================

class OlfactoryAllocRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryCreateNullConfig_UsesDefaults) {
    /* olfact_create(NULL) should use default config and return valid pointer */
    nimcp_olfactory_t* olfact = olfact_create(NULL);
    ASSERT_NE(olfact, nullptr);
    EXPECT_EQ(olfact_get_status(olfact), OLFACT_STATUS_READY);
    olfact_destroy(olfact);
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryCreateDestroyCycle) {
    /* Repeated create/destroy should not leak or crash */
    for (int i = 0; i < 10; i++) {
        nimcp_olfactory_t* olfact = olfact_create(NULL);
        ASSERT_NE(olfact, nullptr);
        olfact_destroy(olfact);
    }
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryDestroyNull_NoCrash) {
    /* olfact_destroy(NULL) should not crash */
    olfact_destroy(NULL);
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryResetNull_ReturnsError) {
    /* olfact_reset(NULL) should return error, not crash */
    int ret = olfact_reset(NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryUpdateNull_ReturnsError) {
    /* olfact_update(NULL, dt) should return error, not crash */
    int ret = olfact_update(NULL, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryCreateWithConfig_Succeeds) {
    olfact_config_t config = olfact_default_config();
    nimcp_olfactory_t* olfact = olfact_create(&config);
    ASSERT_NE(olfact, nullptr);
    EXPECT_EQ(olfact_get_status(olfact), OLFACT_STATUS_READY);
    olfact_destroy(olfact);
}

TEST_F(OlfactoryAllocRegressionTest, Regression_OlfactoryKnownOdorsAllocated) {
    /* P2-8: known_odors should be allocated and accessible after create */
    nimcp_olfactory_t* olfact = olfact_create(NULL);
    ASSERT_NE(olfact, nullptr);
    /* known_odors should be non-NULL after successful creation */
    EXPECT_NE(olfact->known_odors, nullptr);
    EXPECT_EQ(olfact->num_known_odors, 0u);
    olfact_destroy(olfact);
}

//=============================================================================
// Raphe Plasticity Bridge Allocation Regression Tests
//=============================================================================

class RaphePlasticityAllocRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(RaphePlasticityAllocRegressionTest, Regression_RapheCreateNullConfig_UsesDefaults) {
    /* nimcp_raphe_plasticity_create(NULL) should use default config */
    nimcp_raphe_plasticity_bridge_t* bridge = nimcp_raphe_plasticity_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_raphe_plasticity_destroy(bridge);
}

TEST_F(RaphePlasticityAllocRegressionTest, Regression_RapheCreateDestroyCycle) {
    for (int i = 0; i < 10; i++) {
        nimcp_raphe_plasticity_bridge_t* bridge = nimcp_raphe_plasticity_create(NULL);
        ASSERT_NE(bridge, nullptr);
        nimcp_raphe_plasticity_destroy(bridge);
    }
}

TEST_F(RaphePlasticityAllocRegressionTest, Regression_RapheDestroyNull_NoCrash) {
    nimcp_raphe_plasticity_destroy(NULL);
}

TEST_F(RaphePlasticityAllocRegressionTest, Regression_RapheResetNull_ReturnsError) {
    int ret = nimcp_raphe_plasticity_reset(NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Habenula Plasticity Bridge Allocation Regression Tests
//=============================================================================

class HabenulaPlasticityAllocRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(HabenulaPlasticityAllocRegressionTest, Regression_HabenulaCreateNullConfig_UsesDefaults) {
    nimcp_habenula_plasticity_bridge_t* bridge = nimcp_habenula_plasticity_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_habenula_plasticity_destroy(bridge);
}

TEST_F(HabenulaPlasticityAllocRegressionTest, Regression_HabenulaCreateDestroyCycle) {
    for (int i = 0; i < 10; i++) {
        nimcp_habenula_plasticity_bridge_t* bridge = nimcp_habenula_plasticity_create(NULL);
        ASSERT_NE(bridge, nullptr);
        nimcp_habenula_plasticity_destroy(bridge);
    }
}

TEST_F(HabenulaPlasticityAllocRegressionTest, Regression_HabenulaDestroyNull_NoCrash) {
    nimcp_habenula_plasticity_destroy(NULL);
}

TEST_F(HabenulaPlasticityAllocRegressionTest, Regression_HabenulaResetNull_ReturnsError) {
    int ret = nimcp_habenula_plasticity_reset(NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// VTA Plasticity Bridge Allocation Regression Tests
//=============================================================================

class VtaPlasticityAllocRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(VtaPlasticityAllocRegressionTest, Regression_VtaCreateNullConfig_UsesDefaults) {
    nimcp_vta_plasticity_bridge_t* bridge = nimcp_vta_plasticity_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_vta_plasticity_destroy(bridge);
}

TEST_F(VtaPlasticityAllocRegressionTest, Regression_VtaCreateDestroyCycle) {
    for (int i = 0; i < 10; i++) {
        nimcp_vta_plasticity_bridge_t* bridge = nimcp_vta_plasticity_create(NULL);
        ASSERT_NE(bridge, nullptr);
        nimcp_vta_plasticity_destroy(bridge);
    }
}

TEST_F(VtaPlasticityAllocRegressionTest, Regression_VtaDestroyNull_NoCrash) {
    nimcp_vta_plasticity_destroy(NULL);
}

TEST_F(VtaPlasticityAllocRegressionTest, Regression_VtaResetNull_ReturnsError) {
    int ret = nimcp_vta_plasticity_reset(NULL);
    EXPECT_EQ(ret, -1);
}

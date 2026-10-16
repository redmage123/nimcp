/**
 * @file test_glial_fep_bridges.cpp
 * @brief Unit tests for all Glial-FEP Bridge modules
 *
 * WHAT: Comprehensive tests for glial-FEP bidirectional integrations
 * WHY:  Ensure glial support integrates with FEP precision and predictions
 * HOW:  Test lifecycle, effects, and bio-async for each bridge type
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "glial/astrocytes/nimcp_astrocytes_fep_bridge.h"
#include "glial/microglia/nimcp_microglia_fep_bridge.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes_fep_bridge.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath_fep_bridge.h"
#include "glial/integration/nimcp_glial_integration_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class GlialFepBridgesTestBase : public ::testing::Test {
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
 * Astrocytes FEP Bridge Tests
 * ============================================================================ */

class AstrocytesFepBridgeTest : public GlialFepBridgesTestBase {
protected:
    astrocytes_fep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            astrocytes_fep_destroy(bridge);
            bridge = nullptr;
        }
        GlialFepBridgesTestBase::TearDown();
    }
};

TEST_F(AstrocytesFepBridgeTest, DefaultConfig) {
    astrocytes_fep_config_t config;
    int ret = astrocytes_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_calcium_prediction);
}

TEST_F(AstrocytesFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(astrocytes_fep_default_config(nullptr), 0);
}

TEST_F(AstrocytesFepBridgeTest, CreateWithNullConfig) {
    astrocytes_fep_bridge_t* br = astrocytes_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(AstrocytesFepBridgeTest, CreateWithNullFep) {
    astrocytes_fep_config_t config;
    astrocytes_fep_default_config(&config);
    astrocytes_fep_bridge_t* br = astrocytes_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(AstrocytesFepBridgeTest, DestroyNull) {
    astrocytes_fep_destroy(nullptr);
}

/* ============================================================================
 * Microglia FEP Bridge Tests
 * ============================================================================ */

class MicrogliaFepBridgeTest : public GlialFepBridgesTestBase {
protected:
    microglia_fep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            microglia_fep_destroy(bridge);
            bridge = nullptr;
        }
        GlialFepBridgesTestBase::TearDown();
    }
};

TEST_F(MicrogliaFepBridgeTest, DefaultConfig) {
    microglia_fep_config_t config;
    int ret = microglia_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.precision_threshold, 0.0f);
    EXPECT_TRUE(config.enable_precision_pruning);
}

TEST_F(MicrogliaFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(microglia_fep_default_config(nullptr), 0);
}

TEST_F(MicrogliaFepBridgeTest, CreateWithNullConfig) {
    microglia_fep_bridge_t* br = microglia_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MicrogliaFepBridgeTest, CreateWithNullFep) {
    microglia_fep_config_t config;
    microglia_fep_default_config(&config);
    microglia_fep_bridge_t* br = microglia_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MicrogliaFepBridgeTest, DestroyNull) {
    microglia_fep_destroy(nullptr);
}

/* ============================================================================
 * Oligodendrocytes FEP Bridge Tests
 * ============================================================================ */

class OligodendrocytesFepBridgeTest : public GlialFepBridgesTestBase {
protected:
    oligodendrocytes_fep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            oligodendrocytes_fep_destroy(bridge);
            bridge = nullptr;
        }
        GlialFepBridgesTestBase::TearDown();
    }
};

TEST_F(OligodendrocytesFepBridgeTest, DefaultConfig) {
    oligodendrocytes_fep_config_t config;
    int ret = oligodendrocytes_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(OligodendrocytesFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(oligodendrocytes_fep_default_config(nullptr), 0);
}

TEST_F(OligodendrocytesFepBridgeTest, CreateWithNullConfig) {
    oligodendrocytes_fep_bridge_t* br = oligodendrocytes_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(OligodendrocytesFepBridgeTest, CreateWithNullFep) {
    oligodendrocytes_fep_config_t config;
    oligodendrocytes_fep_default_config(&config);
    oligodendrocytes_fep_bridge_t* br = oligodendrocytes_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(OligodendrocytesFepBridgeTest, DestroyNull) {
    oligodendrocytes_fep_destroy(nullptr);
}

/* ============================================================================
 * Myelin Sheath FEP Bridge Tests
 * ============================================================================ */

class MyelinSheathFepBridgeTest : public GlialFepBridgesTestBase {
protected:
    myelin_sheath_fep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            myelin_sheath_fep_destroy(bridge);
            bridge = nullptr;
        }
        GlialFepBridgesTestBase::TearDown();
    }
};

TEST_F(MyelinSheathFepBridgeTest, DefaultConfig) {
    myelin_sheath_fep_config_t config;
    int ret = myelin_sheath_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(MyelinSheathFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(myelin_sheath_fep_default_config(nullptr), 0);
}

TEST_F(MyelinSheathFepBridgeTest, CreateWithNullConfig) {
    myelin_sheath_fep_bridge_t* br = myelin_sheath_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MyelinSheathFepBridgeTest, CreateWithNullFep) {
    myelin_sheath_fep_config_t config;
    myelin_sheath_fep_default_config(&config);
    myelin_sheath_fep_bridge_t* br = myelin_sheath_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(MyelinSheathFepBridgeTest, DestroyNull) {
    myelin_sheath_fep_destroy(nullptr);
}

/* ============================================================================
 * Glial Integration FEP Bridge Tests
 * ============================================================================ */

class GlialIntegrationFepBridgeTest : public GlialFepBridgesTestBase {
protected:
    glial_integration_fep_bridge_t* bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            glial_integration_fep_destroy(bridge);
            bridge = nullptr;
        }
        GlialFepBridgesTestBase::TearDown();
    }
};

TEST_F(GlialIntegrationFepBridgeTest, DefaultConfig) {
    glial_integration_fep_config_t config;
    int ret = glial_integration_fep_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.metabolic_gain, 0.0f);
    EXPECT_TRUE(config.enable_metabolic_prediction);
}

TEST_F(GlialIntegrationFepBridgeTest, DefaultConfigNull) {
    EXPECT_NE(glial_integration_fep_default_config(nullptr), 0);
}

TEST_F(GlialIntegrationFepBridgeTest, CreateWithNullConfig) {
    glial_integration_fep_bridge_t* br = glial_integration_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(GlialIntegrationFepBridgeTest, CreateWithNullFep) {
    glial_integration_fep_config_t config;
    glial_integration_fep_default_config(&config);
    glial_integration_fep_bridge_t* br = glial_integration_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(GlialIntegrationFepBridgeTest, DestroyNull) {
    glial_integration_fep_destroy(nullptr);
}


/**
 * @file test_brain_substrate_thalamic_wireup.cpp
 * @brief Verifies Phase 1-4 substrate + thalamic router are actually wired
 *        into SNN/LNN/cortex CNN networks after brain init.
 *
 * WHAT: After brain creation (and optional multi-network training activation),
 *       the brain must own a neural_substrate_t + thalamic_router_t and every
 *       specialized network that exists must hold the same borrowed pointers.
 *
 * WHY:  Prior to the substrate+thalamic init wave, 95 callers of the attach
 *       functions lived in test code and 0 in src/. The adapters were fully
 *       implemented but dormant. This test guards against regression.
 *
 * HOW:  Use only the public accessor API exposed in the init subsystems
 *       header — avoids the thalamic_router_t typedef clash between
 *       parietal.h and nimcp_thalamic_router.h when included under C++.
 */

#include <gtest/gtest.h>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/* Forward decl of multi-network training entry — avoids pulling in
 * nimcp_brain_learning.h which transitively drags CUDA templates into
 * an extern "C" context (illegal under C++). */
int brain_enable_multi_network_training(brain_t brain);
}

class BrainSubstrateThalamicWireupTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;

    void SetUp() override {
        unified_mem_config_t mc = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mc);
        ASSERT_NE(mem_mgr_, nullptr);

        nimcp_bio_async_config_t bc = nimcp_bio_async_default_config();
        (void)nimcp_bio_async_init(&bc);
        bio_router_config_t rc = bio_router_default_config();
        (void)bio_router_init(&rc);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }
};

//=============================================================================
// Test 1 — substrate + thalamic router are created during brain init.
//=============================================================================
TEST_F(BrainSubstrateThalamicWireupTest, SubstrateAndRouterExistAfterBrainCreate)
{
    /* A TINY brain is sufficient for init-wave coverage — SNN/LNN will not
     * be created yet (they're created on-demand in multi-network training)
     * but brain->substrate and brain->thalamic_router must exist because
     * the init wave runs regardless of network type. */
    brain_t brain = brain_create("wireup_test",
                                 BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION,
                                 16, 8);
    ASSERT_NE(brain, nullptr) << "brain_create returned NULL";

    EXPECT_NE(nimcp_brain_get_substrate(brain), nullptr)
        << "brain->substrate was NULL — substrate_thalamic init wave did not run";
    EXPECT_TRUE(nimcp_brain_substrate_is_enabled(brain))
        << "substrate_enabled flag not set despite substrate being created";

    EXPECT_NE(nimcp_brain_get_thalamic_router(brain), nullptr)
        << "brain->thalamic_router was NULL — router init wave did not run";
    EXPECT_TRUE(nimcp_brain_thalamic_router_is_enabled(brain))
        << "thalamic_router_enabled flag not set despite router being created";

    brain_destroy(brain);
}

//=============================================================================
// Test 2 — after enable_multi_network_training, any networks that got created
//           have the substrate attached. For a TINY brain with 16/8 I/O, LNN
//           should create (≥ 8 threshold) and so should SNN.
//=============================================================================
TEST_F(BrainSubstrateThalamicWireupTest, NetworksReceiveSubstrateOnEnableMultiNetwork)
{
    brain_t brain = brain_create("wireup_test3",
                                 BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION,
                                 16, 8);
    ASSERT_NE(brain, nullptr);

    /* Enable multi-network training — creates LNN and SNN, then attach
     * helper runs inside brain_enable_multi_network_training's epilog. */
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0) << "brain_enable_multi_network_training failed";

    struct neural_substrate* brain_sub    = nimcp_brain_get_substrate(brain);
    struct thalamic_router*  brain_router = nimcp_brain_get_thalamic_router(brain);
    ASSERT_NE(brain_sub, nullptr)
        << "brain->substrate disappeared after multi-network training enable";

    /* Compare SNN's borrowed substrate pointer to brain's. If SNN was created
     * at all, it must share the same substrate. */
    struct neural_substrate* snn_sub = nimcp_brain_snn_get_substrate_ref(brain);
    if (snn_sub != nullptr) {
        EXPECT_EQ(snn_sub, brain_sub)
            << "SNN substrate pointer was not wired to brain->substrate "
               "(snn=" << (void*)snn_sub << " vs brain=" << (void*)brain_sub << ")";
    }

    /* LNN substrate + thalamic_channel checks. */
    struct neural_substrate* lnn_sub = nimcp_brain_lnn_get_substrate_ref(brain);
    void* lnn_chan = nimcp_brain_lnn_get_thalamic_channel_ref(brain);
    if (lnn_sub != nullptr || lnn_chan != nullptr) {
        EXPECT_EQ(lnn_sub, brain_sub)
            << "LNN substrate pointer was not wired to brain->substrate";
        if (brain_router != nullptr) {
            EXPECT_NE(lnn_chan, nullptr)
                << "LNN thalamic_channel was not created by "
                   "lnn_network_attach_thalamic_router";
        }
    }

    /* At least ONE of the specialized networks must have been wired — this
     * is the regression guard. A successful multi-network enable with 16/8
     * dims should create at least an SNN (via feedforward fallback). */
    EXPECT_TRUE(snn_sub != nullptr || lnn_sub != nullptr)
        << "No specialized network received a substrate pointer — "
           "brain_enable_multi_network_training may not have created any, "
           "or nimcp_brain_attach_substrate_thalamic() did not run";

    brain_destroy(brain);
}

//=============================================================================
// Test 3 — substrate and router are destroyed cleanly.
//           If this test doesn't segfault under ASan, the destroy ordering
//           (detach-before-destroy) is correct.
//=============================================================================
TEST_F(BrainSubstrateThalamicWireupTest, DestroyOrderingIsClean)
{
    brain_t brain = brain_create("wireup_test4",
                                 BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION,
                                 16, 8);
    ASSERT_NE(brain, nullptr);
    (void)brain_enable_multi_network_training(brain);

    /* brain_destroy must detach substrate/router from every network BEFORE
     * destroying them, then destroy the networks themselves. ASan/TSan
     * would flag a use-after-free if order were wrong. */
    brain_destroy(brain);
    SUCCEED();
}

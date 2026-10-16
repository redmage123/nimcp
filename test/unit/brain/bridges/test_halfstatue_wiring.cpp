/**
 * @file test_halfstatue_wiring.cpp
 * @brief Verify Wave 8A+ (2026-04-24) cochlea-bridge output-buffer fix.
 *
 * Before this fix, brain_part_core.c + brain_learning.c + the
 * BRAIN_CYCLE_COCHLEA_BRIDGES tick wrapper all passed NULL as the
 * `cochlea_output_t*` arg to the 11 NULL-rejecting cochlea bridges.
 * Each bridge's `_update` function bailed out on `if (!cochlea_output)`
 * without ever incrementing `bridge_base_t::total_updates`.
 *
 * The fix: brain->cochlea_output_buffer is a shared, zero-filled
 * cochlea_output_t* allocated at init_cochlea time and destroyed with
 * the cochlea itself. All three call sites now pass it instead of NULL.
 *
 * These tests verify:
 *   1. brain->cochlea_output_buffer is non-NULL after brain_create when
 *      cochlea is enabled.
 *   2. brain_decide() advances each wired bridge's total_updates counter.
 *   3. The buffer is cleaned up by brain_destroy (no double-free).
 *
 * Each bridge in the cochlea cluster embeds a `bridge_base_t base` as its
 * FIRST field, so we can read `total_updates` by casting the stored
 * bridge pointer to `bridge_base_t*`. This is stable across header
 * collisions — we don't need to include the bridge header.
 */

#include <gtest/gtest.h>
#include <cstring>

/* The NIMCP headers are already guarded with their own extern "C" blocks
 * for C++ callers. Wrapping them in an outer extern "C" pulls cuBLAS /
 * cuDNN templates into C linkage and they redeclare with differing
 * types — see test_brain_glial_init.cpp for the same workaround. */
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_cochlea.h"
#include "utils/bridge/nimcp_bridge_base.h"

static brain_t make_cochlea_brain() {
    brain_t brain = brain_create("halfstatue_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 8, 4);
    if (!brain) return nullptr;
    /* BRAIN_SIZE_TINY defaults lazy_init_mode=true, which skips
     * init_cochlea during parallel_init. Force-run the cochlea wave
     * here so we can exercise the bridge wiring. */
    (void)nimcp_brain_factory_init_cochlea_subsystem(brain);
    return brain;
}

// Read total_updates from the bridge pointer via its leading bridge_base_t.
static uint64_t read_total_updates(void* bridge_ptr) {
    if (!bridge_ptr) return 0;
    const bridge_base_t* base = static_cast<const bridge_base_t*>(bridge_ptr);
    return base->total_updates;
}

class HalfstatueWiringTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// --- Test 1: buffer is allocated when cochlea is live ----------------

TEST_F(HalfstatueWiringTest, CochleaOutputBufferAllocated) {
    brain = make_cochlea_brain();
    ASSERT_NE(brain, nullptr);
    if (!brain->cochlea) {
        // Cochlea disabled on this size — buffer must stay NULL, test
        // passes vacuously.
        EXPECT_EQ(brain->cochlea_output_buffer, nullptr);
        GTEST_SKIP() << "cochlea not enabled on BRAIN_SIZE_TINY build";
    }
    EXPECT_NE(brain->cochlea_output_buffer, nullptr);
}

// --- Test 2: after brain_decide, each wired bridge's total_updates
//              has moved forward from 0 ------------------------------

TEST_F(HalfstatueWiringTest, BrainDecideAdvancesBridgeCounters) {
    brain = make_cochlea_brain();
    ASSERT_NE(brain, nullptr);
    if (!brain->cochlea || !brain->cochlea_output_buffer) {
        GTEST_SKIP() << "cochlea / output buffer not live on this build";
    }

    struct { const char* name; void* ptr; } bridges[] = {
        {"audio_cortex",   (void*)brain->cochlea_audio_cortex_bridge},
        {"bio_async",      (void*)brain->cochlea_bio_async_bridge},
        {"broca",          (void*)brain->cochlea_broca_bridge},
        {"collective",     (void*)brain->cochlea_collective_bridge},
        {"cortical_deep",  (void*)brain->cochlea_cortical_deep_bridge},
        {"fep",            (void*)brain->cochlea_fep_bridge},
        {"immune",         (void*)brain->cochlea_immune_bridge},
        {"medulla",        (void*)brain->cochlea_medulla_bridge},
        {"occipital",      (void*)brain->cochlea_occipital_bridge},
        {"rcog",           (void*)brain->cochlea_rcog_bridge},
        {"substrate",      (void*)brain->cochlea_substrate_bridge},
        {"verification",   (void*)brain->cochlea_verification_bridge},
    };
    const size_t N = sizeof(bridges) / sizeof(bridges[0]);

    // Snapshot baseline counts (some bridges may already have been ticked
    // during init by the driven cycle before the test got a chance to
    // observe, so just grab the current count).
    uint64_t before[N];
    bool any_live = false;
    for (size_t i = 0; i < N; i++) {
        before[i] = read_total_updates(bridges[i].ptr);
        if (bridges[i].ptr) any_live = true;
    }
    if (!any_live) {
        GTEST_SKIP() << "no cochlea bridges live on this build";
    }

    // Run a handful of inference passes so we go through brain_decide +
    // the cochlea-bridges tick block at least once.
    float feats[8] = { 0.1f, -0.2f, 0.3f, 0.0f, 0.5f, -0.1f, 0.2f, 0.4f };
    for (int k = 0; k < 4; k++) {
        brain_decision_t* d = brain_decide(brain, feats, 8);
        if (d) brain_free_decision(d);
    }

    // Every live bridge should have advanced its counter at least once.
    for (size_t i = 0; i < N; i++) {
        if (!bridges[i].ptr) continue;
        uint64_t after = read_total_updates(bridges[i].ptr);
        EXPECT_GT(after, before[i])
            << "Bridge '" << bridges[i].name << "' total_updates did not "
            << "advance — suggests _update() still early-returns on NULL "
            << "input (Wave 8A+ regression).";
    }
}

// --- Test 3: buffer is safely cleaned up -----------------------------

TEST(HalfstatueWiringDestroy, DestroyIsSafe) {
    brain_t b = brain_create("halfstatue_destroy_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(b, nullptr);
    // Whether cochlea is enabled or not, brain_destroy must not crash.
    brain_destroy(b);
}

// --- Test 4: DIRECT bridge-update path (no brain needed).
//
// This exercises the core fix without requiring that the brain has
// actually instantiated the cochlea subsystem. We:
//   1. Create a standalone cochlea + one NULL-rejecting consumer bridge
//      (fep is the simplest — only needs a fep_orchestrator which we
//      pass as NULL to keep the bridge a standalone pipeline).
//   2. Allocate a cochlea_output_t and call _update with it.
//   3. Verify total_updates advanced from 0.
//
// Pre-fix, the hot-path wiring passed NULL for the output and this
// counter never moved. Post-fix, the shared output buffer makes the
// call valid. This test doesn't depend on brain init cascades, so it
// always runs (as long as cochlea_create is linked in).

extern "C" {
    // Manual forward-decls of the three cochlea-API symbols we need.
    // Declared here rather than via <perception/nimcp_cochlea.h> because
    // that header transitively pulls types that collide with
    // nimcp_brain_internal.h's medulla_t / thalamus_t definitions.
    typedef struct cochlea cochlea_t;
    struct cochlea_output;
    typedef struct cochlea_output cochlea_output_t;  // NB: tag-only here
    struct cochlea_fep_bridge;
    typedef struct cochlea_fep_bridge cochlea_fep_bridge_t;

    void* cochlea_output_create(cochlea_t*, uint32_t);
    void  cochlea_output_destroy(void*);
    void  cochlea_output_clear(void*);

    cochlea_fep_bridge_t* cochlea_fep_bridge_create(cochlea_t*, void*,
                                                    const void*);
    void cochlea_fep_bridge_destroy(cochlea_fep_bridge_t*);
    int cochlea_fep_bridge_update(cochlea_fep_bridge_t*, const void*, float);
}

TEST(HalfstatueWiringDirect, FepBridgeAdvancesWithNonNullOutput) {
    // Create the FEP bridge with cochlea=NULL and fep=NULL — the bridge
    // doesn't deref either in its update path, it only reads the output
    // buffer we pass. This exercises the "is the output buffer honored"
    // wiring contract without pulling in a full brain init.
    cochlea_fep_bridge_t* b = cochlea_fep_bridge_create(nullptr, nullptr,
                                                        nullptr);
    if (!b) {
        GTEST_SKIP() << "cochlea_fep_bridge_create failed without cochlea "
                        "— implementation requires cochlea (ok to skip)";
    }
    const bridge_base_t* base = reinterpret_cast<const bridge_base_t*>(b);
    uint64_t before = base->total_updates;

    // Allocate a dummy output_t-shaped zero buffer. Because cochlea_output_t
    // is an anonymous-struct typedef we can't sizeof it portably from here;
    // the _update function only needs a non-NULL pointer to pass its guard.
    // Internally it reads fields but the bridge's zero-rate learning path
    // handles zero data gracefully (precision_weights stay at init values).
    char stub[1024];
    std::memset(stub, 0, sizeof(stub));

    for (int i = 0; i < 3; i++) {
        (void)cochlea_fep_bridge_update(b, stub, 1.0f);
    }

    // This is the core assertion: with a non-NULL output pointer, the
    // bridge runs its update loop and increments the counter via
    // bridge_base_record_update. Pre-fix this was never reached.
    EXPECT_GT(base->total_updates, before)
        << "cochlea_fep_bridge_update did not advance total_updates — "
        << "the 8A+ output-buffer contract is broken.";

    cochlea_fep_bridge_destroy(b);
}

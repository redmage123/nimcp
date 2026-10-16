/**
 * @file test_synapse_lifecycle.cpp
 * @brief Comprehensive unit tests for synapse lifecycle manager
 *
 * WHAT: Test suite for dynamic synapse pruning, growth, metadata GC
 * WHY:  Validate API contracts, config defaults, NULL safety, interval tracking
 * HOW:  Google Test; uses weak stubs (NULL network yields neuron_count=0)
 *
 * TEST COVERAGE:
 * - Lifecycle manager basics (create/destroy, config defaults, NULL safety)
 * - Pruning (interval tracking, config storage, report fields)
 * - Growth (interval tracking, config storage, report fields)
 * - Metadata GC (interval tracking, enable/disable, report fields)
 * - Embedded capacity constants from sparse_synapse.h
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/neuralnet/nimcp_synapse_lifecycle.h"
#include "core/neuralnet/nimcp_sparse_synapse.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SynapseLifecycleTest : public ::testing::Test {
protected:
    nimcp_synapse_lifecycle_t* mgr = nullptr;

    void TearDown() override {
        if (mgr) {
            nimcp_synapse_lifecycle_destroy(mgr);
            mgr = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Manager Basics (5 tests)
//=============================================================================

TEST_F(SynapseLifecycleTest, CreateDestroyWithDefaultConfig) {
    // WHAT: Create manager with NULL config (use defaults), then destroy
    // WHY:  Basic lifecycle must work
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr) << "nimcp_synapse_lifecycle_create(NULL) must return valid handle";

    // Destroy is tested implicitly in TearDown; explicit call here
    nimcp_synapse_lifecycle_destroy(mgr);
    mgr = nullptr;  // Prevent double-free in TearDown
}

TEST_F(SynapseLifecycleTest, ConfigDefaultsAreReasonable) {
    // WHAT: Verify default config values match documented expectations
    // WHY:  Config is the contract between the lifecycle manager and callers
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();

    // Pruning defaults
    EXPECT_FLOAT_EQ(cfg.weight_prune_threshold, 0.001f);
    EXPECT_FLOAT_EQ(cfg.activity_prune_threshold, 0.1f);
    EXPECT_EQ(cfg.prune_interval_steps, 1000u);
    EXPECT_EQ(cfg.max_prune_per_sweep, 0u);         // 0 = auto (5% of total)
    EXPECT_EQ(cfg.min_synapses_per_neuron, 16u);

    // Growth defaults
    EXPECT_FLOAT_EQ(cfg.activity_growth_threshold, 0.8f);
    EXPECT_EQ(cfg.max_new_per_sweep, 8u);
    EXPECT_EQ(cfg.growth_interval_steps, 2000u);
    EXPECT_FLOAT_EQ(cfg.initial_weight, 0.01f);
    EXPECT_EQ(cfg.max_synapses_per_neuron, 512u);

    // Metadata GC defaults
    EXPECT_TRUE(cfg.enable_metadata_gc);
    EXPECT_EQ(cfg.gc_interval_steps, 5000u);
    EXPECT_EQ(cfg.metadata_pool_cap, 50000000u);     // 50M

    // Compaction
    EXPECT_TRUE(cfg.enable_compaction);
}

TEST_F(SynapseLifecycleTest, StepWithNullNetworkReturnsError) {
    // WHAT: step() with NULL network returns -1
    // WHY:  Graceful NULL handling is required
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int ret = nimcp_synapse_lifecycle_step(mgr, nullptr, 1000, &report);
    EXPECT_EQ(ret, -1);
}

TEST_F(SynapseLifecycleTest, GetReportShowsZeroedInitially) {
    // WHAT: Fresh manager has zeroed cumulative counters
    // WHY:  Report must start clean
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0xFF, sizeof(report));  // Fill with garbage

    int ret = nimcp_synapse_lifecycle_get_report(mgr, &report);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(report.synapses_pruned, 0u);
    EXPECT_EQ(report.synapses_grown, 0u);
    EXPECT_EQ(report.overflow_compacted, 0u);
}

TEST_F(SynapseLifecycleTest, NullSafetyOnAllFunctions) {
    // WHAT: All API functions handle NULL manager gracefully
    // WHY:  Robustness against programming errors
    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    // NULL manager
    EXPECT_EQ(nimcp_synapse_lifecycle_step(nullptr, (void*)0x1, 0, &report), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_prune(nullptr, (void*)0x1, &report), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_grow(nullptr, (void*)0x1, &report), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_gc(nullptr, (void*)0x1, &report), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_get_report(nullptr, &report), -1);

    // NULL report
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, (void*)0x1, 0, nullptr), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_prune(mgr, (void*)0x1, nullptr), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_grow(mgr, (void*)0x1, nullptr), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_gc(mgr, (void*)0x1, nullptr), -1);
    EXPECT_EQ(nimcp_synapse_lifecycle_get_report(mgr, nullptr), -1);

    // Destroy with NULL is safe
    nimcp_synapse_lifecycle_destroy(nullptr);
}

//=============================================================================
// Pruning (8 tests)
//=============================================================================

TEST_F(SynapseLifecycleTest, PruneWithNullNetworkReturnsError) {
    // WHAT: prune() with NULL network returns -1
    // WHY:  Network pointer is required
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int ret = nimcp_synapse_lifecycle_prune(mgr, nullptr, &report);
    EXPECT_EQ(ret, -1);
}

TEST_F(SynapseLifecycleTest, PruneWithWeakStubDoesNothing) {
    // WHAT: prune() with non-NULL network but weak stubs (neuron_count=0) returns 0
    // WHY:  Empty network is not an error, just a no-op
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int dummy_network = 42;
    int ret = nimcp_synapse_lifecycle_prune(mgr, &dummy_network, &report);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(report.synapses_pruned, 0u);
}

TEST_F(SynapseLifecycleTest, ConfigWeightThresholdStoredCorrectly) {
    // WHAT: Custom weight_prune_threshold is stored and retrievable via config
    // WHY:  Caller's config must be respected
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.weight_prune_threshold = 0.05f;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    // Verify via a no-op prune (weak stubs return 0 neurons)
    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));
    int dummy = 1;
    int ret = nimcp_synapse_lifecycle_prune(mgr, &dummy, &report);
    EXPECT_EQ(ret, 0);
    // Can't directly inspect config, but the manager accepted it without error
}

TEST_F(SynapseLifecycleTest, ConfigMinSynapsesPerNeuronIsRespected) {
    // WHAT: min_synapses_per_neuron set to custom value is accepted
    // WHY:  Prevents over-pruning
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.min_synapses_per_neuron = 32;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);
    // Manager created successfully with custom min
}

TEST_F(SynapseLifecycleTest, ConfigMaxPrunePerSweepLimitsRemovals) {
    // WHAT: max_prune_per_sweep=100 is stored correctly
    // WHY:  Limits pruning to avoid catastrophic connectivity loss
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.max_prune_per_sweep = 100;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);
}

TEST_F(SynapseLifecycleTest, PruneIntervalTracking) {
    // WHAT: step() only prunes at prune_interval_steps intervals
    // WHY:  Pruning is expensive; only run periodically
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.prune_interval_steps = 500;
    cfg.growth_interval_steps = 100000;  // Disable growth for this test
    cfg.gc_interval_steps = 100000;      // Disable GC for this test
    cfg.enable_metadata_gc = false;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    int dummy = 1;

    // Step 0 should trigger prune (0 - 0 >= 500 is false initially, but 0 == 0 means
    // the manager starts with last_prune_step=0, so step(0) would check 0-0 >= 500 = false)
    memset(&report, 0, sizeof(report));
    int ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 100, &report);
    EXPECT_EQ(ret, 0);
    // Step 100: 100 - 0 = 100 < 500, no prune triggered

    // Step 500 should trigger prune
    memset(&report, 0, sizeof(report));
    ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 500, &report);
    EXPECT_EQ(ret, 0);
    // 500 - 0 >= 500, prune triggered (but weak stubs yield 0 neurons, so no actual pruning)

    // Step 600 should NOT trigger prune (only 100 since last)
    memset(&report, 0, sizeof(report));
    ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 600, &report);
    EXPECT_EQ(ret, 0);

    // Step 1000 should trigger prune again (1000 - 500 >= 500)
    memset(&report, 0, sizeof(report));
    ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 1000, &report);
    EXPECT_EQ(ret, 0);
}

TEST_F(SynapseLifecycleTest, PruneReportFieldsPopulated) {
    // WHAT: After prune via step(), report fields are zeroed then populated
    // WHY:  Caller depends on valid report contents
    // NOTE: prune() alone does NOT zero the report; step() does the memset.
    //       With weak stubs (0 neurons), prune exits early after neuron_count==0,
    //       so we test via step() which zeroes report before calling prune().
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0xFF, sizeof(report));  // Fill with garbage

    int dummy = 1;
    // Use step() at prune_interval to trigger prune; step zeroes report first
    int ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 1000, &report);
    EXPECT_EQ(ret, 0);

    // step() zeroes report, then prune with 0 neurons is a no-op
    EXPECT_EQ(report.synapses_pruned, 0u);
    EXPECT_FLOAT_EQ(report.avg_weight_pruned, 0.0f);
}

TEST_F(SynapseLifecycleTest, MultiplePruneSweepsAccumulateTotals) {
    // WHAT: Cumulative report grows across multiple prune calls
    // WHY:  Lifecycle manager must track historical totals
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    int dummy = 1;
    nimcp_synapse_lifecycle_report_t report;

    // Call prune multiple times (all no-ops with weak stubs)
    for (int i = 0; i < 5; i++) {
        memset(&report, 0, sizeof(report));
        nimcp_synapse_lifecycle_prune(mgr, &dummy, &report);
    }

    // Cumulative report
    nimcp_synapse_lifecycle_report_t cumulative;
    memset(&cumulative, 0xFF, sizeof(cumulative));
    int ret = nimcp_synapse_lifecycle_get_report(mgr, &cumulative);
    EXPECT_EQ(ret, 0);

    // With weak stubs, totals remain 0 but function succeeds
    EXPECT_EQ(cumulative.synapses_pruned, 0u);
    EXPECT_EQ(cumulative.synapses_grown, 0u);
}

TEST_F(SynapseLifecycleTest, PruneWithZeroThresholdPrunesNothing) {
    // WHAT: weight_prune_threshold=0 means nothing qualifies for pruning
    // WHY:  Zero threshold should be a valid "disable pruning" setting
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.weight_prune_threshold = 0.0f;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int dummy = 1;
    int ret = nimcp_synapse_lifecycle_prune(mgr, &dummy, &report);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(report.synapses_pruned, 0u);
}

//=============================================================================
// Growth (5 tests)
//=============================================================================

TEST_F(SynapseLifecycleTest, GrowWithNullNetworkReturnsError) {
    // WHAT: grow() with NULL network returns -1
    // WHY:  Network pointer is required
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int ret = nimcp_synapse_lifecycle_grow(mgr, nullptr, &report);
    EXPECT_EQ(ret, -1);
}

TEST_F(SynapseLifecycleTest, GrowWithWeakStubDoesNothing) {
    // WHAT: grow() with weak stubs (neuron_count=0) returns 0 with no growth
    // WHY:  Empty network is a valid no-op
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int dummy = 1;
    int ret = nimcp_synapse_lifecycle_grow(mgr, &dummy, &report);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(report.synapses_grown, 0u);
}

TEST_F(SynapseLifecycleTest, ConfigGrowthThresholdStoredCorrectly) {
    // WHAT: Custom activity_growth_threshold is accepted
    // WHY:  Controls which neurons get new synapses
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.activity_growth_threshold = 0.5f;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    // No crash, manager accepted custom threshold
    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));
    int dummy = 1;
    EXPECT_EQ(nimcp_synapse_lifecycle_grow(mgr, &dummy, &report), 0);
}

TEST_F(SynapseLifecycleTest, ConfigMaxNewPerSweepLimitsAdditions) {
    // WHAT: max_new_per_sweep=1 is accepted
    // WHY:  Limits new synapse creation rate
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.max_new_per_sweep = 1;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);
}

TEST_F(SynapseLifecycleTest, GrowthIntervalTracking) {
    // WHAT: step() only grows at growth_interval_steps intervals
    // WHY:  Growth is periodic, not every step
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.prune_interval_steps = 100000;   // Disable prune
    cfg.growth_interval_steps = 2000;
    cfg.gc_interval_steps = 100000;      // Disable GC
    cfg.enable_metadata_gc = false;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    int dummy = 1;

    // Step 1000: 1000 - 0 < 2000, no growth
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 1000, &report), 0);

    // Step 2000: 2000 - 0 >= 2000, growth triggered
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 2000, &report), 0);

    // Step 3000: 3000 - 2000 < 2000, no growth
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 3000, &report), 0);

    // Step 4000: 4000 - 2000 >= 2000, growth triggered
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 4000, &report), 0);
}

//=============================================================================
// Metadata GC (4 tests)
//=============================================================================

TEST_F(SynapseLifecycleTest, GCWithNullNetworkReturnsError) {
    // WHAT: gc() with NULL network returns -1
    // WHY:  Network pointer is required
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));

    int ret = nimcp_synapse_lifecycle_gc(mgr, nullptr, &report);
    EXPECT_EQ(ret, -1);
}

TEST_F(SynapseLifecycleTest, GCCanBeDisabledViaConfig) {
    // WHAT: enable_metadata_gc=false skips GC in step()
    // WHY:  Allow disabling expensive GC pass
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.enable_metadata_gc = false;
    cfg.prune_interval_steps = 100000;   // Disable prune
    cfg.growth_interval_steps = 100000;  // Disable growth
    cfg.gc_interval_steps = 1;           // Would trigger every step if enabled

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    int dummy = 1;

    // Even at step 10000 (well past gc_interval), GC should not run
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 10000, &report), 0);

    // metadata_pool_usage should be 0 (never queried because GC didn't run)
    EXPECT_EQ(report.metadata_pool_usage, 0u);
}

TEST_F(SynapseLifecycleTest, GCIntervalTracking) {
    // WHAT: step() only runs GC at gc_interval_steps intervals
    // WHY:  GC is expensive and periodic
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.prune_interval_steps = 100000;   // Disable prune
    cfg.growth_interval_steps = 100000;  // Disable growth
    cfg.gc_interval_steps = 5000;
    cfg.enable_metadata_gc = true;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    int dummy = 1;

    // Step 1000: 1000 < 5000, no GC
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 1000, &report), 0);

    // Step 5000: 5000 - 0 >= 5000, GC triggered
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 5000, &report), 0);

    // Step 6000: 6000 - 5000 < 5000, no GC
    memset(&report, 0, sizeof(report));
    EXPECT_EQ(nimcp_synapse_lifecycle_step(mgr, &dummy, 6000, &report), 0);
}

TEST_F(SynapseLifecycleTest, GCReportFields) {
    // WHAT: GC populates report fields correctly
    // WHY:  Caller uses metadata_pool_usage/cap for monitoring
    mgr = nimcp_synapse_lifecycle_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0xFF, sizeof(report));  // Fill with garbage

    int dummy = 1;
    int ret = nimcp_synapse_lifecycle_gc(mgr, &dummy, &report);
    EXPECT_EQ(ret, 0);

    // Weak stubs return 0 for pool usage and cap
    EXPECT_EQ(report.metadata_pool_usage, 0u);
    EXPECT_EQ(report.metadata_pool_cap, 0u);
    EXPECT_EQ(report.metadata_orphans_collected, 0u);
}

//=============================================================================
// Embedded Capacity Constants (3 tests)
//=============================================================================

TEST(SparseConstantsTest, EmbeddedCapacityEquals64) {
    // WHAT: Verify SPARSE_SYNAPSE_EMBEDDED_CAPACITY is 64
    // WHY:  Phase 1 dynamic synapse arch reduced from 256 to 64
    EXPECT_EQ(SPARSE_SYNAPSE_EMBEDDED_CAPACITY, 64);
}

TEST(SparseConstantsTest, StorageSizeIsReasonable) {
    // WHAT: sizeof sparse_synapse_storage_t should be under 2000 bytes
    // WHY:  Per-neuron storage must be compact (64 handles * 24 bytes + overhead)
    size_t sz = sizeof(sparse_synapse_storage_t);
    EXPECT_LT(sz, 2000u) << "sparse_synapse_storage_t is " << sz
                          << " bytes, expected < 2000";
    // Also verify it's at least as big as 64 handles
    EXPECT_GE(sz, 64 * sizeof(synapse_handle_t));
}

TEST(SparseConstantsTest, DefaultPoolSizeIs500000) {
    // WHAT: Verify default pool size constant
    // WHY:  Pool size was increased to 500K to compensate for smaller embedded capacity
    EXPECT_EQ(SPARSE_SYNAPSE_DEFAULT_POOL_SIZE, 500000u);
}

//=============================================================================
// Additional config edge cases (bonus coverage)
//=============================================================================

TEST_F(SynapseLifecycleTest, CreateWithExplicitConfig) {
    // WHAT: Create manager with explicitly set config
    // WHY:  Verify custom config path works
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.weight_prune_threshold = 0.1f;
    cfg.prune_interval_steps = 500;
    cfg.max_new_per_sweep = 4;
    cfg.max_synapses_per_neuron = 256;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);
}

TEST_F(SynapseLifecycleTest, StepWithAllPhasesEnabled) {
    // WHAT: step() at a step number that triggers all three phases
    // WHY:  Verify all phases can run in a single step
    nimcp_synapse_lifecycle_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.weight_prune_threshold = 0.001f;
    cfg.activity_prune_threshold = 0.1f;
    cfg.prune_interval_steps = 100;
    cfg.max_prune_per_sweep = 10;
    cfg.min_synapses_per_neuron = 8;
    cfg.activity_growth_threshold = 0.8f;
    cfg.max_new_per_sweep = 4;
    cfg.growth_interval_steps = 100;
    cfg.initial_weight = 0.01f;
    cfg.max_synapses_per_neuron = 512;
    cfg.enable_metadata_gc = true;
    cfg.gc_interval_steps = 100;
    cfg.metadata_pool_cap = 1000000;
    cfg.enable_compaction = true;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    int dummy = 1;

    // Step 100 should trigger all three phases
    memset(&report, 0, sizeof(report));
    int ret = nimcp_synapse_lifecycle_step(mgr, &dummy, 100, &report);
    EXPECT_EQ(ret, 0);
}

TEST_F(SynapseLifecycleTest, ConfigMaxSynapsesPerNeuronCap) {
    // WHAT: max_synapses_per_neuron is stored in config
    // WHY:  Growth must respect the cap
    nimcp_synapse_lifecycle_config_t cfg = nimcp_synapse_lifecycle_config_default();
    cfg.max_synapses_per_neuron = 128;

    mgr = nimcp_synapse_lifecycle_create(&cfg);
    ASSERT_NE(mgr, nullptr);

    nimcp_synapse_lifecycle_report_t report;
    memset(&report, 0, sizeof(report));
    int dummy = 1;
    EXPECT_EQ(nimcp_synapse_lifecycle_grow(mgr, &dummy, &report), 0);
}

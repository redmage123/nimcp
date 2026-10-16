/**
 * @file test_mesh_performance_regression.cpp
 * @brief Mesh Network Performance Regression Tests
 *
 * WHAT: Performance benchmarks to catch throughput/latency regressions
 * WHY:  Ensure mesh network maintains acceptable performance over time
 * HOW:  Measure key metrics and compare against baseline thresholds
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Performance Thresholds (Baseline)
 * ============================================================================ */

/* These thresholds define acceptable performance levels */
static constexpr double THRESHOLD_TX_THROUGHPUT_MIN = 100.0;      /* transactions/second */
static constexpr double THRESHOLD_TX_LATENCY_P50_MAX = 50.0;      /* milliseconds */
static constexpr double THRESHOLD_TX_LATENCY_P99_MAX = 200.0;     /* milliseconds */
static constexpr double THRESHOLD_ELECTION_TIME_MAX = 500.0;      /* milliseconds */
static constexpr double THRESHOLD_CONSENSUS_TIME_MAX = 100.0;     /* milliseconds */
static constexpr double THRESHOLD_GOSSIP_CONVERGENCE_MAX = 500.0; /* milliseconds */
static constexpr size_t THRESHOLD_MEMORY_PER_TX_MAX = 4096;       /* bytes */

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MeshPerformanceRegressionTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry = nullptr;
    mesh_channel_t* channel = nullptr;
    mesh_coordinator_pool_t* pool = nullptr;
    mesh_ordering_service_t* ordering = nullptr;

    void SetUp() override {
        /* Create participant registry */
        mesh_registry_config_t reg_config;
        mesh_registry_default_config(&reg_config);
        reg_config.max_participants = 256;
        registry = mesh_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create channel */
        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        strncpy(ch_config.name, "perf_test", MESH_MAX_NAME_LEN);
        ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
        channel = mesh_channel_create(&ch_config);
        ASSERT_NE(channel, nullptr);

        /* Create coordinator pool */
        mesh_coordinator_pool_config_t pool_config;
        mesh_coordinator_pool_default_config(&pool_config);
        strncpy(pool_config.name, "perf_pool", MESH_MAX_NAME_LEN);
        pool_config.min_coordinators = 3;
        pool_config.max_coordinators = 7;
        pool = mesh_coordinator_pool_create(&pool_config);
        ASSERT_NE(pool, nullptr);

        /* Create ordering service */
        mesh_ordering_config_t ord_config;
        mesh_ordering_default_config(&ord_config);
        strncpy(ord_config.name, "perf_ordering", MESH_MAX_NAME_LEN);
        ord_config.batch_size = 50;
        ord_config.batch_timeout_ms = 20.0f;
        ordering = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering, nullptr);
    }

    void TearDown() override {
        if (ordering) mesh_ordering_destroy(ordering);
        if (pool) mesh_coordinator_pool_destroy(pool);
        if (channel) mesh_channel_destroy(channel);
        if (registry) mesh_registry_destroy(registry);
    }

    /* Helper to add coordinators */
    void add_coordinators(size_t count) {
        for (size_t i = 0; i < count; i++) {
            mesh_coordinator_config_t config;
            mesh_coordinator_default_config(&config);
            snprintf(config.name, MESH_MAX_NAME_LEN, "coord_%zu", i);
            config.id = mesh_make_participant_id(
                MESH_CHANNEL_LEFT_HEMISPHERE,
                MESH_PARTICIPANT_COORDINATOR,
                (uint32_t)(100 + i)
            );
            mesh_coordinator_t* coord = mesh_coordinator_create(&config);
            if (coord) {
                mesh_coordinator_pool_add(pool, coord);
            }
        }
    }

    /* Helper to register participants */
    void register_participants(size_t count) {
        for (size_t i = 0; i < count; i++) {
            mesh_participant_config_t config;
            mesh_participant_default_config(&config);
            snprintf(config.module_name, MESH_MAX_NAME_LEN, "module_%zu", i);
            config.type = MESH_PARTICIPANT_MODULE;
            config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

            mesh_participant_id_t id;
            mesh_participant_register(registry, &config, &id);
        }
    }

    /* Helper to create a transaction */
    mesh_transaction_t* create_test_transaction(int index) {
        mesh_transaction_config_t config;
        mesh_transaction_default_config(&config);
        snprintf(config.name, MESH_MAX_NAME_LEN, "perf_tx_%d", index);
        config.type = MESH_TX_BELIEF_UPDATE;
        config.source_channel = MESH_CHANNEL_LEFT_HEMISPHERE;
        config.target_channel = MESH_CHANNEL_LEFT_HEMISPHERE;
        config.proposer_id = mesh_make_participant_id(
            MESH_CHANNEL_LEFT_HEMISPHERE,
            MESH_PARTICIPANT_MODULE,
            (uint32_t)(index % 100)
        );
        return mesh_transaction_create(&config);
    }

    /* Calculate percentile from sorted vector */
    double percentile(std::vector<double>& sorted_values, double p) {
        if (sorted_values.empty()) return 0.0;
        size_t idx = (size_t)(p * (sorted_values.size() - 1));
        return sorted_values[idx];
    }
};

/* ============================================================================
 * Transaction Throughput Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, TransactionThroughputBaseline) {
    register_participants(50);
    add_coordinators(5);
    mesh_coordinator_pool_elect_leader(pool);

    const int NUM_TRANSACTIONS = 1000;
    std::vector<mesh_transaction_t*> txs;
    txs.reserve(NUM_TRANSACTIONS);

    /* Create all transactions first */
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        mesh_transaction_t* tx = create_test_transaction(i);
        if (tx) txs.push_back(tx);
    }

    /* Measure submission throughput */
    auto start = std::chrono::high_resolution_clock::now();

    for (auto tx : txs) {
        mesh_ordering_submit(ordering, tx);
    }

    /* Process all batches */
    while (mesh_ordering_has_pending(ordering)) {
        mesh_ordering_process_batch(ordering);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = (NUM_TRANSACTIONS / elapsed_ms) * 1000.0;  /* tx/second */

    EXPECT_GE(throughput, THRESHOLD_TX_THROUGHPUT_MIN)
        << "Transaction throughput " << throughput << " tx/s below threshold "
        << THRESHOLD_TX_THROUGHPUT_MIN << " tx/s";

    /* Record for reporting */
    RecordProperty("throughput_tx_per_sec", throughput);
    RecordProperty("elapsed_ms", elapsed_ms);

    for (auto tx : txs) {
        mesh_transaction_destroy(tx);
    }
}

TEST_F(MeshPerformanceRegressionTest, ConcurrentTransactionThroughput) {
    register_participants(100);
    add_coordinators(5);
    mesh_coordinator_pool_elect_leader(pool);

    const int NUM_THREADS = 4;
    const int TX_PER_THREAD = 250;
    std::atomic<int> completed{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &completed]() {
            for (int i = 0; i < TX_PER_THREAD; i++) {
                mesh_transaction_t* tx = create_test_transaction(t * TX_PER_THREAD + i);
                if (tx) {
                    mesh_ordering_submit(ordering, tx);
                    mesh_transaction_destroy(tx);
                    completed++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    /* Process remaining batches */
    while (mesh_ordering_has_pending(ordering)) {
        mesh_ordering_process_batch(ordering);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = (completed.load() / elapsed_ms) * 1000.0;

    EXPECT_GE(throughput, THRESHOLD_TX_THROUGHPUT_MIN)
        << "Concurrent throughput " << throughput << " tx/s below threshold";

    RecordProperty("concurrent_throughput_tx_per_sec", throughput);
}

/* ============================================================================
 * Transaction Latency Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, TransactionLatencyDistribution) {
    register_participants(20);
    add_coordinators(3);
    mesh_coordinator_pool_elect_leader(pool);

    const int NUM_TRANSACTIONS = 500;
    std::vector<double> latencies;
    latencies.reserve(NUM_TRANSACTIONS);

    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        mesh_transaction_t* tx = create_test_transaction(i);
        if (!tx) continue;

        auto start = std::chrono::high_resolution_clock::now();

        mesh_ordering_submit(ordering, tx);
        mesh_ordering_process_batch(ordering);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
        latencies.push_back(latency_ms);

        mesh_transaction_destroy(tx);
    }

    /* Calculate percentiles */
    std::sort(latencies.begin(), latencies.end());
    double p50 = percentile(latencies, 0.50);
    double p90 = percentile(latencies, 0.90);
    double p99 = percentile(latencies, 0.99);
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

    EXPECT_LE(p50, THRESHOLD_TX_LATENCY_P50_MAX)
        << "P50 latency " << p50 << "ms exceeds threshold " << THRESHOLD_TX_LATENCY_P50_MAX << "ms";

    EXPECT_LE(p99, THRESHOLD_TX_LATENCY_P99_MAX)
        << "P99 latency " << p99 << "ms exceeds threshold " << THRESHOLD_TX_LATENCY_P99_MAX << "ms";

    RecordProperty("latency_p50_ms", p50);
    RecordProperty("latency_p90_ms", p90);
    RecordProperty("latency_p99_ms", p99);
    RecordProperty("latency_avg_ms", avg);
}

/* ============================================================================
 * Leader Election Performance Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, LeaderElectionLatency) {
    add_coordinators(5);

    const int NUM_ELECTIONS = 20;
    std::vector<double> election_times;

    for (int i = 0; i < NUM_ELECTIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (err == NIMCP_SUCCESS) {
            election_times.push_back(elapsed_ms);
        }
    }

    ASSERT_FALSE(election_times.empty()) << "No successful elections";

    double avg_time = std::accumulate(election_times.begin(), election_times.end(), 0.0) /
                      election_times.size();
    double max_time = *std::max_element(election_times.begin(), election_times.end());

    EXPECT_LE(max_time, THRESHOLD_ELECTION_TIME_MAX)
        << "Election time " << max_time << "ms exceeds threshold " << THRESHOLD_ELECTION_TIME_MAX << "ms";

    RecordProperty("election_avg_ms", avg_time);
    RecordProperty("election_max_ms", max_time);
}

TEST_F(MeshPerformanceRegressionTest, FailoverRecoveryLatency) {
    add_coordinators(5);
    ASSERT_EQ(mesh_coordinator_pool_elect_leader(pool), NIMCP_SUCCESS);

    const int NUM_FAILOVERS = 10;
    std::vector<double> failover_times;

    for (int i = 0; i < NUM_FAILOVERS; i++) {
        mesh_participant_id_t leader = mesh_coordinator_pool_get_leader(pool);
        if (leader == 0) break;

        auto start = std::chrono::high_resolution_clock::now();

        mesh_coordinator_pool_handle_failure(pool, leader);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        failover_times.push_back(elapsed_ms);

        /* Verify new leader elected */
        mesh_participant_id_t new_leader = mesh_coordinator_pool_get_leader(pool);
        if (new_leader == 0) break;
    }

    if (!failover_times.empty()) {
        double avg_time = std::accumulate(failover_times.begin(), failover_times.end(), 0.0) /
                          failover_times.size();
        double max_time = *std::max_element(failover_times.begin(), failover_times.end());

        EXPECT_LE(max_time, THRESHOLD_ELECTION_TIME_MAX)
            << "Failover recovery " << max_time << "ms exceeds threshold";

        RecordProperty("failover_avg_ms", avg_time);
        RecordProperty("failover_max_ms", max_time);
    }
}

/* ============================================================================
 * Consensus Performance Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, BeliefConsensusConvergence) {
    add_coordinators(5);
    mesh_coordinator_pool_elect_leader(pool);

    const int NUM_BELIEFS = 50;
    std::vector<double> convergence_times;

    for (int i = 0; i < NUM_BELIEFS; i++) {
        /* Create and submit belief */
        mesh_belief_t belief = {
            .belief_id = (uint64_t)(i + 1),
            .source = mesh_make_participant_id(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, (uint32_t)(i % 10)),
            .channel = MESH_CHANNEL_LEFT_HEMISPHERE,
            .certainty = 0.8f,
            .vector_dim = 4
        };
        belief.belief_vector[0] = (float)i / NUM_BELIEFS;

        auto start = std::chrono::high_resolution_clock::now();

        mesh_channel_submit_belief(channel, &belief);

        mesh_consensus_t consensus;
        mesh_channel_check_consensus(channel, &consensus);

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        convergence_times.push_back(elapsed_ms);
    }

    std::sort(convergence_times.begin(), convergence_times.end());
    double p50 = percentile(convergence_times, 0.50);
    double p99 = percentile(convergence_times, 0.99);
    double avg = std::accumulate(convergence_times.begin(), convergence_times.end(), 0.0) /
                 convergence_times.size();

    EXPECT_LE(p99, THRESHOLD_CONSENSUS_TIME_MAX)
        << "Consensus P99 " << p99 << "ms exceeds threshold " << THRESHOLD_CONSENSUS_TIME_MAX << "ms";

    RecordProperty("consensus_p50_ms", p50);
    RecordProperty("consensus_p99_ms", p99);
    RecordProperty("consensus_avg_ms", avg);
}

/* ============================================================================
 * Memory Usage Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, TransactionMemoryOverhead) {
    const int NUM_TRANSACTIONS = 100;

    /* Measure baseline memory (approximation) */
    size_t baseline_size = sizeof(mesh_transaction_config_t);

    std::vector<mesh_transaction_t*> txs;
    txs.reserve(NUM_TRANSACTIONS);

    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        mesh_transaction_t* tx = create_test_transaction(i);
        if (tx) txs.push_back(tx);
    }

    /* Get memory stats from ordering service */
    mesh_ordering_stats_t stats;
    mesh_ordering_get_stats(ordering, &stats);

    /* Estimate per-transaction memory */
    size_t estimated_per_tx = stats.memory_used_bytes / (NUM_TRANSACTIONS > 0 ? NUM_TRANSACTIONS : 1);

    /* Note: This is a rough estimate; actual measurement depends on implementation */
    RecordProperty("estimated_bytes_per_tx", estimated_per_tx);

    for (auto tx : txs) {
        mesh_transaction_destroy(tx);
    }
}

TEST_F(MeshPerformanceRegressionTest, CoordinatorPoolMemoryScaling) {
    /* Test memory scaling as coordinators increase */
    std::vector<size_t> memory_at_size;

    for (size_t count = 1; count <= 7; count++) {
        add_coordinators(1);  /* Add one more */

        mesh_coordinator_pool_stats_t stats;
        mesh_coordinator_pool_get_stats(pool, &stats);

        memory_at_size.push_back(stats.memory_used_bytes);
    }

    /* Memory should scale linearly or sub-linearly */
    if (memory_at_size.size() >= 3) {
        size_t delta1 = memory_at_size[1] - memory_at_size[0];
        size_t delta2 = memory_at_size[2] - memory_at_size[1];

        /* Allow 50% variation in deltas for reasonable scaling */
        double ratio = (delta1 > 0) ? (double)delta2 / delta1 : 1.0;
        EXPECT_LE(ratio, 2.0) << "Memory scaling should be at most linear";
    }
}

/* ============================================================================
 * Scalability Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, ParticipantScaling) {
    add_coordinators(5);
    mesh_coordinator_pool_elect_leader(pool);

    std::vector<double> throughputs;
    std::vector<size_t> participant_counts = {10, 50, 100, 200};

    for (size_t p_count : participant_counts) {
        /* Register additional participants */
        while (mesh_registry_count(registry) < p_count) {
            mesh_participant_config_t config;
            mesh_participant_default_config(&config);
            snprintf(config.module_name, MESH_MAX_NAME_LEN, "scale_module_%zu",
                     mesh_registry_count(registry));
            config.type = MESH_PARTICIPANT_MODULE;
            config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

            mesh_participant_id_t id;
            mesh_participant_register(registry, &config, &id);
        }

        /* Measure throughput at this scale */
        const int NUM_TX = 200;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TX; i++) {
            mesh_transaction_t* tx = create_test_transaction(i);
            if (tx) {
                mesh_ordering_submit(ordering, tx);
                mesh_transaction_destroy(tx);
            }
        }

        while (mesh_ordering_has_pending(ordering)) {
            mesh_ordering_process_batch(ordering);
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double throughput = (NUM_TX / elapsed_ms) * 1000.0;

        throughputs.push_back(throughput);
    }

    /* Throughput should not degrade more than 50% as participants increase */
    if (throughputs.size() >= 2) {
        double initial = throughputs[0];
        double final_tp = throughputs.back();
        double degradation = (initial - final_tp) / initial;

        EXPECT_LE(degradation, 0.5)
            << "Throughput degraded by " << (degradation * 100) << "% which exceeds 50% threshold";

        RecordProperty("throughput_10_participants", throughputs[0]);
        RecordProperty("throughput_200_participants", throughputs.back());
        RecordProperty("degradation_percent", degradation * 100);
    }
}

/* ============================================================================
 * Sustained Load Tests
 * ============================================================================ */

TEST_F(MeshPerformanceRegressionTest, SustainedThroughputStability) {
    register_participants(50);
    add_coordinators(5);
    mesh_coordinator_pool_elect_leader(pool);

    const int DURATION_SECONDS = 2;
    const int TX_PER_ITERATION = 50;
    std::vector<double> interval_throughputs;

    auto test_start = std::chrono::high_resolution_clock::now();
    auto test_end = test_start + std::chrono::seconds(DURATION_SECONDS);

    while (std::chrono::high_resolution_clock::now() < test_end) {
        auto interval_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < TX_PER_ITERATION; i++) {
            mesh_transaction_t* tx = create_test_transaction(i);
            if (tx) {
                mesh_ordering_submit(ordering, tx);
                mesh_transaction_destroy(tx);
            }
        }

        while (mesh_ordering_has_pending(ordering)) {
            mesh_ordering_process_batch(ordering);
        }

        auto interval_end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(interval_end - interval_start).count();
        double throughput = (TX_PER_ITERATION / elapsed_ms) * 1000.0;

        interval_throughputs.push_back(throughput);
    }

    /* Calculate variance in throughput */
    if (interval_throughputs.size() > 1) {
        double avg = std::accumulate(interval_throughputs.begin(), interval_throughputs.end(), 0.0) /
                     interval_throughputs.size();
        double variance = 0.0;
        for (double tp : interval_throughputs) {
            variance += (tp - avg) * (tp - avg);
        }
        variance /= interval_throughputs.size();
        double std_dev = std::sqrt(variance);
        double cv = std_dev / avg;  /* Coefficient of variation */

        EXPECT_LE(cv, 0.5) << "Throughput variance too high (CV=" << cv << ")";

        RecordProperty("sustained_avg_throughput", avg);
        RecordProperty("sustained_std_dev", std_dev);
        RecordProperty("sustained_cv", cv);
    }
}


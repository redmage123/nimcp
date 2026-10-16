/**
 * @file test_fault_event_bus_integration.cpp
 * @brief Integration Tests for Event Bus with Fault Tolerance Modules
 *
 * WHAT: End-to-end integration tests for event bus in fault tolerance system
 * WHY:  Validate module decoupling and event-driven workflows
 * HOW:  Simulate real module interactions using event bus
 *
 * SCENARIOS:
 * - Diagnostics → Recovery workflow
 * - Health Monitor subscribes to all events
 * - Checkpoint → Recovery integration
 * - Multi-module event cascades
 * - High-load performance testing
 */

#include <gtest/gtest.h>
#include "core/events/nimcp_event_bus.h"
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <chrono>

//=============================================================================
// Mock Module Contexts
//=============================================================================

// Diagnostics module simulation
struct DiagnosticsModule {
    event_bus_t bus;
    std::atomic<int> errors_detected{0};
    std::atomic<int> diagnostics_run{0};

    void detect_error(const char* error_type) {
        errors_detected++;

        // Publish ERROR_DETECTED event
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_HIGH, "diagnostics");

        // Run diagnostics
        diagnostics_run++;

        // Publish DIAGNOSTICS_COMPLETE event
        event_bus_publish_simple(bus, EVENT_DIAGNOSTICS_COMPLETE,
                                EVENT_PRIORITY_NORMAL, "diagnostics");
    }
};

// Recovery module simulation
struct RecoveryModule {
    event_bus_t bus;
    std::atomic<int> recoveries_started{0};
    std::atomic<int> recoveries_completed{0};
    event_subscription_handle_t error_handle;

    static void on_error_detected(const brain_event_t* event, void* context) {
        RecoveryModule* module = (RecoveryModule*)context;
        module->start_recovery();
    }

    void subscribe() {
        error_handle = event_bus_subscribe(
            bus, EVENT_ERROR_DETECTED, on_error_detected, this);
    }

    void start_recovery() {
        recoveries_started++;

        // Publish RECOVERY_STARTED
        event_bus_publish_simple(bus, EVENT_RECOVERY_STARTED,
                                EVENT_PRIORITY_HIGH, "recovery");

        // Simulate recovery work
        usleep(1000); // 1ms

        // Publish RECOVERY_COMPLETE
        recoveries_completed++;
        event_bus_publish_simple(bus, EVENT_RECOVERY_COMPLETE,
                                EVENT_PRIORITY_NORMAL, "recovery");
    }
};

// Health Monitor module simulation
struct HealthMonitorModule {
    event_bus_t bus;
    std::atomic<int> events_received{0};
    std::atomic<int> errors_tracked{0};
    std::atomic<int> recoveries_tracked{0};
    event_subscription_handle_t all_events_handle;

    static void on_any_event(const brain_event_t* event, void* context) {
        HealthMonitorModule* module = (HealthMonitorModule*)context;
        module->track_event(event);
    }

    void subscribe() {
        all_events_handle = event_bus_subscribe(
            bus, EVENT_ALL, on_any_event, this);
    }

    void track_event(const brain_event_t* event) {
        events_received++;

        switch (event->type) {
            case EVENT_ERROR_DETECTED:
                errors_tracked++;
                break;
            case EVENT_RECOVERY_COMPLETE:
                recoveries_tracked++;
                break;
            default:
                break;
        }
    }
};

// Checkpoint module simulation
struct CheckpointModule {
    event_bus_t bus;
    std::atomic<int> checkpoints_created{0};
    std::atomic<int> checkpoints_loaded{0};
    event_subscription_handle_t recovery_handle;

    static void on_recovery_started(const brain_event_t* event, void* context) {
        CheckpointModule* module = (CheckpointModule*)context;
        module->load_checkpoint();
    }

    void subscribe() {
        recovery_handle = event_bus_subscribe(
            bus, EVENT_RECOVERY_STARTED, on_recovery_started, this);
    }

    void create_checkpoint() {
        checkpoints_created++;
        event_bus_publish_simple(bus, EVENT_CHECKPOINT_CREATED,
                                EVENT_PRIORITY_NORMAL, "checkpoint");
    }

    void load_checkpoint() {
        checkpoints_loaded++;
        event_bus_publish_simple(bus, EVENT_CHECKPOINT_LOADED,
                                EVENT_PRIORITY_HIGH, "checkpoint");
    }
};

//=============================================================================
// Integration Test Fixture
//=============================================================================

class EventBusIntegrationTest : public ::testing::Test {
protected:
    event_bus_t bus;

    void SetUp() override {
        bus = event_bus_create("integration_test", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(nullptr, bus);
    }

    void TearDown() override {
        if (bus) event_bus_destroy(bus);
    }
};

//=============================================================================
// Module Integration Tests
//=============================================================================

TEST_F(EventBusIntegrationTest, DiagnosticsRecoveryWorkflow_EndToEnd_Success) {
    // Setup modules
    DiagnosticsModule diagnostics;
    diagnostics.bus = bus;

    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Simulate error detection
    diagnostics.detect_error("memory_corruption");

    // Verify workflow
    ASSERT_EQ(1, diagnostics.errors_detected);
    ASSERT_EQ(1, diagnostics.diagnostics_run);
    ASSERT_EQ(1, recovery.recoveries_started);
    ASSERT_EQ(1, recovery.recoveries_completed);

    // Health monitor should see all events
    ASSERT_EQ(4, health.events_received); // ERROR_DETECTED, DIAGNOSTICS_COMPLETE,
                                           // RECOVERY_STARTED, RECOVERY_COMPLETE
    ASSERT_EQ(1, health.errors_tracked);
    ASSERT_EQ(1, health.recoveries_tracked);
}

TEST_F(EventBusIntegrationTest, CheckpointRecoveryIntegration_Success) {
    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    CheckpointModule checkpoint;
    checkpoint.bus = bus;
    checkpoint.subscribe();

    // Create checkpoint
    checkpoint.create_checkpoint();

    // Trigger error (will start recovery, which loads checkpoint)
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "test");

    ASSERT_EQ(1, recovery.recoveries_started);
    ASSERT_EQ(1, checkpoint.checkpoints_loaded);
}

TEST_F(EventBusIntegrationTest, MultiModuleEventCascade_Success) {
    DiagnosticsModule diagnostics;
    diagnostics.bus = bus;

    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    CheckpointModule checkpoint;
    checkpoint.bus = bus;
    checkpoint.subscribe();

    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Trigger cascade
    diagnostics.detect_error("numerical_instability");

    // Verify all modules participated
    ASSERT_EQ(1, diagnostics.errors_detected);
    ASSERT_EQ(1, recovery.recoveries_started);
    ASSERT_EQ(1, checkpoint.checkpoints_loaded);
    ASSERT_GT(health.events_received, 0);
}

TEST_F(EventBusIntegrationTest, HealthMonitorTracksAllEvents_Success) {
    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Publish various events
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "test");
    event_bus_publish_simple(bus, EVENT_RECOVERY_STARTED,
                            EVENT_PRIORITY_NORMAL, "test");
    event_bus_publish_simple(bus, EVENT_CHECKPOINT_CREATED,
                            EVENT_PRIORITY_LOW, "test");
    event_bus_publish_simple(bus, EVENT_HEALTH_DEGRADED,
                            EVENT_PRIORITY_HIGH, "test");

    ASSERT_EQ(4, health.events_received);
}

//=============================================================================
// Decoupling Tests
//=============================================================================

TEST_F(EventBusIntegrationTest, ModuleDecoupling_NoDirectDependencies_Success) {
    // Recovery doesn't need to know about Diagnostics
    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    // Directly publish error (simulating any source)
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "unknown_source");

    ASSERT_EQ(1, recovery.recoveries_started);
}

TEST_F(EventBusIntegrationTest, DynamicModuleAddRemove_Success) {
    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Publish event
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "test");
    ASSERT_EQ(1, health.events_received);

    // Add recovery module mid-flight
    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    // Publish another event
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "test");

    ASSERT_EQ(2, health.events_received);
    ASSERT_EQ(1, recovery.recoveries_started);

    // Remove recovery
    event_bus_unsubscribe(bus, recovery.error_handle);

    // Publish event (recovery shouldn't respond)
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_HIGH, "test");

    ASSERT_EQ(3, health.events_received);
    ASSERT_EQ(1, recovery.recoveries_started); // Still 1
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(EventBusIntegrationTest, HighLoadPublish_1000Events_Success) {
    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "stress_test");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ASSERT_EQ(1000, health.events_received);
    EXPECT_LT(duration.count(), 1000); // Should complete in < 1 second
}

TEST_F(EventBusIntegrationTest, HighLoadSubscribers_100Subscribers_Success) {
    std::vector<HealthMonitorModule> monitors(100);

    for (auto& monitor : monitors) {
        monitor.bus = bus;
        monitor.subscribe();
    }

    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                            EVENT_PRIORITY_NORMAL, "test");

    for (const auto& monitor : monitors) {
        ASSERT_EQ(1, monitor.events_received);
    }
}

TEST_F(EventBusIntegrationTest, HighLoadConcurrentPublish_MultipleModules_Success) {
    const int NUM_PUBLISHERS = 10;
    const int EVENTS_PER_PUBLISHER = 100;

    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    std::vector<DiagnosticsModule> publishers(NUM_PUBLISHERS);
    std::vector<pthread_t> threads(NUM_PUBLISHERS);

    auto publish_func = [](void* arg) -> void* {
        DiagnosticsModule* module = (DiagnosticsModule*)arg;
        for (int i = 0; i < EVENTS_PER_PUBLISHER; i++) {
            module->detect_error("test");
        }
        return nullptr;
    };

    // Launch publishers
    for (int i = 0; i < NUM_PUBLISHERS; i++) {
        publishers[i].bus = bus;
        pthread_create(&threads[i], nullptr,
                      [](void* arg) -> void* {
                          DiagnosticsModule* module = (DiagnosticsModule*)arg;
                          for (int i = 0; i < EVENTS_PER_PUBLISHER; i++) {
                              module->detect_error("test");
                          }
                          return nullptr;
                      }, &publishers[i]);
    }

    // Wait for completion
    for (auto& thread : threads) {
        pthread_join(thread, nullptr);
    }

    // Each publisher emits 2 events per detect_error
    int expected_events = NUM_PUBLISHERS * EVENTS_PER_PUBLISHER * 2;
    ASSERT_EQ(expected_events, health.events_received);
}

//=============================================================================
// Async Mode Integration Tests
//=============================================================================

class AsyncEventBusIntegrationTest : public ::testing::Test {
protected:
    event_bus_t bus;

    void SetUp() override {
        bus = event_bus_create("async_integration", EVENT_DELIVERY_ASYNC);
        ASSERT_NE(nullptr, bus);
        ASSERT_TRUE(event_bus_start(bus));
    }

    void TearDown() override {
        if (bus) {
            event_bus_stop(bus, true);
            event_bus_destroy(bus);
        }
    }
};

TEST_F(AsyncEventBusIntegrationTest, AsyncWorkflow_EventualConsistency_Success) {
    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Publish events asynchronously
    for (int i = 0; i < 10; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_HIGH, "async_test");
    }

    // Wait for processing
    usleep(100000); // 100ms

    ASSERT_EQ(10, recovery.recoveries_started);
    ASSERT_GE(health.events_received, 20); // At least 20 (10 errors + 10 recoveries)
}

TEST_F(AsyncEventBusIntegrationTest, AsyncQueueOverflow_DropsEvents_Success) {
    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Stop worker to fill queue
    event_bus_stop(bus, false);

    // Publish more than queue size
    int published = 0;
    for (int i = 0; i < EVENT_BUS_QUEUE_SIZE + 100; i++) {
        if (event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                     EVENT_PRIORITY_NORMAL, "overflow")) {
            published++;
        }
    }

    // Should have dropped some
    ASSERT_LE(published, EVENT_BUS_QUEUE_SIZE);

    // Verify stats show drops
    event_bus_stats_t stats;
    event_bus_get_stats(bus, &stats);
    ASSERT_GT(stats.total_events_dropped, 0);
}

//=============================================================================
// Priority Ordering Tests
//=============================================================================

struct PriorityTestContext {
    std::vector<event_priority_t> priorities;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

static void priority_callback(const brain_event_t* event, void* context) {
    PriorityTestContext* ctx = (PriorityTestContext*)context;
    pthread_mutex_lock(&ctx->mutex);
    ctx->priorities.push_back(event->priority);
    pthread_mutex_unlock(&ctx->mutex);
}

TEST_F(EventBusIntegrationTest, PriorityFiltering_MultipleSubscribers_Success) {
    PriorityTestContext ctx_all, ctx_high, ctx_critical;

    // Subscribe with different priority filters
    event_bus_subscribe_priority(bus, EVENT_ERROR_DETECTED,
                                 EVENT_PRIORITY_LOW, priority_callback, &ctx_all);
    event_bus_subscribe_priority(bus, EVENT_ERROR_DETECTED,
                                 EVENT_PRIORITY_HIGH, priority_callback, &ctx_high);
    event_bus_subscribe_priority(bus, EVENT_ERROR_DETECTED,
                                 EVENT_PRIORITY_CRITICAL, priority_callback, &ctx_critical);

    // Publish events with different priorities
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED, EVENT_PRIORITY_LOW, "test");
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED, EVENT_PRIORITY_HIGH, "test");
    event_bus_publish_simple(bus, EVENT_ERROR_DETECTED, EVENT_PRIORITY_CRITICAL, "test");

    // Verify filtering
    ASSERT_EQ(3, ctx_all.priorities.size()); // Receives all
    ASSERT_EQ(2, ctx_high.priorities.size()); // Receives high + critical
    ASSERT_EQ(1, ctx_critical.priorities.size()); // Receives only critical
}

//=============================================================================
// Error Recovery Integration
//=============================================================================

TEST_F(EventBusIntegrationTest, ErrorRecoveryLoop_MultipleIterations_Success) {
    DiagnosticsModule diagnostics;
    diagnostics.bus = bus;

    RecoveryModule recovery;
    recovery.bus = bus;
    recovery.subscribe();

    // Simulate multiple error-recovery cycles
    for (int i = 0; i < 5; i++) {
        diagnostics.detect_error("transient_error");
    }

    ASSERT_EQ(5, diagnostics.errors_detected);
    ASSERT_EQ(5, recovery.recoveries_started);
    ASSERT_EQ(5, recovery.recoveries_completed);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(EventBusIntegrationTest, StressTest_RapidSubscribePublishUnsubscribe_Success) {
    for (int iteration = 0; iteration < 10; iteration++) {
        HealthMonitorModule health;
        health.bus = bus;
        health.subscribe();

        for (int i = 0; i < 100; i++) {
            event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                    EVENT_PRIORITY_NORMAL, "stress");
        }

        ASSERT_EQ(100, health.events_received);

        event_bus_unsubscribe(bus, health.all_events_handle);
    }
}

TEST_F(EventBusIntegrationTest, StressTest_ManyEventTypes_Success) {
    HealthMonitorModule health;
    health.bus = bus;
    health.subscribe();

    // Publish different event types
    brain_event_type_t types[] = {
        EVENT_ERROR_DETECTED,
        EVENT_RECOVERY_STARTED,
        EVENT_CHECKPOINT_CREATED,
        EVENT_HEALTH_DEGRADED,
        EVENT_ANOMALY_DETECTED,
        EVENT_DIAGNOSTICS_COMPLETE,
        EVENT_ROLLBACK_STARTED,
        EVENT_SYSTEM_STARTED
    };

    for (int i = 0; i < 100; i++) {
        for (auto type : types) {
            event_bus_publish_simple(bus, type, EVENT_PRIORITY_NORMAL, "stress");
        }
    }

    ASSERT_EQ(100 * 8, health.events_received);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

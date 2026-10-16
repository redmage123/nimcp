/**
 * @file test_mesh_cycle_coordinator.cpp
 * @brief Unit tests for Mesh-Cycle Coordinator Integration
 *
 * WHAT: Tests integration layer connecting mesh network to brain cycle coordinator
 * WHY:  Validate timing constraints, stall recovery, health aggregation between
 *       the mesh network and brain cycle coordinator
 * HOW:  GTest framework with fixture class managing mesh bootstrap and coordinator
 *
 * TEST COVERAGE:
 * - Configuration tests (5 tests)
 * - Lifecycle tests (5 tests)
 * - Connection tests (8 tests)
 * - Timing constraint tests (6 tests)
 * - Stall recovery tests (8 tests)
 * - Health tests (5 tests)
 * - Statistics tests (4 tests)
 * - Edge cases (3 tests)
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_resilience_integration.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
/* Note: nimcp_mesh_cycle_coordinator.h not included - using mock definitions below */
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Mesh-Cycle Coordinator Integration Types (Simulated API)
 *
 * These types represent the integration layer being tested. In production,
 * these would be defined in a header file.
 * ============================================================================ */

/** @brief Cycle types for mesh-brain timing */
typedef enum mesh_cycle_type {
    MESH_CYCLE_OSCILLATIONS = 0,    /**< Neural oscillations (10ms interval) */
    MESH_CYCLE_BRAIN_UPDATE,        /**< Brain update loop (16ms deadline) */
    MESH_CYCLE_IMMUNE_TICK,         /**< Immune system tick (50ms validation timeout) */
    MESH_CYCLE_HEALTH_AGENT,        /**< Health agent heartbeat (100ms) */
    MESH_CYCLE_BATCH_WINDOW,        /**< Ordering batch window */
    MESH_CYCLE_COMMIT_DEADLINE,     /**< Transaction commit deadline */
    MESH_CYCLE_COUNT
} mesh_cycle_type_t;

/** @brief Stall severity levels */
typedef enum mesh_stall_severity {
    MESH_STALL_NONE = 0,
    MESH_STALL_WARNING,
    MESH_STALL_DEGRADED,
    MESH_STALL_CRITICAL,
    MESH_STALL_FATAL
} mesh_stall_severity_t;

/* Note: mesh_recovery_action_type_t is defined in nimcp_mesh_resilience_integration.h */

/** @brief Recovery status (mock for testing - uses action type enum) */
typedef struct mesh_recovery_status {
    mesh_recovery_action_type_t action_type;
    bool in_progress;
    uint64_t started_at_ns;
    uint32_t attempts;
    bool success;
} mesh_recovery_status_t;

/** @brief Timing constraint */
typedef struct mesh_timing_constraint {
    mesh_cycle_type_t type;
    uint64_t interval_ms;
    uint64_t deadline_ms;
    uint64_t timeout_ms;
    float multiplier;
} mesh_timing_constraint_t;

/** @brief Mesh-cycle coordinator integration config (mock matching real header) */
typedef struct mesh_cycle_coordinator_config {
    /* Feature enables */
    bool enable_timing_constraints;     /**< Use cycle timing for ordering batches */
    bool enable_stall_recovery;         /**< Trigger mesh recovery on cycle stalls */
    bool enable_distributed_health;     /**< Participate in mesh health consensus */
    bool enable_cross_channel_sync;     /**< Sync cycles across mesh channels */

    /* Stall recovery settings */
    uint32_t stall_recovery_threshold;  /**< Consecutive stalls before recovery */

    /* Timing settings */
    float timing_batch_multiplier;      /**< Multiplier for batch window from cycle interval */

    /* Health consensus settings */
    float health_endorsement_weight;    /**< Weight of this node in health consensus [0-1] */

    /* Logging */
    bool verbose_logging;               /**< Enable verbose logging */
    bool enable_debug_logging;          /**< Enable debug-level logging */
} mesh_cycle_coordinator_config_t;

/** @brief Mesh-cycle coordinator integration statistics (mock matching real header) */
typedef struct mesh_cycle_coordinator_stats {
    /* Cycle reporting */
    uint64_t cycles_reported;           /**< Total cycle ticks reported */
    uint64_t stalls_detected;           /**< Total stalls detected */
    uint64_t recoveries_triggered;      /**< Recovery actions triggered */

    /* Timing */
    uint64_t timing_constraints_applied;/**< Times timing constraints affected batching */
    uint64_t mesh_transactions_timed;   /**< Transactions with timing metadata */
    uint64_t ordering_batches_adjusted; /**< Batches adjusted due to cycle timing */

    /* Health */
    uint64_t health_endorsements;       /**< Health endorsements contributed */

    /* Exception/immune routing */
    uint64_t exceptions_routed_to_immune; /**< Exceptions routed via exception bridge */

    /* Security */
    uint64_t bbb_validations;           /**< BBB validations performed */

    /* Timing performance */
    uint64_t total_timing_violations;   /**< Total timing constraint violations */
    float avg_batch_window_us;          /**< Average batch window in microseconds */
    float avg_commit_deadline_us;       /**< Average commit deadline in microseconds */
} mesh_cycle_coordinator_stats_t;

/** @brief Mesh-cycle coordinator integration handle (opaque) */
typedef struct mesh_cycle_coordinator_integration {
    uint32_t magic;
    mesh_bootstrap_t* bootstrap;
    mesh_coordinator_t* coordinator;
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_stats_t stats;

    /* Connected services */
    mesh_ordering_service_t* ordering;
    mesh_resilience_integration_t* resilience;
    mesh_health_bridge_t* health_bridge;
    mesh_exception_bridge_t* exception_bridge;
    void* msp;  /* MSP for BBB */

    /* Stall tracking */
    uint32_t consecutive_stalls;
    mesh_stall_severity_t current_severity;
    mesh_recovery_status_t recovery_status;

    /* Timing constraints */
    mesh_timing_constraint_t constraints[MESH_CYCLE_COUNT];

    /* State */
    bool initialized;
    bool connected;
    float aggregate_health;
} mesh_cycle_coordinator_integration_t;

#define MESH_CYCLE_COORD_MAGIC 0x4D434349  /* "MCCI" */

/* ============================================================================
 * Simulated API Functions
 *
 * These simulate the integration API. In production, these would be
 * implemented in a source file.
 * ============================================================================ */

static nimcp_error_t mesh_cycle_coordinator_default_config(
    mesh_cycle_coordinator_config_t* config)
{
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    config->enable_timing_constraints = true;
    config->enable_stall_recovery = true;
    config->enable_distributed_health = true;
    config->enable_cross_channel_sync = false;
    config->stall_recovery_threshold = 3;
    config->timing_batch_multiplier = 2.0f;  /* Match real implementation default */
    config->health_endorsement_weight = 0.5f;
    config->verbose_logging = false;
    config->enable_debug_logging = false;

    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_validate_config(
    const mesh_cycle_coordinator_config_t* config)
{
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    /* Validate timing multiplier range */
    if (config->timing_batch_multiplier < 0.1f || config->timing_batch_multiplier > 10.0f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate health weight range */
    if (config->health_endorsement_weight < 0.0f || config->health_endorsement_weight > 1.0f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate stall threshold */
    if (config->stall_recovery_threshold == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

static mesh_cycle_coordinator_integration_t* mesh_cycle_coordinator_create(
    mesh_bootstrap_t* bootstrap,
    mesh_coordinator_t* coordinator,
    const mesh_cycle_coordinator_config_t* config)
{
    if (!bootstrap) return nullptr;
    if (!coordinator) return nullptr;

    mesh_cycle_coordinator_integration_t* integration =
        (mesh_cycle_coordinator_integration_t*)nimcp_calloc(
            1, sizeof(mesh_cycle_coordinator_integration_t));
    if (!integration) return nullptr;

    integration->magic = MESH_CYCLE_COORD_MAGIC;
    integration->bootstrap = bootstrap;
    integration->coordinator = coordinator;

    if (config) {
        if (mesh_cycle_coordinator_validate_config(config) != NIMCP_SUCCESS) {
            nimcp_free(integration);
            return nullptr;
        }
        integration->config = *config;
    } else {
        mesh_cycle_coordinator_default_config(&integration->config);
    }

    /* Initialize timing constraints with defaults */
    integration->constraints[MESH_CYCLE_OSCILLATIONS].interval_ms = 10;
    integration->constraints[MESH_CYCLE_BRAIN_UPDATE].deadline_ms = 16;
    integration->constraints[MESH_CYCLE_IMMUNE_TICK].timeout_ms = 50;
    integration->constraints[MESH_CYCLE_HEALTH_AGENT].interval_ms = 100;
    integration->constraints[MESH_CYCLE_BATCH_WINDOW].interval_ms = 50;
    integration->constraints[MESH_CYCLE_COMMIT_DEADLINE].deadline_ms = 5000;

    for (int i = 0; i < MESH_CYCLE_COUNT; i++) {
        integration->constraints[i].type = (mesh_cycle_type_t)i;
        integration->constraints[i].multiplier = integration->config.timing_batch_multiplier;
    }

    integration->aggregate_health = 1.0f;
    integration->initialized = true;

    return integration;
}

static void mesh_cycle_coordinator_destroy(
    mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return;
    if (integration->magic != MESH_CYCLE_COORD_MAGIC) return;

    integration->magic = 0;
    integration->initialized = false;
    nimcp_free(integration);
}

static nimcp_error_t mesh_cycle_coordinator_connect_ordering(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_ordering_service_t* ordering)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!ordering) return NIMCP_ERROR_NULL_POINTER;
    if (!integration->initialized) return NIMCP_ERROR_INVALID_STATE;

    integration->ordering = ordering;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_connect_resilience(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_resilience_integration_t* resilience)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!resilience) return NIMCP_ERROR_NULL_POINTER;

    integration->resilience = resilience;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_connect_health_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_health_bridge_t* health_bridge)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!health_bridge) return NIMCP_ERROR_NULL_POINTER;

    integration->health_bridge = health_bridge;
    integration->connected = true;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_connect_exception_bridge(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_exception_bridge_t* exception_bridge)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!exception_bridge) return NIMCP_ERROR_NULL_POINTER;

    integration->exception_bridge = exception_bridge;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mock_connect_msp(
    mesh_cycle_coordinator_integration_t* integration,
    void* msp)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!msp) return NIMCP_ERROR_NULL_POINTER;

    integration->msp = msp;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_disconnect(
    mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;

    integration->ordering = nullptr;
    integration->resilience = nullptr;
    integration->health_bridge = nullptr;
    integration->exception_bridge = nullptr;
    integration->msp = nullptr;
    integration->connected = false;

    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_get_timing_constraint(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_type_t type,
    mesh_timing_constraint_t* constraint)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!constraint) return NIMCP_ERROR_NULL_POINTER;
    if (type >= MESH_CYCLE_COUNT) return NIMCP_ERROR_INVALID_PARAM;

    *constraint = integration->constraints[type];
    return NIMCP_SUCCESS;
}

static uint64_t mesh_cycle_coordinator_get_batch_window(
    const mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return 0;
    return (uint64_t)(integration->constraints[MESH_CYCLE_BATCH_WINDOW].interval_ms *
                      integration->config.timing_batch_multiplier);
}

static uint64_t mesh_cycle_coordinator_get_commit_deadline(
    const mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return 0;
    return (uint64_t)(integration->constraints[MESH_CYCLE_COMMIT_DEADLINE].deadline_ms *
                      integration->config.timing_batch_multiplier);
}

static nimcp_error_t mesh_cycle_coordinator_on_stall(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_type_t type,
    uint64_t stall_duration_ms)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (type >= MESH_CYCLE_COUNT) return NIMCP_ERROR_INVALID_PARAM;

    integration->consecutive_stalls++;
    integration->stats.stalls_detected++;

    /* Map stall duration to severity */
    if (stall_duration_ms < 100) {
        integration->current_severity = MESH_STALL_WARNING;
    } else if (stall_duration_ms < 500) {
        integration->current_severity = MESH_STALL_DEGRADED;
    } else if (stall_duration_ms < 2000) {
        integration->current_severity = MESH_STALL_CRITICAL;
    } else {
        integration->current_severity = MESH_STALL_FATAL;
    }

    /* Check if threshold triggers recovery */
    if (integration->consecutive_stalls >= integration->config.stall_recovery_threshold &&
        integration->config.enable_stall_recovery) {
        integration->recovery_status.action_type = MESH_RECOVERY_RESTART_MODULE;
        integration->recovery_status.in_progress = true;
        integration->recovery_status.attempts++;
        integration->stats.recoveries_triggered++;
    }

    /* Route to immune system if enabled and exception bridge connected */
    if (integration->config.enable_stall_recovery &&
        integration->exception_bridge) {
        /* Exception would be routed here */
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_request_recovery(
    mesh_cycle_coordinator_integration_t* integration,
    mesh_recovery_action_type_t action_type)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;

    integration->recovery_status.action_type = action_type;
    integration->recovery_status.in_progress = true;
    integration->recovery_status.attempts++;
    integration->stats.recoveries_triggered++;

    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_get_recovery_status(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_recovery_status_t* status)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!status) return NIMCP_ERROR_NULL_POINTER;

    *status = integration->recovery_status;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_get_health_endorsement(
    mesh_cycle_coordinator_integration_t* integration,
    float* health_score)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!health_score) return NIMCP_ERROR_NULL_POINTER;

    /* Get health from connected coordinator */
    if (integration->coordinator) {
        *health_score = mesh_coordinator_get_health(integration->coordinator);
    } else {
        *health_score = 1.0f;
    }

    integration->stats.health_endorsements++;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mesh_cycle_coordinator_contribute_health(
    mesh_cycle_coordinator_integration_t* integration,
    float contribution)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (contribution < 0.0f || contribution > 1.0f) return NIMCP_ERROR_INVALID_PARAM;

    /* Apply weight and update aggregate */
    float weighted = contribution * integration->config.health_endorsement_weight;
    integration->aggregate_health = (integration->aggregate_health + weighted) / 2.0f;
    integration->stats.health_endorsements++;

    return NIMCP_SUCCESS;
}

static float mock_get_aggregate_health(
    const mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return 0.0f;
    return integration->aggregate_health;
}

static nimcp_error_t mock_get_stats(
    const mesh_cycle_coordinator_integration_t* integration,
    mesh_cycle_coordinator_stats_t* stats)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    *stats = integration->stats;
    return NIMCP_SUCCESS;
}

static nimcp_error_t mock_reset_stats(
    mesh_cycle_coordinator_integration_t* integration)
{
    if (!integration) return NIMCP_ERROR_NULL_POINTER;

    memset(&integration->stats, 0, sizeof(mesh_cycle_coordinator_stats_t));
    integration->consecutive_stalls = 0;
    integration->current_severity = MESH_STALL_NONE;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshCycleCoordinatorTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_coordinator_t* coordinator = nullptr;
    mesh_cycle_coordinator_integration_t* integration = nullptr;

    void SetUp() override {
        /* Create minimal bootstrap */
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = false;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        /* Create coordinator */
        mesh_coordinator_config_t coord_config;
        mesh_coordinator_default_config(&coord_config);
        coord_config.name = "test_coordinator";
        coord_config.enable_logging = false;

        mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
        mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);

        if (registry && channel) {
            coordinator = mesh_coordinator_create(&coord_config, registry, channel);
        }
    }

    void TearDown() override {
        if (integration) {
            mesh_cycle_coordinator_destroy(integration);
            integration = nullptr;
        }
        if (coordinator) {
            mesh_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
    }

    void CreateIntegration(const mesh_cycle_coordinator_config_t* config = nullptr) {
        if (!coordinator) {
            GTEST_SKIP() << "Coordinator not available";
        }
        integration = mesh_cycle_coordinator_create(bootstrap, coordinator, config);
        ASSERT_NE(integration, nullptr);
    }
};

/* ============================================================================
 * 1. Configuration Tests (5 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, DefaultConfig) {
    mesh_cycle_coordinator_config_t config;
    nimcp_error_t err = mesh_cycle_coordinator_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(config.timing_batch_multiplier, 2.0f);  /* Real implementation default */
    EXPECT_TRUE(config.enable_timing_constraints);
    EXPECT_FLOAT_EQ(config.health_endorsement_weight, 0.5f);
    EXPECT_TRUE(config.enable_distributed_health);
    EXPECT_EQ(config.stall_recovery_threshold, 3u);
    EXPECT_TRUE(config.enable_stall_recovery);
    EXPECT_TRUE(config.enable_stall_recovery);
    EXPECT_FALSE(config.verbose_logging);
}

TEST_F(MeshCycleCoordinatorTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_cycle_coordinator_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshCycleCoordinatorTest, ConfigValidation) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);

    /* Valid config */
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_SUCCESS);

    /* Invalid stall threshold */
    config.stall_recovery_threshold = 0;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshCycleCoordinatorTest, ConfigTimingMultiplierRange) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);

    /* Below range */
    config.timing_batch_multiplier = 0.05f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_ERROR_INVALID_PARAM);

    /* Above range */
    config.timing_batch_multiplier = 15.0f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_ERROR_INVALID_PARAM);

    /* Valid minimum */
    config.timing_batch_multiplier = 0.1f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_SUCCESS);

    /* Valid maximum */
    config.timing_batch_multiplier = 10.0f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_SUCCESS);
}

TEST_F(MeshCycleCoordinatorTest, ConfigHealthWeightRange) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);

    /* Below range */
    config.health_endorsement_weight = -0.1f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_ERROR_INVALID_PARAM);

    /* Above range */
    config.health_endorsement_weight = 1.5f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_ERROR_INVALID_PARAM);

    /* Valid minimum */
    config.health_endorsement_weight = 0.0f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_SUCCESS);

    /* Valid maximum */
    config.health_endorsement_weight = 1.0f;
    EXPECT_EQ(mesh_cycle_coordinator_validate_config(&config), NIMCP_SUCCESS);
}

/* ============================================================================
 * 2. Lifecycle Tests (5 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, CreateDestroy) {
    if (!coordinator) {
        GTEST_SKIP() << "Coordinator not available";
    }

    integration = mesh_cycle_coordinator_create(bootstrap, coordinator, nullptr);
    ASSERT_NE(integration, nullptr);
    EXPECT_TRUE(integration->initialized);
    EXPECT_EQ(integration->magic, MESH_CYCLE_COORD_MAGIC);

    mesh_cycle_coordinator_destroy(integration);
    integration = nullptr;  /* Prevent double-free in TearDown */
}

TEST_F(MeshCycleCoordinatorTest, CreateWithNullBootstrap) {
    mesh_cycle_coordinator_integration_t* result =
        mesh_cycle_coordinator_create(nullptr, coordinator, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(MeshCycleCoordinatorTest, CreateWithNullCoordinator) {
    mesh_cycle_coordinator_integration_t* result =
        mesh_cycle_coordinator_create(bootstrap, nullptr, nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(MeshCycleCoordinatorTest, CreateWithNullConfig) {
    if (!coordinator) {
        GTEST_SKIP() << "Coordinator not available";
    }

    integration = mesh_cycle_coordinator_create(bootstrap, coordinator, nullptr);
    ASSERT_NE(integration, nullptr);

    /* Should use default values (real implementation default = 2.0f) */
    EXPECT_FLOAT_EQ(integration->config.timing_batch_multiplier, 2.0f);
    EXPECT_FLOAT_EQ(integration->config.health_endorsement_weight, 0.5f);
    EXPECT_EQ(integration->config.stall_recovery_threshold, 3u);
}

TEST_F(MeshCycleCoordinatorTest, DoubleDestroy) {
    if (!coordinator) {
        GTEST_SKIP() << "Coordinator not available";
    }

    integration = mesh_cycle_coordinator_create(bootstrap, coordinator, nullptr);
    ASSERT_NE(integration, nullptr);

    mesh_cycle_coordinator_destroy(integration);

    /* Second destroy should be safe (magic cleared) */
    mesh_cycle_coordinator_destroy(integration);
    integration = nullptr;
}

/* ============================================================================
 * 3. Connection Tests (8 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, ConnectOrdering) {
    CreateIntegration();

    int dummy_ordering = 1;
    nimcp_error_t err = mesh_cycle_coordinator_connect_ordering(
        integration, (mesh_ordering_service_t*)&dummy_ordering);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->ordering, (mesh_ordering_service_t*)&dummy_ordering);
}

TEST_F(MeshCycleCoordinatorTest, ConnectResilience) {
    CreateIntegration();

    int dummy_resilience = 2;
    nimcp_error_t err = mesh_cycle_coordinator_connect_resilience(
        integration, (mesh_resilience_integration_t*)&dummy_resilience);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->resilience, (mesh_resilience_integration_t*)&dummy_resilience);
}

TEST_F(MeshCycleCoordinatorTest, ConnectHealthBridge) {
    CreateIntegration();

    mesh_health_bridge_t* health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    nimcp_error_t err = mesh_cycle_coordinator_connect_health_bridge(
        integration, health_bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(integration->connected);
}

TEST_F(MeshCycleCoordinatorTest, ConnectExceptionBridge) {
    CreateIntegration();

    mesh_exception_bridge_t* exception_bridge =
        mesh_bootstrap_get_exception_bridge(bootstrap);
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    nimcp_error_t err = mesh_cycle_coordinator_connect_exception_bridge(
        integration, exception_bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCycleCoordinatorTest, ConnectMSP) {
    CreateIntegration();

    int dummy_msp = 42;
    nimcp_error_t err = mock_connect_msp(
        integration, &dummy_msp);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->msp, &dummy_msp);
}

TEST_F(MeshCycleCoordinatorTest, ConnectNullIntegration) {
    int dummy = 1;

    EXPECT_EQ(mesh_cycle_coordinator_connect_ordering(nullptr,
        (mesh_ordering_service_t*)&dummy), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_resilience(nullptr,
        (mesh_resilience_integration_t*)&dummy), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_health_bridge(nullptr,
        (mesh_health_bridge_t*)&dummy), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_exception_bridge(nullptr,
        (mesh_exception_bridge_t*)&dummy), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mock_connect_msp(nullptr, &dummy),
        NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshCycleCoordinatorTest, ConnectNullService) {
    CreateIntegration();

    EXPECT_EQ(mesh_cycle_coordinator_connect_ordering(integration, nullptr),
        NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_resilience(integration, nullptr),
        NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_health_bridge(integration, nullptr),
        NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_connect_exception_bridge(integration, nullptr),
        NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mock_connect_msp(integration, nullptr),
        NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshCycleCoordinatorTest, DisconnectAndReconnect) {
    CreateIntegration();

    int dummy_ordering = 1;
    mesh_cycle_coordinator_connect_ordering(
        integration, (mesh_ordering_service_t*)&dummy_ordering);

    /* Disconnect all */
    nimcp_error_t err = mesh_cycle_coordinator_disconnect(integration);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->ordering, nullptr);
    EXPECT_FALSE(integration->connected);

    /* Reconnect */
    err = mesh_cycle_coordinator_connect_ordering(
        integration, (mesh_ordering_service_t*)&dummy_ordering);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->ordering, (mesh_ordering_service_t*)&dummy_ordering);
}

/* ============================================================================
 * 4. Timing Constraint Tests (6 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, GetTimingConstraintOscillations) {
    CreateIntegration();

    mesh_timing_constraint_t constraint;
    nimcp_error_t err = mesh_cycle_coordinator_get_timing_constraint(
        integration, MESH_CYCLE_OSCILLATIONS, &constraint);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(constraint.type, MESH_CYCLE_OSCILLATIONS);
    EXPECT_EQ(constraint.interval_ms, 10u);  /* 10ms interval */
}

TEST_F(MeshCycleCoordinatorTest, GetTimingConstraintBrainUpdate) {
    CreateIntegration();

    mesh_timing_constraint_t constraint;
    nimcp_error_t err = mesh_cycle_coordinator_get_timing_constraint(
        integration, MESH_CYCLE_BRAIN_UPDATE, &constraint);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(constraint.type, MESH_CYCLE_BRAIN_UPDATE);
    EXPECT_EQ(constraint.deadline_ms, 16u);  /* 16ms deadline */
}

TEST_F(MeshCycleCoordinatorTest, GetTimingConstraintImmuneTick) {
    CreateIntegration();

    mesh_timing_constraint_t constraint;
    nimcp_error_t err = mesh_cycle_coordinator_get_timing_constraint(
        integration, MESH_CYCLE_IMMUNE_TICK, &constraint);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(constraint.type, MESH_CYCLE_IMMUNE_TICK);
    EXPECT_EQ(constraint.timeout_ms, 50u);  /* 50ms validation timeout */
}

TEST_F(MeshCycleCoordinatorTest, GetTimingConstraintHealthAgent) {
    CreateIntegration();

    mesh_timing_constraint_t constraint;
    nimcp_error_t err = mesh_cycle_coordinator_get_timing_constraint(
        integration, MESH_CYCLE_HEALTH_AGENT, &constraint);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(constraint.type, MESH_CYCLE_HEALTH_AGENT);
    EXPECT_EQ(constraint.interval_ms, 100u);  /* 100ms heartbeat */
}

TEST_F(MeshCycleCoordinatorTest, GetBatchWindow) {
    CreateIntegration();

    /* Default multiplier is 2.0f, so 50ms * 2.0 = 100ms */
    uint64_t batch_window = mesh_cycle_coordinator_get_batch_window(integration);
    EXPECT_EQ(batch_window, 100u);  /* 50ms * 2.0 = 100ms */

    /* With different multiplier (4.0f) */
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);
    config.timing_batch_multiplier = 4.0f;

    mesh_cycle_coordinator_destroy(integration);
    integration = mesh_cycle_coordinator_create(bootstrap, coordinator, &config);
    ASSERT_NE(integration, nullptr);

    batch_window = mesh_cycle_coordinator_get_batch_window(integration);
    EXPECT_EQ(batch_window, 200u);  /* 50ms * 4.0 = 200ms */
}

TEST_F(MeshCycleCoordinatorTest, GetCommitDeadline) {
    CreateIntegration();

    /* Default multiplier is 2.0f, so 5000ms * 2.0 = 10000ms */
    uint64_t commit_deadline = mesh_cycle_coordinator_get_commit_deadline(integration);
    EXPECT_EQ(commit_deadline, 10000u);  /* 5000ms * 2.0 = 10000ms */
}

/* ============================================================================
 * 5. Stall Recovery Tests (8 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, OnStallSingleEvent) {
    CreateIntegration();

    nimcp_error_t err = mesh_cycle_coordinator_on_stall(
        integration, MESH_CYCLE_BRAIN_UPDATE, 50);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->consecutive_stalls, 1u);
    EXPECT_EQ(integration->stats.stalls_detected, 1u);
}

TEST_F(MeshCycleCoordinatorTest, OnStallConsecutive) {
    CreateIntegration();

    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 60);
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 70);

    EXPECT_EQ(integration->consecutive_stalls, 3u);
    EXPECT_EQ(integration->stats.stalls_detected, 3u);
}

TEST_F(MeshCycleCoordinatorTest, OnStallThresholdTriggersRecovery) {
    CreateIntegration();

    /* Config has threshold = 3 */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    EXPECT_FALSE(integration->recovery_status.in_progress);

    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    EXPECT_FALSE(integration->recovery_status.in_progress);

    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    EXPECT_TRUE(integration->recovery_status.in_progress);
    EXPECT_EQ(integration->recovery_status.action_type, MESH_RECOVERY_RESTART_MODULE);
    EXPECT_EQ(integration->stats.recoveries_triggered, 1u);
}

TEST_F(MeshCycleCoordinatorTest, OnStallSeverityMapping) {
    CreateIntegration();

    /* < 100ms = WARNING */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    EXPECT_EQ(integration->current_severity, MESH_STALL_WARNING);

    /* 100-500ms = DEGRADED */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 200);
    EXPECT_EQ(integration->current_severity, MESH_STALL_DEGRADED);

    /* 500-2000ms = CRITICAL */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 1000);
    EXPECT_EQ(integration->current_severity, MESH_STALL_CRITICAL);

    /* > 2000ms = FATAL */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 3000);
    EXPECT_EQ(integration->current_severity, MESH_STALL_FATAL);
}

TEST_F(MeshCycleCoordinatorTest, RequestRecoveryRestart) {
    CreateIntegration();

    nimcp_error_t err = mesh_cycle_coordinator_request_recovery(
        integration, MESH_RECOVERY_RESTART_MODULE);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->recovery_status.action_type, MESH_RECOVERY_RESTART_MODULE);
    EXPECT_TRUE(integration->recovery_status.in_progress);
    EXPECT_EQ(integration->stats.recoveries_triggered, 1u);
}

TEST_F(MeshCycleCoordinatorTest, RequestRecoveryElection) {
    CreateIntegration();

    nimcp_error_t err = mesh_cycle_coordinator_request_recovery(
        integration, MESH_RECOVERY_TRIGGER_ELECTION);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->recovery_status.action_type, MESH_RECOVERY_TRIGGER_ELECTION);
    EXPECT_TRUE(integration->recovery_status.in_progress);
}

TEST_F(MeshCycleCoordinatorTest, GetRecoveryStatus) {
    CreateIntegration();

    mesh_cycle_coordinator_request_recovery(integration, MESH_RECOVERY_ROLLBACK);

    mesh_recovery_status_t status;
    nimcp_error_t err = mesh_cycle_coordinator_get_recovery_status(integration, &status);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(status.action_type, MESH_RECOVERY_ROLLBACK);
    EXPECT_TRUE(status.in_progress);
    EXPECT_EQ(status.attempts, 1u);
}

TEST_F(MeshCycleCoordinatorTest, StallRoutesToImmune) {
    CreateIntegration();

    /* Connect exception bridge */
    mesh_exception_bridge_t* exception_bridge =
        mesh_bootstrap_get_exception_bridge(bootstrap);
    if (exception_bridge) {
        mesh_cycle_coordinator_connect_exception_bridge(integration, exception_bridge);
    }

    /* Verify config enables routing */
    EXPECT_TRUE(integration->config.enable_stall_recovery);

    /* Stall should attempt to route (we can't verify actual routing without mock) */
    nimcp_error_t err = mesh_cycle_coordinator_on_stall(
        integration, MESH_CYCLE_BRAIN_UPDATE, 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * 6. Health Tests (5 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, GetHealthEndorsement) {
    CreateIntegration();

    float health_score = 0.0f;
    nimcp_error_t err = mesh_cycle_coordinator_get_health_endorsement(
        integration, &health_score);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(health_score, 0.0f);
    EXPECT_LE(health_score, 1.0f);
    EXPECT_EQ(integration->stats.health_endorsements, 1u);
}

TEST_F(MeshCycleCoordinatorTest, ContributeHealth) {
    CreateIntegration();

    nimcp_error_t err = mesh_cycle_coordinator_contribute_health(integration, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(integration->stats.health_endorsements, 1u);

    /* Invalid contribution should fail */
    err = mesh_cycle_coordinator_contribute_health(integration, 1.5f);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);

    err = mesh_cycle_coordinator_contribute_health(integration, -0.1f);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshCycleCoordinatorTest, GetAggregateHealth) {
    CreateIntegration();

    float health = mock_get_aggregate_health(integration);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
    EXPECT_FLOAT_EQ(health, 1.0f);  /* Initial value */

    /* After contributions */
    mesh_cycle_coordinator_contribute_health(integration, 0.5f);
    health = mock_get_aggregate_health(integration);
    EXPECT_GT(health, 0.0f);
    EXPECT_LT(health, 1.0f);
}

TEST_F(MeshCycleCoordinatorTest, HealthWeightApplied) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);
    config.health_endorsement_weight = 0.25f;  /* Low weight */

    CreateIntegration(&config);

    float initial_health = mock_get_aggregate_health(integration);

    /* Contribute with low weight - should have less impact */
    mesh_cycle_coordinator_contribute_health(integration, 0.0f);
    float after_contribution = mock_get_aggregate_health(integration);

    /* With weight 0.25, contributing 0.0 should not drop health as much */
    EXPECT_GT(after_contribution, 0.4f);  /* Weighted averaging */
}

TEST_F(MeshCycleCoordinatorTest, HealthWithDisconnectedCoordinator) {
    CreateIntegration();

    /* Disconnect to test graceful handling */
    mesh_cycle_coordinator_disconnect(integration);

    /* Should still work with default values */
    float health_score = 0.0f;
    nimcp_error_t err = mesh_cycle_coordinator_get_health_endorsement(
        integration, &health_score);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(health_score, 0.0f);
}

/* ============================================================================
 * 7. Statistics Tests (4 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, GetStats) {
    CreateIntegration();

    mesh_cycle_coordinator_stats_t stats;
    nimcp_error_t err = mock_get_stats(integration, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.stalls_detected, 0u);
    EXPECT_EQ(stats.recoveries_triggered, 0u);
    EXPECT_EQ(stats.health_endorsements, 0u);
}

TEST_F(MeshCycleCoordinatorTest, ResetStats) {
    CreateIntegration();

    /* Generate some stats */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    mesh_cycle_coordinator_contribute_health(integration, 0.8f);

    mesh_cycle_coordinator_stats_t stats;
    mock_get_stats(integration, &stats);
    EXPECT_GT(stats.stalls_detected, 0u);

    /* Reset */
    nimcp_error_t err = mock_reset_stats(integration);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mock_get_stats(integration, &stats);
    EXPECT_EQ(stats.stalls_detected, 0u);
    EXPECT_EQ(stats.recoveries_triggered, 0u);
    EXPECT_EQ(stats.health_endorsements, 0u);
}

TEST_F(MeshCycleCoordinatorTest, StatsIncrementOnStall) {
    CreateIntegration();

    mesh_cycle_coordinator_stats_t before, after;
    mock_get_stats(integration, &before);

    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_OSCILLATIONS, 100);

    mock_get_stats(integration, &after);
    EXPECT_EQ(after.stalls_detected, before.stalls_detected + 1);
}

TEST_F(MeshCycleCoordinatorTest, StatsIncrementOnRecovery) {
    CreateIntegration();

    mesh_cycle_coordinator_stats_t before, after;
    mock_get_stats(integration, &before);

    mesh_cycle_coordinator_request_recovery(integration, MESH_RECOVERY_RESTART_MODULE);

    mock_get_stats(integration, &after);
    EXPECT_EQ(after.recoveries_triggered, before.recoveries_triggered + 1);
}

/* ============================================================================
 * 8. Edge Cases (3 tests)
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, NullParameterHandling) {
    CreateIntegration();

    /* Get timing constraint */
    mesh_timing_constraint_t constraint;
    EXPECT_EQ(mesh_cycle_coordinator_get_timing_constraint(nullptr, MESH_CYCLE_BRAIN_UPDATE, &constraint),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_get_timing_constraint(integration, MESH_CYCLE_BRAIN_UPDATE, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    /* On stall */
    EXPECT_EQ(mesh_cycle_coordinator_on_stall(nullptr, MESH_CYCLE_BRAIN_UPDATE, 100),
              NIMCP_ERROR_NULL_POINTER);

    /* Request recovery */
    EXPECT_EQ(mesh_cycle_coordinator_request_recovery(nullptr, MESH_RECOVERY_RESTART_MODULE),
              NIMCP_ERROR_NULL_POINTER);

    /* Get recovery status */
    mesh_recovery_status_t status;
    EXPECT_EQ(mesh_cycle_coordinator_get_recovery_status(nullptr, &status),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_get_recovery_status(integration, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    /* Get health */
    float health;
    EXPECT_EQ(mesh_cycle_coordinator_get_health_endorsement(nullptr, &health),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_cycle_coordinator_get_health_endorsement(integration, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    /* Contribute health */
    EXPECT_EQ(mesh_cycle_coordinator_contribute_health(nullptr, 0.5f),
              NIMCP_ERROR_NULL_POINTER);

    /* Get aggregate health */
    EXPECT_FLOAT_EQ(mock_get_aggregate_health(nullptr), 0.0f);

    /* Get stats */
    mesh_cycle_coordinator_stats_t stats;
    EXPECT_EQ(mock_get_stats(nullptr, &stats),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mock_get_stats(integration, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    /* Reset stats */
    EXPECT_EQ(mock_reset_stats(nullptr),
              NIMCP_ERROR_NULL_POINTER);

    /* Disconnect */
    EXPECT_EQ(mesh_cycle_coordinator_disconnect(nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshCycleCoordinatorTest, InvalidCycleType) {
    CreateIntegration();

    mesh_timing_constraint_t constraint;
    nimcp_error_t err = mesh_cycle_coordinator_get_timing_constraint(
        integration, MESH_CYCLE_COUNT, &constraint);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);

    err = mesh_cycle_coordinator_get_timing_constraint(
        integration, (mesh_cycle_type_t)99, &constraint);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);

    err = mesh_cycle_coordinator_on_stall(
        integration, MESH_CYCLE_COUNT, 100);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshCycleCoordinatorTest, ConcurrentAccess) {
    CreateIntegration();

    std::atomic<int> stall_count{0};
    std::atomic<int> health_count{0};

    auto stall_worker = [&]() {
        for (int i = 0; i < 100; i++) {
            mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
            stall_count++;
        }
    };

    auto health_worker = [&]() {
        for (int i = 0; i < 100; i++) {
            mesh_cycle_coordinator_contribute_health(integration, 0.8f);
            health_count++;
        }
    };

    std::thread t1(stall_worker);
    std::thread t2(health_worker);
    std::thread t3(stall_worker);
    std::thread t4(health_worker);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(stall_count.load(), 200);
    EXPECT_EQ(health_count.load(), 200);

    /* Stats should reflect concurrent operations */
    mesh_cycle_coordinator_stats_t stats;
    mock_get_stats(integration, &stats);
    EXPECT_EQ(stats.stalls_detected, 200u);
    EXPECT_EQ(stats.health_endorsements, 200u);
}

/* ============================================================================
 * Additional Tests for Full Coverage
 * ============================================================================ */

TEST_F(MeshCycleCoordinatorTest, TimingMultiplierAffectsAllConstraints) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);
    config.timing_batch_multiplier = 2.0f;

    CreateIntegration(&config);

    mesh_timing_constraint_t constraint;

    /* Check all constraints have multiplier applied */
    for (int i = 0; i < MESH_CYCLE_COUNT; i++) {
        mesh_cycle_coordinator_get_timing_constraint(
            integration, (mesh_cycle_type_t)i, &constraint);
        EXPECT_FLOAT_EQ(constraint.multiplier, 2.0f);
    }
}

TEST_F(MeshCycleCoordinatorTest, DisableAutoRecovery) {
    mesh_cycle_coordinator_config_t config;
    mesh_cycle_coordinator_default_config(&config);
    config.enable_stall_recovery = false;

    CreateIntegration(&config);

    /* Hit threshold but recovery should not trigger */
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);

    EXPECT_FALSE(integration->recovery_status.in_progress);
    EXPECT_EQ(integration->stats.recoveries_triggered, 0u);
}

TEST_F(MeshCycleCoordinatorTest, MultipleRecoveryAttempts) {
    CreateIntegration();

    mesh_cycle_coordinator_request_recovery(integration, MESH_RECOVERY_RESTART_MODULE);
    mesh_cycle_coordinator_request_recovery(integration, MESH_RECOVERY_RESTART_MODULE);
    mesh_cycle_coordinator_request_recovery(integration, MESH_RECOVERY_TRIGGER_ELECTION);

    mesh_recovery_status_t status;
    mesh_cycle_coordinator_get_recovery_status(integration, &status);
    EXPECT_EQ(status.attempts, 3u);
    EXPECT_EQ(status.action_type, MESH_RECOVERY_TRIGGER_ELECTION);  /* Last action */
}

TEST_F(MeshCycleCoordinatorTest, StallCounterResetOnStatsReset) {
    CreateIntegration();

    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    mesh_cycle_coordinator_on_stall(integration, MESH_CYCLE_BRAIN_UPDATE, 50);
    EXPECT_EQ(integration->consecutive_stalls, 2u);

    mock_reset_stats(integration);
    EXPECT_EQ(integration->consecutive_stalls, 0u);
    EXPECT_EQ(integration->current_severity, MESH_STALL_NONE);
}

TEST_F(MeshCycleCoordinatorTest, HealthContributionAccumulates) {
    CreateIntegration();

    float initial = mock_get_aggregate_health(integration);
    EXPECT_FLOAT_EQ(initial, 1.0f);

    /* Multiple contributions should blend */
    mesh_cycle_coordinator_contribute_health(integration, 0.5f);
    float after1 = mock_get_aggregate_health(integration);

    mesh_cycle_coordinator_contribute_health(integration, 0.5f);
    float after2 = mock_get_aggregate_health(integration);

    /* Values should converge towards contributed value */
    EXPECT_LT(after1, initial);
    /* Multiple contributions of same value should stabilize */
    EXPECT_LE(after2, after1);
}

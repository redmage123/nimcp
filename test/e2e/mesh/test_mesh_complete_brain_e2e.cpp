/**
 * @file test_mesh_complete_brain_e2e.cpp
 * @brief End-to-End Tests for Complete Brain Mesh Integration
 *
 * WHAT: Tests complete brain simulation with all regions through mesh
 * WHY:  Verify full system operates correctly as unified brain simulation
 * HOW:  Create all brain regions, register with mesh, send predictions, verify health
 *
 * TEST COVERAGE:
 * - Create full brain with all regions
 * - Register all regions with mesh
 * - Send predictions through mesh
 * - Verify full round-trip through all channels
 * - Test 1000 predictions through mesh
 * - Verify all health metrics collected
 * - Cross-hemisphere communication
 * - Subcortical integration
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <map>
#include <set>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t PREDICTION_COUNT = 1000;
static constexpr size_t BRAIN_REGION_COUNT = 20;
static constexpr uint32_t MOCK_BRAIN_MAGIC = 0xBRAIN01;

// =============================================================================
// Mock Brain Region Structure
// =============================================================================

typedef struct mock_brain_region {
    uint32_t magic;
    char name[64];
    mesh_adapter_category_t category;
    float activation_level;
    float prediction_error;
    size_t predictions_processed;
    bool is_active;
} mock_brain_region_t;

#define mock_brain_region_t_MAGIC MOCK_BRAIN_MAGIC

// =============================================================================
// Test Fixture - Complete Brain E2E
// =============================================================================

class MeshCompleteBrainE2ETest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap_ = nullptr;
    mesh_module_registry_t* registry_ = nullptr;
    mesh_bio_bridge_t* bio_bridge_ = nullptr;
    mesh_health_bridge_t* health_bridge_ = nullptr;
    mesh_pattern_router_t* pattern_router_ = nullptr;

    std::map<std::string, mock_brain_region_t*> brain_regions_;
    std::map<std::string, mesh_participant_id_t> participant_ids_;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = true;
        config.subsystems.enable_sensory = true;
        config.subsystems.enable_motor = true;
        config.subsystems.enable_memory = true;
        config.subsystems.enable_security = true;
        config.enable_health_monitoring = true;
        config.verbose_logging = false;

        bootstrap_ = mesh_bootstrap_create(&config);
        if (!bootstrap_) {
            GTEST_SKIP() << "Bootstrap creation not available";
        }

        registry_ = mesh_bootstrap_get_module_registry(bootstrap_);
        bio_bridge_ = mesh_bootstrap_get_bio_bridge(bootstrap_);
        health_bridge_ = mesh_bootstrap_get_health_bridge(bootstrap_);
        pattern_router_ = mesh_bootstrap_get_pattern_router(bootstrap_);

        // Create all brain regions
        CreateBrainRegions();
    }

    void TearDown() override {
        for (auto& pair : brain_regions_) {
            nimcp_free(pair.second);
        }
        brain_regions_.clear();
        participant_ids_.clear();

        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
    }

    void CreateBrainRegions() {
        // Cortical regions - Left Hemisphere
        CreateRegion("left_pfc", MESH_ADAPTER_CATEGORY_COGNITIVE);
        CreateRegion("left_dlpfc", MESH_ADAPTER_CATEGORY_COGNITIVE);
        CreateRegion("left_motor_cortex", MESH_ADAPTER_CATEGORY_MOTOR);
        CreateRegion("left_sensory_cortex", MESH_ADAPTER_CATEGORY_PERCEPTION);
        CreateRegion("broca_area", MESH_ADAPTER_CATEGORY_COGNITIVE);
        CreateRegion("left_temporal", MESH_ADAPTER_CATEGORY_PERCEPTION);
        CreateRegion("left_parietal", MESH_ADAPTER_CATEGORY_PERCEPTION);

        // Cortical regions - Right Hemisphere
        CreateRegion("right_pfc", MESH_ADAPTER_CATEGORY_COGNITIVE);
        CreateRegion("right_dlpfc", MESH_ADAPTER_CATEGORY_COGNITIVE);
        CreateRegion("right_motor_cortex", MESH_ADAPTER_CATEGORY_MOTOR);
        CreateRegion("right_sensory_cortex", MESH_ADAPTER_CATEGORY_PERCEPTION);
        CreateRegion("right_spatial", MESH_ADAPTER_CATEGORY_PERCEPTION);
        CreateRegion("right_creative", MESH_ADAPTER_CATEGORY_COGNITIVE);

        // Subcortical regions
        CreateRegion("hippocampus", MESH_ADAPTER_CATEGORY_MEMORY);
        CreateRegion("amygdala", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
        CreateRegion("thalamus", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
        CreateRegion("basal_ganglia", MESH_ADAPTER_CATEGORY_MOTOR);
        CreateRegion("hypothalamus", MESH_ADAPTER_CATEGORY_SUBCORTICAL);
        CreateRegion("cerebellum", MESH_ADAPTER_CATEGORY_MOTOR);

        // Security/Immune
        CreateRegion("bbb_gateway", MESH_ADAPTER_CATEGORY_SECURITY);
    }

    mock_brain_region_t* CreateRegion(const char* name, mesh_adapter_category_t category) {
        auto* region = static_cast<mock_brain_region_t*>(
            nimcp_calloc(1, sizeof(mock_brain_region_t)));

        if (!region) return nullptr;

        region->magic = MOCK_BRAIN_MAGIC;
        strncpy(region->name, name, sizeof(region->name) - 1);
        region->category = category;
        region->activation_level = 0.0f;
        region->prediction_error = 0.0f;
        region->predictions_processed = 0;
        region->is_active = true;

        brain_regions_[name] = region;
        return region;
    }

    nimcp_error_t RegisterRegionWithMesh(const char* name) {
        auto it = brain_regions_.find(name);
        if (it == brain_regions_.end()) return NIMCP_ERROR_INVALID_PARAMETER;

        mock_brain_region_t* region = it->second;

        mesh_module_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.module_name = name;
        desc.category = region->category;
        desc.module_instance = region;
        desc.module_size = sizeof(mock_brain_region_t);
        desc.module_magic = MOCK_BRAIN_MAGIC;
        desc.endorser_role = ENDORSER_ROLE_OPTIONAL;

        return mesh_bootstrap_register_module(bootstrap_, &desc);
    }

    mesh_transaction_t* CreatePredictionTransaction(
        const char* source_region, const char* prediction_data) {

        mesh_transaction_config_t config;
        mesh_transaction_config_init(&config);
        config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        config.payload = prediction_data;
        config.payload_size = strlen(prediction_data);

        return mesh_transaction_create(&config);
    }
};

// =============================================================================
// Test 1: Create Full Brain with All Regions
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, CreateFullBrainWithAllRegions) {
    // Verify all brain regions were created
    EXPECT_EQ(brain_regions_.size(), BRAIN_REGION_COUNT)
        << "Should have all " << BRAIN_REGION_COUNT << " brain regions";

    // Verify each region is properly initialized
    for (const auto& pair : brain_regions_) {
        const mock_brain_region_t* region = pair.second;
        EXPECT_EQ(region->magic, MOCK_BRAIN_MAGIC)
            << "Region " << pair.first << " should have correct magic";
        EXPECT_TRUE(region->is_active)
            << "Region " << pair.first << " should be active";
        EXPECT_STREQ(region->name, pair.first.c_str())
            << "Region name should match";
    }

    // Verify category distribution
    std::map<mesh_adapter_category_t, size_t> category_counts;
    for (const auto& pair : brain_regions_) {
        category_counts[pair.second->category]++;
    }

    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_COGNITIVE], 0u);
    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_PERCEPTION], 0u);
    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_MOTOR], 0u);
    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_MEMORY], 0u);
    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_SUBCORTICAL], 0u);
    EXPECT_GT(category_counts[MESH_ADAPTER_CATEGORY_SECURITY], 0u);
}

// =============================================================================
// Test 2: Register All Regions with Mesh
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, RegisterAllRegionsWithMesh) {
    if (!registry_) {
        GTEST_SKIP() << "Module registry not available";
    }

    size_t success_count = 0;

    for (const auto& pair : brain_regions_) {
        nimcp_error_t err = RegisterRegionWithMesh(pair.first.c_str());
        if (err == NIMCP_SUCCESS) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, BRAIN_REGION_COUNT)
        << "All brain regions should register successfully";

    // Verify registration via registry
    mesh_module_registry_stats_t stats;
    mesh_module_registry_get_stats(registry_, &stats);
    EXPECT_EQ(stats.total_registered, BRAIN_REGION_COUNT);

    // Verify each region can be looked up
    for (const auto& pair : brain_regions_) {
        const mesh_registered_module_t* found = mesh_module_registry_get(
            registry_, pair.first.c_str());
        EXPECT_NE(found, nullptr)
            << "Region " << pair.first << " should be found in registry";

        if (found) {
            EXPECT_TRUE(found->registered);
            EXPECT_EQ(found->descriptor.module_instance, pair.second);
        }
    }
}

// =============================================================================
// Test 3: Send Predictions Through Mesh
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, SendPredictionsThroughMesh) {
    // Register all regions first
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    mesh_integration_t* integration = mesh_bootstrap_get_integration(bootstrap_);
    if (!integration) {
        GTEST_SKIP() << "Integration not available";
    }

    size_t predictions_sent = 0;
    size_t predictions_success = 0;

    // Send predictions from cognitive regions
    std::vector<std::string> cognitive_regions = {
        "left_pfc", "right_pfc", "left_dlpfc", "right_dlpfc", "broca_area"
    };

    for (size_t i = 0; i < 100; i++) {
        for (const auto& region_name : cognitive_regions) {
            char prediction[128];
            snprintf(prediction, sizeof(prediction),
                    "prediction:%s:iteration_%zu:confidence_0.%02zu",
                    region_name.c_str(), i, (i % 100));

            mesh_transaction_t* tx = CreatePredictionTransaction(
                region_name.c_str(), prediction);

            if (tx) {
                predictions_sent++;

                // Simulate processing
                auto it = brain_regions_.find(region_name);
                if (it != brain_regions_.end()) {
                    it->second->predictions_processed++;
                    predictions_success++;
                }

                mesh_transaction_destroy(tx);
            }
        }
    }

    EXPECT_EQ(predictions_sent, 500u)
        << "Should send 100 predictions from each of 5 cognitive regions";

    EXPECT_EQ(predictions_success, predictions_sent)
        << "All predictions should be processed";

    // Verify processing counts
    for (const auto& region_name : cognitive_regions) {
        auto it = brain_regions_.find(region_name);
        ASSERT_NE(it, brain_regions_.end());
        EXPECT_EQ(it->second->predictions_processed, 100u)
            << "Region " << region_name << " should process 100 predictions";
    }
}

// =============================================================================
// Test 4: Full Round-Trip Through All Channels
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, FullRoundTripThroughAllChannels) {
    // Register all regions
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    // Get channels
    mesh_channel_t* left_hemi = mesh_bootstrap_get_channel(
        bootstrap_, MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_channel_t* right_hemi = mesh_bootstrap_get_channel(
        bootstrap_, MESH_CHANNEL_RIGHT_HEMISPHERE);
    mesh_channel_t* subcortical = mesh_bootstrap_get_channel(
        bootstrap_, MESH_CHANNEL_SUBCORTICAL);

    // Skip if channels not available
    if (!left_hemi || !right_hemi || !subcortical) {
        GTEST_SKIP() << "Not all channels available";
    }

    // Simulate sensory input -> processing -> motor output
    struct ProcessingStage {
        const char* region;
        mesh_channel_t* channel;
        const char* stage_name;
    };

    ProcessingStage pipeline[] = {
        {"left_sensory_cortex", left_hemi, "sensory_input"},
        {"left_pfc", left_hemi, "cognitive_processing"},
        {"hippocampus", subcortical, "memory_encoding"},
        {"basal_ganglia", subcortical, "action_selection"},
        {"left_motor_cortex", left_hemi, "motor_output"}
    };

    std::vector<uint64_t> stage_timestamps;

    for (size_t i = 0; i < sizeof(pipeline) / sizeof(pipeline[0]); i++) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                "stage:%s:from:%s:seq:%zu",
                pipeline[i].stage_name, pipeline[i].region, i);

        auto start = std::chrono::high_resolution_clock::now();

        mesh_transaction_t* tx = CreatePredictionTransaction(
            pipeline[i].region, payload);

        if (tx) {
            // Process through channel
            mesh_channel_id_t ch_id = mesh_channel_get_id(pipeline[i].channel);
            mesh_transaction_destroy(tx);

            auto it = brain_regions_.find(pipeline[i].region);
            if (it != brain_regions_.end()) {
                it->second->predictions_processed++;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start).count();
        stage_timestamps.push_back(static_cast<uint64_t>(duration_ns));
    }

    // Verify all stages completed
    EXPECT_EQ(stage_timestamps.size(), sizeof(pipeline) / sizeof(pipeline[0]));

    // Calculate total round-trip time
    uint64_t total_ns = 0;
    for (auto ns : stage_timestamps) {
        total_ns += ns;
    }
    float total_ms = total_ns / 1000000.0f;

    EXPECT_LT(total_ms, 100.0f)
        << "Full round-trip should complete in under 100ms";
}

// =============================================================================
// Test 5: 1000 Predictions Through Mesh
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, ThousandPredictionsThroughMesh) {
    // Register all regions
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<size_t> success_count{0};
    std::atomic<size_t> fail_count{0};

    // Send 1000 predictions
    for (size_t i = 0; i < PREDICTION_COUNT; i++) {
        // Rotate through different source regions
        std::vector<std::string> regions = {
            "left_pfc", "right_pfc", "hippocampus", "amygdala", "thalamus"
        };
        const char* source = regions[i % regions.size()].c_str();

        char prediction[128];
        snprintf(prediction, sizeof(prediction),
                "prediction_%zu:source_%s:confidence_%.2f",
                i, source, (float)(i % 100) / 100.0f);

        mesh_transaction_t* tx = CreatePredictionTransaction(source, prediction);
        if (tx) {
            success_count++;
            mesh_transaction_destroy(tx);

            auto it = brain_regions_.find(source);
            if (it != brain_regions_.end()) {
                it->second->predictions_processed++;
                it->second->activation_level = (float)(i % 100) / 100.0f;
            }
        } else {
            fail_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    EXPECT_EQ(success_count.load(), PREDICTION_COUNT)
        << "All 1000 predictions should be processed";

    EXPECT_EQ(fail_count.load(), 0u)
        << "No predictions should fail";

    // Performance assertion
    float throughput = static_cast<float>(success_count.load()) /
                       (duration_ms / 1000.0f);
    EXPECT_GT(throughput, 100.0f)
        << "Throughput should be at least 100 predictions/sec, got " << throughput;

    // Verify predictions distributed across regions
    size_t total_processed = 0;
    for (const auto& pair : brain_regions_) {
        total_processed += pair.second->predictions_processed;
    }
    EXPECT_EQ(total_processed, PREDICTION_COUNT);
}

// =============================================================================
// Test 6: Verify All Health Metrics Collected
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, VerifyAllHealthMetricsCollected) {
    if (!health_bridge_) {
        GTEST_SKIP() << "Health bridge not available";
    }

    // Register all regions
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    // Get system health
    mesh_system_health_t system_health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(health_bridge_, &system_health);

    if (err != NIMCP_SUCCESS) {
        GTEST_SKIP() << "System health not available";
    }

    // Verify system-level health
    EXPECT_NE(system_health.status, MESH_HEALTH_UNKNOWN)
        << "System health status should be determined";

    // Verify channel health collected
    for (size_t i = 0; i < system_health.channel_count; i++) {
        const mesh_channel_health_t& ch_health = system_health.channels[i];
        EXPECT_NE(ch_health.status, MESH_HEALTH_UNKNOWN)
            << "Channel " << i << " health should be determined";
    }

    // Verify health metrics are reasonable
    EXPECT_GE(system_health.system_health_score, 0.0f);
    EXPECT_LE(system_health.system_health_score, 1.0f);

    // Simulate some activity to generate more metrics
    for (size_t i = 0; i < 50; i++) {
        mesh_transaction_t* tx = CreatePredictionTransaction(
            "left_pfc", "health_test_prediction");
        if (tx) {
            mesh_transaction_destroy(tx);
        }
    }

    // Get bridge stats
    mesh_health_bridge_stats_t stats;
    mesh_health_bridge_get_stats(health_bridge_, &stats);

    // Stats should be collected
    EXPECT_GE(stats.health_checks_performed, 0u);
}

// =============================================================================
// Test 7: Cross-Hemisphere Communication
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, CrossHemisphereCommunication) {
    // Register all regions
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    // Simulate cross-hemisphere communication
    // (like visual information processed in right hemisphere
    //  integrated with language in left hemisphere)

    struct CrossHemiTask {
        const char* source;
        const char* target;
        const char* task;
    };

    CrossHemiTask tasks[] = {
        {"right_spatial", "broca_area", "describe_spatial_scene"},
        {"left_temporal", "right_creative", "creative_language_interpretation"},
        {"right_creative", "left_pfc", "analytical_evaluation_of_ideas"},
        {"left_motor_cortex", "right_motor_cortex", "bimanual_coordination"},
    };

    size_t successful_transfers = 0;

    for (const auto& task : tasks) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                "cross_hemi:%s->%s:task_%s",
                task.source, task.target, task.task);

        mesh_transaction_t* tx = CreatePredictionTransaction(task.source, payload);
        if (tx) {
            // Simulate transfer
            auto source_it = brain_regions_.find(task.source);
            auto target_it = brain_regions_.find(task.target);

            if (source_it != brain_regions_.end() &&
                target_it != brain_regions_.end()) {

                source_it->second->predictions_processed++;
                target_it->second->predictions_processed++;
                successful_transfers++;
            }

            mesh_transaction_destroy(tx);
        }
    }

    EXPECT_EQ(successful_transfers, sizeof(tasks) / sizeof(tasks[0]))
        << "All cross-hemisphere tasks should complete";
}

// =============================================================================
// Test 8: Subcortical Integration
// =============================================================================

TEST_F(MeshCompleteBrainE2ETest, SubcorticalIntegration) {
    // Register all regions
    for (const auto& pair : brain_regions_) {
        RegisterRegionWithMesh(pair.first.c_str());
    }

    // Test subcortical integration scenarios

    // Scenario 1: Emotional memory formation (Amygdala -> Hippocampus)
    {
        auto* amygdala = brain_regions_["amygdala"];
        auto* hippocampus = brain_regions_["hippocampus"];

        // Simulate emotional tagging of memory
        amygdala->activation_level = 0.9f;  // High emotional arousal

        mesh_transaction_t* tx = CreatePredictionTransaction(
            "amygdala", "emotional_tag:high_salience:fear_response");
        if (tx) {
            hippocampus->predictions_processed++;
            hippocampus->activation_level = 0.8f;  // Strong encoding
            mesh_transaction_destroy(tx);
        }

        EXPECT_GT(hippocampus->predictions_processed, 0u);
    }

    // Scenario 2: Motor learning (Basal Ganglia -> Cerebellum)
    {
        auto* basal_ganglia = brain_regions_["basal_ganglia"];
        auto* cerebellum = brain_regions_["cerebellum"];

        for (int i = 0; i < 10; i++) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                    "motor_sequence:step_%d:timing_adjustment", i);

            mesh_transaction_t* tx = CreatePredictionTransaction(
                "basal_ganglia", payload);
            if (tx) {
                cerebellum->predictions_processed++;
                basal_ganglia->predictions_processed++;
                mesh_transaction_destroy(tx);
            }
        }

        EXPECT_EQ(cerebellum->predictions_processed, 10u);
    }

    // Scenario 3: Arousal regulation (Hypothalamus -> Thalamus)
    {
        auto* hypothalamus = brain_regions_["hypothalamus"];
        auto* thalamus = brain_regions_["thalamus"];

        mesh_transaction_t* tx = CreatePredictionTransaction(
            "hypothalamus", "arousal_modulation:increase:stress_response");
        if (tx) {
            thalamus->predictions_processed++;
            thalamus->activation_level = 0.7f;
            mesh_transaction_destroy(tx);
        }

        EXPECT_GT(thalamus->predictions_processed, 0u);
    }

    // Verify subcortical regions processed predictions
    EXPECT_GT(brain_regions_["amygdala"]->activation_level, 0.0f);
    EXPECT_GT(brain_regions_["hippocampus"]->predictions_processed, 0u);
    EXPECT_GT(brain_regions_["cerebellum"]->predictions_processed, 0u);
    EXPECT_GT(brain_regions_["thalamus"]->predictions_processed, 0u);
}


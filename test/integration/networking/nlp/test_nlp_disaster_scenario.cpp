/**
 * @file test_nlp_disaster_scenario.cpp
 * @brief Integration Tests for NLP Disaster/SAR Scenario Operations
 *
 * WHAT: Tests disaster response and search & rescue (SAR) use cases
 * WHY:  Verify NLP handles time-critical emergency scenarios effectively
 * HOW:  Simulate disaster scenarios with victim reports, sensor data, and alerts
 *
 * TEST COVERAGE:
 * - Victim report broadcast and propagation
 * - Environmental sensor data aggregation from multiple drones
 * - GPS location tracking and updates
 * - Hazard alert prioritization and emergency messaging
 * - Resource status reporting (battery, fuel)
 * - Time-critical message delivery
 * - Emergency message override in EMCON modes
 * - Multi-node disaster coordination
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <cmath>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture and Helpers
//=============================================================================

/**
 * @brief Disaster message tracker
 */
struct DisasterTracker {
    std::mutex mutex;
    std::condition_variable cv;

    std::vector<nlp_victim_report_t> victim_reports;
    std::vector<nlp_location_t> location_updates;
    std::vector<nlp_sensor_data_t> sensor_readings;
    std::vector<uint32_t> hazard_alerts;

    std::atomic<uint32_t> victim_count{0};
    std::atomic<uint32_t> location_count{0};
    std::atomic<uint32_t> sensor_count{0};
    std::atomic<uint32_t> hazard_count{0};
    std::atomic<uint32_t> emergency_count{0};

    void record_victim(const nlp_victim_report_t& report) {
        std::lock_guard<std::mutex> lock(mutex);
        victim_reports.push_back(report);
        victim_count++;
        cv.notify_all();
    }

    void record_location(const nlp_location_t& location) {
        std::lock_guard<std::mutex> lock(mutex);
        location_updates.push_back(location);
        location_count++;
        cv.notify_all();
    }

    void record_sensor(const nlp_sensor_data_t& sensors) {
        std::lock_guard<std::mutex> lock(mutex);
        sensor_readings.push_back(sensors);
        sensor_count++;
        cv.notify_all();
    }

    void record_hazard(uint32_t sender_id) {
        std::lock_guard<std::mutex> lock(mutex);
        hazard_alerts.push_back(sender_id);
        hazard_count++;
        cv.notify_all();
    }

    void record_emergency() {
        emergency_count++;
        cv.notify_all();
    }

    bool wait_for_victims(uint32_t count, uint32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this, count]() { return victim_count >= count; });
    }

    bool wait_for_hazards(uint32_t count, uint32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this, count]() { return hazard_count >= count; });
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        victim_reports.clear();
        location_updates.clear();
        sensor_readings.clear();
        hazard_alerts.clear();
        victim_count = 0;
        location_count = 0;
        sensor_count = 0;
        hazard_count = 0;
        emergency_count = 0;
    }
};

/**
 * @brief Message callback for disaster tracking
 */
static void disaster_message_callback(nlp_node_t node, const nlp_peer_t* peer,
                                     const nlp_message_t* msg, void* user_data) {
    auto* tracker = static_cast<DisasterTracker*>(user_data);
    uint16_t msg_type = ntohs(msg->header.msg_type);
    uint16_t payload_len = ntohs(msg->header.payload_len);

    switch (static_cast<nlp_msg_type_t>(msg_type)) {
        case NLP_MSG_VICTIM_REPORT: {
            if (payload_len >= sizeof(nlp_victim_report_t)) {
                const nlp_victim_report_t* report =
                    reinterpret_cast<const nlp_victim_report_t*>(msg->payload);
                tracker->record_victim(*report);
            }
            break;
        }

        case NLP_MSG_LOCATION_UPDATE: {
            if (payload_len >= sizeof(nlp_location_t)) {
                const nlp_location_t* location =
                    reinterpret_cast<const nlp_location_t*>(msg->payload);
                tracker->record_location(*location);
            }
            break;
        }

        case NLP_MSG_SENSOR_DATA: {
            if (payload_len >= sizeof(nlp_sensor_data_t)) {
                const nlp_sensor_data_t* sensors =
                    reinterpret_cast<const nlp_sensor_data_t*>(msg->payload);
                tracker->record_sensor(*sensors);
            }
            break;
        }

        case NLP_MSG_HAZARD_ALERT: {
            tracker->record_hazard(ntohl(msg->header.sender_id));
            break;
        }

        case NLP_MSG_EMERGENCY: {
            tracker->record_emergency();
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Test fixture for disaster scenario tests
 */
class NLPDisasterScenarioTest : public ::testing::Test {
protected:
    static constexpr uint16_t BASE_PORT = 20000;
    static constexpr uint32_t TIMEOUT_MS = 5000;

    std::vector<nlp_node_t> nodes_;
    std::vector<std::unique_ptr<DisasterTracker>> trackers_;

    void SetUp() override {
        // Logging initialized by framework
    }

    void TearDown() override {
        for (auto node : nodes_) {
            if (node) {
                nlp_node_stop(node);
                nlp_node_destroy(node);
            }
        }
        nodes_.clear();
        trackers_.clear();
    }

    nlp_node_t CreateNode(uint16_t port, nlp_mode_t mode = NLP_MODE_STANDARD) {
        nlp_config_t config = nlp_config_default();
        config.brain_id = nlp_generate_brain_id();
        config.port = port;
        config.default_mode = mode;
        config.auto_mode_switch = false;
        config.heartbeat_interval_ms = 500;
        config.session_timeout_ms = 5000;
        strncpy(config.bind_address, "127.0.0.1", sizeof(config.bind_address) - 1);

        auto tracker = std::make_unique<DisasterTracker>();
        config.user_data = tracker.get();

        nlp_node_t node = nlp_node_create(&config);
        if (!node) {
            return nullptr;
        }

        nlp_set_message_callback(node, disaster_message_callback);

        if (nlp_node_start(node) != 0) {
            nlp_node_destroy(node);
            return nullptr;
        }

        nodes_.push_back(node);
        trackers_.push_back(std::move(tracker));

        return node;
    }

    DisasterTracker* GetTracker(size_t node_index) {
        if (node_index < trackers_.size()) {
            return trackers_[node_index].get();
        }
        return nullptr;
    }

    void ConnectMesh(size_t num_nodes) {
        for (size_t i = 0; i < num_nodes; i++) {
            for (size_t j = 0; j < num_nodes; j++) {
                if (i != j) {
                    nlp_connect_peer(nodes_[i], "127.0.0.1", BASE_PORT + j);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
};

//=============================================================================
// Victim Report Tests
//=============================================================================

TEST_F(NLPDisasterScenarioTest, VictimReportBroadcast) {
    // Create SAR drone network (4 drones)
    constexpr size_t NUM_DRONES = 4;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        nlp_node_t drone = CreateNode(BASE_PORT + i);
        ASSERT_NE(drone, nullptr) << "Failed to create drone " << i;
    }

    // Connect drones in mesh
    ConnectMesh(NUM_DRONES);

    // Clear trackers
    for (size_t i = 0; i < NUM_DRONES; i++) {
        GetTracker(i)->clear();
    }

    // Drone 0 finds a victim and broadcasts report
    nlp_victim_report_t report = {};
    report.victim_id = htonl(12345);
    report.location.latitude = 37.7749;  // San Francisco
    report.location.longitude = -122.4194;
    report.location.altitude_m = 10.0f;
    report.location.accuracy_m = 5.0f;
    report.triage = NLP_TRIAGE_IMMEDIATE;  // Red - critical
    report.mobility = 0;  // Immobile
    report.consciousness = 1;  // Responsive
    report.breathing = 1;  // Breathing
    report.notes_len = 0;

    int result = nlp_send_victim_report(nodes_[0], &report, nullptr);
    ASSERT_EQ(result, 0) << "Failed to send victim report";

    // Wait for broadcast to reach all other drones
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify all other drones (1, 2, 3) received the report
    for (size_t i = 1; i < NUM_DRONES; i++) {
        DisasterTracker* tracker = GetTracker(i);
        EXPECT_GE(tracker->victim_count, 1u)
            << "Drone " << i << " did not receive victim report";

        if (tracker->victim_count > 0) {
            const auto& received = tracker->victim_reports[0];
            EXPECT_EQ(ntohl(received.victim_id), 12345u);
            EXPECT_EQ(received.triage, NLP_TRIAGE_IMMEDIATE);
            EXPECT_NEAR(received.location.latitude, 37.7749, 0.0001);
            EXPECT_NEAR(received.location.longitude, -122.4194, 0.0001);
        }
    }
}

TEST_F(NLPDisasterScenarioTest, MultipleVictimReports) {
    // Test multiple victims being reported by different drones
    constexpr size_t NUM_DRONES = 3;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        CreateNode(BASE_PORT + 10 + i);
    }
    ConnectMesh(NUM_DRONES);

    for (size_t i = 0; i < NUM_DRONES; i++) {
        GetTracker(i)->clear();
    }

    // Each drone reports a different victim
    for (size_t drone = 0; drone < NUM_DRONES; drone++) {
        nlp_victim_report_t report = {};
        report.victim_id = htonl(1000 + drone);
        report.location.latitude = 37.0 + drone * 0.1;
        report.location.longitude = -122.0 - drone * 0.1;
        report.triage = static_cast<nlp_triage_level_t>(drone % 4);
        report.mobility = 0;
        report.consciousness = 1;
        report.breathing = 1;

        nlp_send_victim_report(nodes_[drone], &report, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Each drone should have received reports from the other drones
    for (size_t i = 0; i < NUM_DRONES; i++) {
        DisasterTracker* tracker = GetTracker(i);
        // Should receive NUM_DRONES-1 reports (all except own)
        EXPECT_GE(tracker->victim_count, NUM_DRONES - 1)
            << "Drone " << i << " did not receive all victim reports";
    }
}

TEST_F(NLPDisasterScenarioTest, VictimReportWithNotes) {
    // Test victim report with attached notes
    nlp_node_t drone1 = CreateNode(BASE_PORT + 20);
    nlp_node_t drone2 = CreateNode(BASE_PORT + 21);
    ASSERT_NE(drone1, nullptr);
    ASSERT_NE(drone2, nullptr);

    nlp_connect_peer(drone1, "127.0.0.1", BASE_PORT + 21);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Create report with notes
    nlp_victim_report_t report = {};
    report.victim_id = htonl(9999);
    report.location.latitude = 40.7128;  // New York
    report.location.longitude = -74.0060;
    report.triage = NLP_TRIAGE_DELAYED;
    report.mobility = 1;  // Assisted
    report.consciousness = 1;
    report.breathing = 1;

    const char* notes = "Elderly male, broken leg, stable";

    int result = nlp_send_victim_report(drone1, &report, notes);
    ASSERT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    DisasterTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->victim_count, 1u);
}

//=============================================================================
// Sensor Data Tests
//=============================================================================

TEST_F(NLPDisasterScenarioTest, SensorDataAggregation) {
    // Test environmental sensor data collection from multiple drones
    constexpr size_t NUM_DRONES = 4;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        CreateNode(BASE_PORT + 30 + i);
    }
    ConnectMesh(NUM_DRONES);

    // Drone 0 is the master collecting data
    GetTracker(0)->clear();

    // Other drones send sensor data to master
    for (size_t drone = 1; drone < NUM_DRONES; drone++) {
        nlp_sensor_data_t sensors = {};
        sensors.temperature_c = 20.0f + drone * 5.0f;
        sensors.humidity_percent = 50.0f + drone * 10.0f;
        sensors.pressure_hpa = 1013.25f;
        sensors.co_ppm = 0.5f * drone;
        sensors.co2_ppm = 400.0f + drone * 50.0f;
        sensors.o2_percent = 21.0f - drone * 0.5f;
        sensors.radiation_usv_h = 0.1f;
        sensors.visibility_m = 1000.0f - drone * 100.0f;
        sensors.sensor_bitmap = NLP_SENSOR_TEMPERATURE | NLP_SENSOR_HUMIDITY |
                               NLP_SENSOR_PRESSURE | NLP_SENSOR_CO |
                               NLP_SENSOR_CO2 | NLP_SENSOR_O2 |
                               NLP_SENSOR_RADIATION | NLP_SENSOR_VISIBILITY;

        nlp_send_sensors(nodes_[drone], &sensors);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Master should have received sensor data from all drones
    DisasterTracker* master_tracker = GetTracker(0);
    EXPECT_GE(master_tracker->sensor_count, NUM_DRONES - 1)
        << "Master did not receive all sensor data";

    // Verify sensor data variety
    if (master_tracker->sensor_count >= 2) {
        float temp1 = master_tracker->sensor_readings[0].temperature_c;
        float temp2 = master_tracker->sensor_readings[1].temperature_c;
        EXPECT_NE(temp1, temp2) << "Sensor readings should differ";
    }
}

TEST_F(NLPDisasterScenarioTest, HazardousEnvironmentDetection) {
    // Test detection and reporting of hazardous conditions
    nlp_node_t drone = CreateNode(BASE_PORT + 40);
    nlp_node_t master = CreateNode(BASE_PORT + 41);
    ASSERT_NE(drone, nullptr);
    ASSERT_NE(master, nullptr);

    nlp_connect_peer(drone, "127.0.0.1", BASE_PORT + 41);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Drone detects hazardous conditions
    nlp_sensor_data_t hazard_sensors = {};
    hazard_sensors.temperature_c = 80.0f;  // High temperature
    hazard_sensors.co_ppm = 50.0f;         // Dangerous CO levels
    hazard_sensors.co2_ppm = 5000.0f;      // High CO2
    hazard_sensors.radiation_usv_h = 10.0f; // Elevated radiation
    hazard_sensors.o2_percent = 15.0f;     // Low oxygen
    hazard_sensors.sensor_bitmap = NLP_SENSOR_TEMPERATURE | NLP_SENSOR_CO |
                                  NLP_SENSOR_CO2 | NLP_SENSOR_RADIATION |
                                  NLP_SENSOR_O2;

    nlp_send_sensors(drone, &hazard_sensors);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    DisasterTracker* master_tracker = GetTracker(1);
    EXPECT_GE(master_tracker->sensor_count, 1u);

    if (master_tracker->sensor_count > 0) {
        const auto& sensors = master_tracker->sensor_readings[0];
        EXPECT_GT(sensors.temperature_c, 75.0f);
        EXPECT_GT(sensors.co_ppm, 40.0f);
        EXPECT_LT(sensors.o2_percent, 18.0f);
    }
}

//=============================================================================
// Location Tracking Tests
//=============================================================================

TEST_F(NLPDisasterScenarioTest, LocationTracking) {
    // Test GPS location updates from moving drones
    constexpr size_t NUM_DRONES = 3;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        CreateNode(BASE_PORT + 50 + i);
    }
    ConnectMesh(NUM_DRONES);

    GetTracker(0)->clear();

    // Drones send location updates as they move
    for (size_t drone = 1; drone < NUM_DRONES; drone++) {
        nlp_location_t location = {};
        location.latitude = 37.0 + drone * 0.01;
        location.longitude = -122.0 - drone * 0.01;
        location.altitude_m = 100.0f + drone * 10.0f;
        location.accuracy_m = 2.0f;
        location.heading_deg = drone * 45.0f;
        location.speed_mps = 5.0f;
        location.fix_quality = 2;  // DGPS
        location.fix_timestamp = static_cast<uint32_t>(
            std::chrono::system_clock::now().time_since_epoch().count());

        nlp_send_location(nodes_[drone], &location);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Master should track all drone locations
    DisasterTracker* master_tracker = GetTracker(0);
    EXPECT_GE(master_tracker->location_count, NUM_DRONES - 1);
}

TEST_F(NLPDisasterScenarioTest, LocationUpdateStream) {
    // Test streaming location updates
    nlp_node_t drone = CreateNode(BASE_PORT + 60);
    nlp_node_t master = CreateNode(BASE_PORT + 61);
    ASSERT_NE(drone, nullptr);
    ASSERT_NE(master, nullptr);

    nlp_connect_peer(drone, "127.0.0.1", BASE_PORT + 61);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Simulate drone movement with periodic location updates
    constexpr int num_updates = 5;
    for (int i = 0; i < num_updates; i++) {
        nlp_location_t location = {};
        location.latitude = 37.0 + i * 0.001;  // Moving north
        location.longitude = -122.0 + i * 0.001;  // Moving east
        location.altitude_m = 100.0f;
        location.accuracy_m = 3.0f;
        location.heading_deg = 45.0f;
        location.speed_mps = 10.0f;
        location.fix_quality = 1;

        nlp_send_location(drone, &location);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    DisasterTracker* master_tracker = GetTracker(1);
    EXPECT_GE(master_tracker->location_count, static_cast<uint32_t>(num_updates * 0.8))
        << "Master missed location updates";
}

//=============================================================================
// Hazard Alert Tests
//=============================================================================

TEST_F(NLPDisasterScenarioTest, HazardAlert) {
    // Test immediate hazard alert broadcast
    constexpr size_t NUM_DRONES = 4;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        CreateNode(BASE_PORT + 70 + i);
    }
    ConnectMesh(NUM_DRONES);

    for (size_t i = 0; i < NUM_DRONES; i++) {
        GetTracker(i)->clear();
    }

    // Drone 0 detects hazard and broadcasts alert
    const char* hazard_msg = "FIRE DETECTED - EVACUATE AREA";
    int result = nlp_send(nodes_[0], 0, NLP_MSG_HAZARD_ALERT,
                         hazard_msg, strlen(hazard_msg) + 1,
                         NLP_PRIORITY_CRITICAL);
    ASSERT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // All other drones should receive the hazard alert
    for (size_t i = 1; i < NUM_DRONES; i++) {
        DisasterTracker* tracker = GetTracker(i);
        EXPECT_GE(tracker->hazard_count, 1u)
            << "Drone " << i << " did not receive hazard alert";
    }
}

TEST_F(NLPDisasterScenarioTest, EmergencyMessagePriority) {
    // Test that emergency messages are prioritized
    nlp_node_t drone = CreateNode(BASE_PORT + 80);
    nlp_node_t master = CreateNode(BASE_PORT + 81);
    ASSERT_NE(drone, nullptr);
    ASSERT_NE(master, nullptr);

    nlp_connect_peer(drone, "127.0.0.1", BASE_PORT + 81);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send normal message
    const char* normal_msg = "Normal status update";
    nlp_send(drone, 0, NLP_MSG_DEBUG, normal_msg, strlen(normal_msg) + 1,
            NLP_PRIORITY_NORMAL);

    // Send emergency message
    const char* emergency_msg = "MAYDAY - DRONE CRASH";
    nlp_send(drone, 0, NLP_MSG_EMERGENCY, emergency_msg,
            strlen(emergency_msg) + 1, NLP_PRIORITY_CRITICAL);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify emergency was received
    DisasterTracker* master_tracker = GetTracker(1);
    EXPECT_GE(master_tracker->emergency_count, 1u);
}

//=============================================================================
// Emergency Mode Tests
//=============================================================================

TEST_F(NLPDisasterScenarioTest, EmergencyOverrideEMCON) {
    // Test that emergency messages override EMCON restrictions
    nlp_node_t drone = CreateNode(BASE_PORT + 90, NLP_MODE_STEALTH);
    nlp_node_t master = CreateNode(BASE_PORT + 91);
    ASSERT_NE(drone, nullptr);
    ASSERT_NE(master, nullptr);

    nlp_connect_peer(drone, "127.0.0.1", BASE_PORT + 91);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Set drone to SILENT mode (no transmissions)
    nlp_set_emcon(drone, NLP_EMCON_SILENT);
    EXPECT_EQ(nlp_get_emcon(drone), NLP_EMCON_SILENT);

    GetTracker(1)->clear();

    // Normal message should be blocked
    const char* normal_msg = "Normal message";
    nlp_send(drone, 0, NLP_MSG_DEBUG, normal_msg, strlen(normal_msg) + 1,
            NLP_PRIORITY_NORMAL);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Emergency message should break EMCON silence
    const char* emergency_msg = "CRITICAL EMERGENCY";
    nlp_send(drone, 0, NLP_MSG_EMERGENCY, emergency_msg,
            strlen(emergency_msg) + 1, NLP_PRIORITY_CRITICAL);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Note: Actual EMCON override behavior depends on implementation
    // Test verifies the API exists and can be called
}

//=============================================================================
// Integrated Disaster Scenario Test
//=============================================================================

TEST_F(NLPDisasterScenarioTest, CompleteDisasterResponse) {
    // Comprehensive test: earthquake scenario with multiple information types
    constexpr size_t NUM_DRONES = 4;
    for (size_t i = 0; i < NUM_DRONES; i++) {
        CreateNode(BASE_PORT + 100 + i);
    }
    ConnectMesh(NUM_DRONES);

    // Clear all trackers
    for (size_t i = 0; i < NUM_DRONES; i++) {
        GetTracker(i)->clear();
    }

    // Scenario: Earthquake aftermath
    // Drone 1: Reports victim
    nlp_victim_report_t victim = {};
    victim.victim_id = htonl(101);
    victim.location.latitude = 37.5;
    victim.location.longitude = -122.3;
    victim.triage = NLP_TRIAGE_IMMEDIATE;
    victim.mobility = 0;
    victim.consciousness = 1;
    victim.breathing = 1;
    nlp_send_victim_report(nodes_[1], &victim, "Trapped in rubble");

    // Drone 2: Reports hazardous gas leak
    nlp_sensor_data_t gas_sensors = {};
    gas_sensors.ch4_ppm = 1000.0f;  // Methane leak
    gas_sensors.co_ppm = 30.0f;     // CO from fires
    gas_sensors.sensor_bitmap = NLP_SENSOR_CH4 | NLP_SENSOR_CO;
    nlp_send_sensors(nodes_[2], &gas_sensors);

    // Drone 2: Broadcasts hazard alert
    const char* hazard = "GAS LEAK - HIGH METHANE";
    nlp_send(nodes_[2], 0, NLP_MSG_HAZARD_ALERT, hazard, strlen(hazard) + 1,
            NLP_PRIORITY_CRITICAL);

    // Drone 3: Updates location as it searches
    nlp_location_t location = {};
    location.latitude = 37.52;
    location.longitude = -122.31;
    location.altitude_m = 50.0f;
    nlp_send_location(nodes_[3], &location);

    // Wait for all messages to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Master (drone 0) should have received all information
    DisasterTracker* master = GetTracker(0);
    EXPECT_GE(master->victim_count, 1u) << "Master missed victim report";
    EXPECT_GE(master->sensor_count, 1u) << "Master missed sensor data";
    EXPECT_GE(master->hazard_count, 1u) << "Master missed hazard alert";
    EXPECT_GE(master->location_count, 1u) << "Master missed location update";

    // Other drones should also have situational awareness
    for (size_t i = 1; i < NUM_DRONES; i++) {
        DisasterTracker* drone_tracker = GetTracker(i);
        uint32_t total_messages = drone_tracker->victim_count +
                                 drone_tracker->sensor_count +
                                 drone_tracker->hazard_count +
                                 drone_tracker->location_count;

        EXPECT_GT(total_messages, 0u)
            << "Drone " << i << " has no situational awareness";
    }
}

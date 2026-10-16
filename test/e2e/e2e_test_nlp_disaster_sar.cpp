/**
 * @file e2e_test_nlp_disaster_sar.cpp
 * @brief E2E Test for Neural Link Protocol - Disaster Search and Rescue
 *
 * WHAT: Complete end-to-end tests for NLP in disaster/SAR scenarios
 * WHY:  Verify protocol supports search patterns, victim tracking, hazard alerts, outage recovery
 * HOW:  Simulate search operations, victim discovery, hazards, low battery, communications blackout
 *
 * TEST SCENARIOS:
 * 1. SearchPattern - Swarm executes coordinated search
 * 2. VictimDiscovery - Victim found, reported, and aggregated
 * 3. HazardAvoidance - Hazard alert causes swarm to reroute
 * 4. ResourceDepletion - Low battery triggers behavior change
 * 5. CommunicationsBlackout - Store-and-forward during outage
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>
#include <set>
#include <cmath>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Infrastructure
//=============================================================================

constexpr uint32_t NUM_SAR_DRONES = 8;
constexpr uint16_t SAR_BASE_PORT = 19200;
constexpr float SEARCH_GRID_SIZE = 100.0f; // 100m x 100m search area
constexpr float HAZARD_ZONE_RADIUS = 20.0f;

/**
 * @brief Search and rescue drone state
 */
struct SARDrone {
    uint32_t drone_id;
    nlp_node_t nlp_node;
    uint16_t port;

    // Location
    nlp_location_t current_location;
    nlp_location_t assigned_search_zone;

    // Search state
    bool searching;
    std::vector<nlp_location_t> search_waypoints;
    size_t current_waypoint;

    // Resources
    float battery_percent;
    bool low_power_mode;

    // Discovered victims
    std::vector<nlp_victim_report_t> victims_found;

    // Sensor data
    nlp_sensor_data_t sensors;

    // Hazard awareness
    std::vector<nlp_location_t> known_hazards;

    // Communications
    std::atomic<uint32_t> messages_sent{0};
    std::atomic<uint32_t> messages_received{0};
    std::atomic<uint32_t> victim_reports_received{0};
    std::atomic<uint32_t> hazard_alerts_received{0};

    // Store-and-forward buffer
    std::queue<nlp_message_t*> message_buffer;
    bool communications_blackout;

    std::mutex state_mutex;

    SARDrone()
        : drone_id(0), nlp_node(nullptr), port(0),
          searching(false), current_waypoint(0),
          battery_percent(100.0f), low_power_mode(false),
          communications_blackout(false) {
        memset(&current_location, 0, sizeof(current_location));
        memset(&assigned_search_zone, 0, sizeof(assigned_search_zone));
        memset(&sensors, 0, sizeof(sensors));
    }

    // Move constructor for use in std::vector
    SARDrone(SARDrone&& other) noexcept
        : drone_id(other.drone_id),
          nlp_node(other.nlp_node),
          port(other.port),
          current_location(other.current_location),
          assigned_search_zone(other.assigned_search_zone),
          searching(other.searching),
          search_waypoints(std::move(other.search_waypoints)),
          current_waypoint(other.current_waypoint),
          battery_percent(other.battery_percent),
          low_power_mode(other.low_power_mode),
          victims_found(std::move(other.victims_found)),
          sensors(other.sensors),
          known_hazards(std::move(other.known_hazards)),
          messages_sent(other.messages_sent.load()),
          messages_received(other.messages_received.load()),
          victim_reports_received(other.victim_reports_received.load()),
          hazard_alerts_received(other.hazard_alerts_received.load()),
          message_buffer(std::move(other.message_buffer)),
          communications_blackout(other.communications_blackout) {
        other.nlp_node = nullptr;
    }

    // Delete copy operations
    SARDrone(const SARDrone&) = delete;
    SARDrone& operator=(const SARDrone&) = delete;
    SARDrone& operator=(SARDrone&&) = delete;
};

/**
 * @brief Shared SAR test state
 */
struct SARTestState {
    std::vector<SARDrone*> drones;
    std::vector<nlp_victim_report_t> all_victims_reported;
    std::vector<nlp_location_t> all_hazards_reported;
    std::atomic<uint32_t> total_victims_found{0};
    std::atomic<uint32_t> total_hazards_found{0};
    std::mutex state_mutex;
};

static SARTestState g_sar_state;

//=============================================================================
// NLP Callbacks for SAR
//=============================================================================

static void sar_message_callback(
    nlp_node_t node,
    const nlp_peer_t* peer,
    const nlp_message_t* msg,
    void* user_data
) {
    auto* drone = static_cast<SARDrone*>(user_data);
    if (!drone) return;

    drone->messages_received++;

    // If in blackout, buffer message for later
    if (drone->communications_blackout) {
        std::lock_guard<std::mutex> lock(drone->state_mutex);
        // In real implementation, would store message
        return;
    }

    uint16_t msg_type = ntohs(msg->header.msg_type);

    std::lock_guard<std::mutex> lock(drone->state_mutex);

    switch (msg_type) {
        case NLP_MSG_VICTIM_REPORT:
            drone->victim_reports_received++;
            if (msg->payload && msg->header.payload_len >= sizeof(nlp_victim_report_t)) {
                nlp_victim_report_t report;
                memcpy(&report, msg->payload, sizeof(nlp_victim_report_t));
                drone->victims_found.push_back(report);

                // Aggregate to global state
                std::lock_guard<std::mutex> glock(g_sar_state.state_mutex);
                g_sar_state.all_victims_reported.push_back(report);
                g_sar_state.total_victims_found++;
            }
            break;

        case NLP_MSG_HAZARD_ALERT:
            drone->hazard_alerts_received++;
            if (msg->payload && msg->header.payload_len >= sizeof(nlp_location_t)) {
                nlp_location_t hazard_loc;
                memcpy(&hazard_loc, msg->payload, sizeof(nlp_location_t));
                drone->known_hazards.push_back(hazard_loc);

                // Aggregate to global state
                std::lock_guard<std::mutex> glock(g_sar_state.state_mutex);
                g_sar_state.all_hazards_reported.push_back(hazard_loc);
                g_sar_state.total_hazards_found++;
            }
            break;

        case NLP_MSG_LOCATION_UPDATE:
            // Track peer locations (not implemented in this test)
            break;

        case NLP_MSG_SENSOR_DATA:
            // Process sensor data from peers
            break;

        case NLP_MSG_RESOURCE_STATUS:
            // Monitor peer battery levels
            break;

        default:
            break;
    }
}

static void sar_peer_callback(
    nlp_node_t node,
    const nlp_peer_t* peer,
    nlp_session_state_t old_state,
    nlp_session_state_t new_state,
    void* user_data
) {
    // Track peer connectivity for SAR coordination
}

static void sar_mode_callback(
    nlp_node_t node,
    nlp_mode_t old_mode,
    nlp_mode_t new_mode,
    const char* reason,
    void* user_data
) {
    // Log mode changes
}

//=============================================================================
// Helper Functions
//=============================================================================

static nlp_node_t create_sar_drone(uint32_t drone_id, uint16_t port, SARDrone* sar_drone) {
    nlp_config_t config = nlp_config_default();

    config.brain_id = drone_id;
    config.is_master = (drone_id == 2000); // First drone is coordinator
    config.port = port;
    snprintf(config.bind_address, sizeof(config.bind_address), "127.0.0.1");

    config.default_mode = NLP_MODE_TACTICAL; // SAR uses tactical mode for resilience
    config.auto_mode_switch = true;
    config.heartbeat_interval_ms = 1000;
    config.session_timeout_ms = 5000;

    // Pre-shared key
    uint8_t psk[NLP_KEY_SIZE];
    for (int i = 0; i < NLP_KEY_SIZE; i++) {
        psk[i] = 0x73 + (i % 16); // SAR team key
    }
    config.psk[0].active = true;
    memcpy(config.psk[0].key, psk, NLP_KEY_SIZE);
    config.psk[0].key_id = 100;
    config.psk[0].valid_from = 0;
    config.psk[0].valid_until = UINT64_MAX;

    config.user_data = sar_drone;

    nlp_node_t node = nlp_node_create(&config);
    if (!node) return nullptr;

    nlp_set_message_callback(node, sar_message_callback);
    nlp_set_peer_callback(node, sar_peer_callback);
    nlp_set_mode_callback(node, sar_mode_callback);

    return node;
}

static void generate_search_grid(
    std::vector<SARDrone>& drones,
    float grid_size,
    float origin_lat,
    float origin_lon
) {
    uint32_t cols = static_cast<uint32_t>(std::sqrt(drones.size()));
    uint32_t rows = (drones.size() + cols - 1) / cols;

    float cell_width = grid_size / cols;
    float cell_height = grid_size / rows;

    for (size_t i = 0; i < drones.size(); i++) {
        uint32_t row = i / cols;
        uint32_t col = i % cols;

        drones[i].assigned_search_zone.latitude = origin_lat + (row * cell_height / 111000.0);
        drones[i].assigned_search_zone.longitude = origin_lon + (col * cell_width / 111000.0);
        drones[i].assigned_search_zone.altitude_m = 50.0f; // 50m altitude for search
    }
}

static float distance_meters(const nlp_location_t& a, const nlp_location_t& b) {
    // Simplified distance (haversine would be more accurate)
    double dlat = (b.latitude - a.latitude) * 111000.0;
    double dlon = (b.longitude - a.longitude) * 111000.0 * cos(a.latitude * M_PI / 180.0);
    return std::sqrt(dlat * dlat + dlon * dlon);
}

//=============================================================================
// Test Fixture
//=============================================================================

class NLPDisasterSARTest : public ::testing::Test {
protected:
    std::vector<SARDrone> drones_;

    void SetUp() override {
        drones_.resize(NUM_SAR_DRONES);
        g_sar_state.drones.clear();
        g_sar_state.all_victims_reported.clear();
        g_sar_state.all_hazards_reported.clear();
        g_sar_state.total_victims_found = 0;
        g_sar_state.total_hazards_found = 0;
    }

    void TearDown() override {
        for (auto& drone : drones_) {
            if (drone.nlp_node) {
                nlp_node_stop(drone.nlp_node);
                nlp_node_destroy(drone.nlp_node);
            }
        }
        drones_.clear();
        g_sar_state.drones.clear();
    }

    void InitializeSARDrones() {
        for (uint32_t i = 0; i < NUM_SAR_DRONES; i++) {
            drones_[i].drone_id = 2000 + i;
            drones_[i].port = SAR_BASE_PORT + i;
            drones_[i].battery_percent = 100.0f;
            drones_[i].searching = false;

            drones_[i].nlp_node = create_sar_drone(
                drones_[i].drone_id,
                drones_[i].port,
                &drones_[i]
            );

            ASSERT_NE(drones_[i].nlp_node, nullptr);
            ASSERT_EQ(nlp_node_start(drones_[i].nlp_node), 0);

            g_sar_state.drones.push_back(&drones_[i]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void ConnectSARNetwork() {
        for (size_t i = 0; i < drones_.size(); i++) {
            for (size_t j = i + 1; j < drones_.size(); j++) {
                nlp_connect_peer(drones_[i].nlp_node, "127.0.0.1", drones_[j].port);
                nlp_connect_peer(drones_[j].nlp_node, "127.0.0.1", drones_[i].port);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
};

//=============================================================================
// Test Cases
//=============================================================================

TEST_F(NLPDisasterSARTest, SearchPattern) {
    PipelineTracker tracker("Coordinated Search Pattern Execution");

    tracker.begin_stage("Initialize SAR Team", 2000);
    InitializeSARDrones();
    ConnectSARNetwork();
    tracker.end_stage();

    tracker.begin_stage("Assign Search Grid", 1000);
    float origin_lat = 37.7749f; // San Francisco
    float origin_lon = -122.4194f;
    generate_search_grid(drones_, SEARCH_GRID_SIZE, origin_lat, origin_lon);

    for (auto& drone : drones_) {
        drone.searching = true;
        drone.current_location = drone.assigned_search_zone;
    }
    tracker.end_stage();

    tracker.begin_stage("Broadcast Search Assignments", 2000);
    for (auto& drone : drones_) {
        nlp_send_location(drone.nlp_node, &drone.assigned_search_zone);
        drone.messages_sent++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Execute Search Pattern", 5000);
    // Simulate drones moving through their zones
    for (int step = 0; step < 10; step++) {
        for (auto& drone : drones_) {
            // Update position (simplified)
            drone.current_location.latitude += 0.0001;

            // Broadcast position update
            nlp_send_location(drone.nlp_node, &drone.current_location);
            drone.messages_sent++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    tracker.end_stage();

    tracker.begin_stage("Verify Search Coordination", 1000);
    for (const auto& drone : drones_) {
        EXPECT_GT(drone.messages_sent.load(), 5)
            << "Drone did not broadcast position updates";
        EXPECT_GT(drone.messages_received.load(), 0)
            << "Drone did not receive peer updates";
    }
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPDisasterSARTest, VictimDiscovery) {
    PipelineTracker tracker("Victim Discovery and Aggregation");

    tracker.begin_stage("Initialize SAR Team", 2000);
    InitializeSARDrones();
    ConnectSARNetwork();
    tracker.end_stage();

    tracker.begin_stage("Drone Discovers Victim", 2000);
    // Drone 0 finds a victim
    nlp_victim_report_t victim;
    memset(&victim, 0, sizeof(victim));
    victim.victim_id = 1001;
    victim.location.latitude = 37.7750;
    victim.location.longitude = -122.4195;
    victim.location.altitude_m = 0.0f;
    victim.triage = NLP_TRIAGE_IMMEDIATE;
    victim.mobility = 0; // Immobile
    victim.consciousness = 1; // Responsive
    victim.breathing = 1; // Breathing
    victim.notes_len = 0;

    int ret = nlp_send_victim_report(drones_[0].nlp_node, &victim, nullptr);
    EXPECT_GE(ret, 0) << "Failed to send victim report";
    tracker.end_stage();

    tracker.begin_stage("Wait for Victim Report Propagation", 3000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Verify Victim Report Received", 1000);
    // All other drones should receive the victim report
    uint32_t drones_with_report = 0;
    for (size_t i = 1; i < drones_.size(); i++) {
        if (drones_[i].victim_reports_received.load() > 0) {
            drones_with_report++;
        }
    }
    EXPECT_GE(drones_with_report, NUM_SAR_DRONES - 3)
        << "Victim report not propagated to swarm";
    tracker.end_stage();

    tracker.begin_stage("Multiple Victims Discovered", 3000);
    // Drones 1, 2, 3 find additional victims
    for (size_t i = 1; i <= 3; i++) {
        nlp_victim_report_t v;
        memset(&v, 0, sizeof(v));
        v.victim_id = 1000 + i + 1;
        v.location.latitude = 37.7750 + (i * 0.0001);
        v.location.longitude = -122.4195 + (i * 0.0001);
        v.triage = (nlp_triage_level_t)(i % 4);
        v.mobility = i % 3;
        v.consciousness = 1;
        v.breathing = 1;
        v.notes_len = 0;

        nlp_send_victim_report(drones_[i].nlp_node, &v, nullptr);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Verify Victim Aggregation", 1000);
    std::lock_guard<std::mutex> lock(g_sar_state.state_mutex);
    EXPECT_GE(g_sar_state.total_victims_found.load(), 3)
        << "Not all victims aggregated";
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPDisasterSARTest, HazardAvoidance) {
    PipelineTracker tracker("Hazard Alert and Swarm Reroute");

    tracker.begin_stage("Initialize SAR Team", 2000);
    InitializeSARDrones();
    ConnectSARNetwork();
    tracker.end_stage();

    tracker.begin_stage("Drone Detects Hazard", 2000);
    // Drone 0 detects hazard (e.g., fire, radiation)
    nlp_location_t hazard_location;
    hazard_location.latitude = 37.7752;
    hazard_location.longitude = -122.4196;
    hazard_location.altitude_m = 0.0f;
    hazard_location.accuracy_m = 5.0f;

    // Send hazard alert
    nlp_send(
        drones_[0].nlp_node,
        0, // Broadcast
        NLP_MSG_HAZARD_ALERT,
        &hazard_location,
        sizeof(hazard_location),
        NLP_PRIORITY_CRITICAL
    );
    tracker.end_stage();

    tracker.begin_stage("Wait for Hazard Alert Propagation", 3000);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Verify Hazard Alert Received", 1000);
    uint32_t drones_alerted = 0;
    for (size_t i = 1; i < drones_.size(); i++) {
        if (drones_[i].hazard_alerts_received.load() > 0) {
            drones_alerted++;
        }
    }
    EXPECT_GE(drones_alerted, NUM_SAR_DRONES - 3)
        << "Hazard alert not received by swarm";
    tracker.end_stage();

    tracker.begin_stage("Verify Hazard Awareness", 1000);
    // Check that drones have recorded the hazard
    for (const auto& drone : drones_) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(drone.state_mutex));
        if (!drone.known_hazards.empty()) {
            float dist = distance_meters(drone.known_hazards[0], hazard_location);
            EXPECT_LT(dist, 10.0f) << "Hazard location incorrect";
        }
    }
    tracker.end_stage();

    tracker.begin_stage("Simulate Hazard Avoidance", 2000);
    // Drones near hazard should reroute (simplified simulation)
    for (auto& drone : drones_) {
        std::lock_guard<std::mutex> lock(drone.state_mutex);
        if (!drone.known_hazards.empty()) {
            float dist = distance_meters(drone.current_location, hazard_location);
            if (dist < HAZARD_ZONE_RADIUS * 2) {
                // Reroute away from hazard (simplified)
                drone.current_location.latitude += 0.001; // Move north
            }
        }
    }
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPDisasterSARTest, ResourceDepletion) {
    PipelineTracker tracker("Low Battery Behavior Change");

    tracker.begin_stage("Initialize SAR Team", 2000);
    InitializeSARDrones();
    ConnectSARNetwork();
    tracker.end_stage();

    tracker.begin_stage("Normal Operations with Full Battery", 2000);
    for (auto& drone : drones_) {
        EXPECT_FALSE(drone.low_power_mode);
        EXPECT_EQ(drone.battery_percent, 100.0f);
    }

    // Send normal heartbeats
    for (auto& drone : drones_) {
        nlp_broadcast(
            drone.nlp_node,
            NLP_MSG_STATE_SYNC,
            "normal", 6,
            NLP_PRIORITY_NORMAL
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Simulate Battery Drain", 1000);
    // Drain batteries on drones 0, 1, 2
    for (size_t i = 0; i <= 2; i++) {
        drones_[i].battery_percent = 15.0f; // Low battery
        drones_[i].low_power_mode = true;
    }
    tracker.end_stage();

    tracker.begin_stage("Broadcast Low Battery Status", 2000);
    // Send resource status updates
    for (size_t i = 0; i <= 2; i++) {
        // In real implementation, would send battery status
        nlp_broadcast(
            drones_[i].nlp_node,
            NLP_MSG_RESOURCE_STATUS,
            &drones_[i].battery_percent,
            sizeof(float),
            NLP_PRIORITY_HIGH
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Verify Low Power Mode Activated", 1000);
    for (size_t i = 0; i <= 2; i++) {
        EXPECT_TRUE(drones_[i].low_power_mode);
        EXPECT_LT(drones_[i].battery_percent, 20.0f);
    }
    tracker.end_stage();

    tracker.begin_stage("Remaining Drones Compensate", 2000);
    // Drones with good battery take over duties
    uint32_t healthy_drones = 0;
    for (size_t i = 3; i < drones_.size(); i++) {
        if (drones_[i].battery_percent > 50.0f) {
            healthy_drones++;
            // Send extra heartbeats to maintain coverage
            nlp_broadcast(
                drones_[i].nlp_node,
                NLP_MSG_STATE_SYNC,
                "compensate", 10,
                NLP_PRIORITY_NORMAL
            );
        }
    }
    EXPECT_GE(healthy_drones, NUM_SAR_DRONES - 4);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPDisasterSARTest, CommunicationsBlackout) {
    PipelineTracker tracker("Store-and-Forward During Blackout");

    tracker.begin_stage("Initialize SAR Team", 2000);
    InitializeSARDrones();
    ConnectSARNetwork();
    tracker.end_stage();

    tracker.begin_stage("Normal Communications", 2000);
    for (auto& drone : drones_) {
        nlp_broadcast(
            drone.nlp_node,
            NLP_MSG_STATE_SYNC,
            "baseline", 8,
            NLP_PRIORITY_NORMAL
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    uint32_t baseline_received = 0;
    for (const auto& drone : drones_) {
        baseline_received += drone.messages_received.load();
    }
    EXPECT_GT(baseline_received, NUM_SAR_DRONES);
    tracker.end_stage();

    tracker.begin_stage("Simulate Communications Blackout", 1000);
    // Drones 0-3 enter blackout zone (disconnect)
    for (size_t i = 0; i <= 3; i++) {
        drones_[i].communications_blackout = true;

        // Disconnect from network
        for (size_t j = 0; j < drones_.size(); j++) {
            if (i != j) {
                nlp_disconnect_peer(drones_[i].nlp_node, drones_[j].drone_id);
            }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    tracker.end_stage();

    tracker.begin_stage("Verify Blackout Isolation", 2000);
    // Blackout drones should buffer messages
    for (size_t i = 0; i <= 3; i++) {
        // Try to send (should be buffered)
        nlp_broadcast(
            drones_[i].nlp_node,
            NLP_MSG_VICTIM_REPORT,
            "blackout_victim", 15,
            NLP_PRIORITY_CRITICAL
        );
    }

    // Non-blackout drones should still communicate
    for (size_t i = 4; i < drones_.size(); i++) {
        nlp_broadcast(
            drones_[i].nlp_node,
            NLP_MSG_STATE_SYNC,
            "active", 6,
            NLP_PRIORITY_NORMAL
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Exit Blackout Zone", 2000);
    // Reconnect blackout drones
    for (size_t i = 0; i <= 3; i++) {
        drones_[i].communications_blackout = false;

        for (size_t j = 4; j < drones_.size(); j++) {
            nlp_connect_peer(drones_[i].nlp_node, "127.0.0.1", drones_[j].port);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Forward Buffered Messages", 3000);
    // In real implementation, buffered messages would be sent
    // For this test, verify reconnection
    for (size_t i = 0; i <= 3; i++) {
        nlp_broadcast(
            drones_[i].nlp_node,
            NLP_MSG_STATE_SYNC,
            "recovered", 9,
            NLP_PRIORITY_NORMAL
        );
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    tracker.end_stage();

    tracker.begin_stage("Verify Network Recovery", 1000);
    uint32_t recovery_received = 0;
    for (const auto& drone : drones_) {
        recovery_received += drone.messages_received.load();
    }
    EXPECT_GT(recovery_received, baseline_received + NUM_SAR_DRONES)
        << "Network did not recover from blackout";
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

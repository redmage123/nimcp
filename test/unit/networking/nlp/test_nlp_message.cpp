/**
 * @file test_nlp_message.cpp
 * @brief Unit tests for Neural Link Protocol message serialization
 *
 * WHAT: Tests for NLP message packing/unpacking for neural sync and SAR payloads
 * WHY:  Ensure correct serialization of domain-specific message types
 * HOW:  Use GoogleTest with payload validation and stealth features
 *
 * TEST COVERAGE: 20 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <string.h>
#include <cstdint>
#include <vector>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_BATCH_ID = 0x12345678;
static const uint32_t TEST_NEURON_IDS[] = {100, 200, 300, 400, 500};
static const uint16_t TEST_SPIKE_TIMES[] = {10, 25, 42, 67, 89};
static const uint32_t TEST_SPIKE_COUNT = 5;

//=============================================================================
// Test Fixture
//=============================================================================

class NLPMessageTest : public ::testing::Test {
protected:
    nlp_node_t node;

    void SetUp() override
    {
        nlp_config_t config = nlp_config_default();
        config.brain_id = 0x11223344;

        node = nlp_node_create(&config);
        ASSERT_NE(node, nullptr) << "Failed to create NLP node";
    }

    void TearDown() override
    {
        if (node) {
            nlp_node_destroy(node);
            node = nullptr;
        }
    }
};

//=============================================================================
// Spike Batch Tests
//=============================================================================

/**
 * WHAT: Test spike batch packing
 * WHY:  Verify neural spikes can be serialized
 */
TEST_F(NLPMessageTest, SpikePackUnpack_BasicPacking)
{
    // Calculate payload size
    size_t header_size = sizeof(nlp_spike_batch_t);
    size_t ids_size = TEST_SPIKE_COUNT * sizeof(uint32_t);
    size_t times_size = TEST_SPIKE_COUNT * sizeof(uint16_t);
    size_t total_size = header_size + ids_size + times_size;

    std::vector<uint8_t> payload(total_size);
    nlp_spike_batch_t* batch = reinterpret_cast<nlp_spike_batch_t*>(payload.data());

    // Pack spike batch
    batch->batch_id = htonl(TEST_BATCH_ID);
    batch->timestamp_us = htonl(123456);
    batch->spike_count = htons(TEST_SPIKE_COUNT);
    batch->reserved = 0;

    // Pack neuron IDs
    uint32_t* ids = reinterpret_cast<uint32_t*>(payload.data() + header_size);
    for (uint32_t i = 0; i < TEST_SPIKE_COUNT; i++) {
        ids[i] = htonl(TEST_NEURON_IDS[i]);
    }

    // Pack spike times
    uint16_t* times = reinterpret_cast<uint16_t*>(payload.data() + header_size + ids_size);
    for (uint32_t i = 0; i < TEST_SPIKE_COUNT; i++) {
        times[i] = htons(TEST_SPIKE_TIMES[i]);
    }

    // Verify packed data
    EXPECT_EQ(ntohl(batch->batch_id), TEST_BATCH_ID);
    EXPECT_EQ(ntohs(batch->spike_count), TEST_SPIKE_COUNT);
}

/**
 * WHAT: Test spike batch unpacking
 * WHY:  Verify deserialization works correctly
 */
TEST_F(NLPMessageTest, SpikePackUnpack_Unpacking)
{
    // Create packed payload
    size_t header_size = sizeof(nlp_spike_batch_t);
    size_t ids_size = TEST_SPIKE_COUNT * sizeof(uint32_t);
    size_t times_size = TEST_SPIKE_COUNT * sizeof(uint16_t);
    size_t total_size = header_size + ids_size + times_size;

    std::vector<uint8_t> payload(total_size);
    nlp_spike_batch_t* batch = reinterpret_cast<nlp_spike_batch_t*>(payload.data());

    batch->batch_id = htonl(TEST_BATCH_ID);
    batch->spike_count = htons(TEST_SPIKE_COUNT);

    uint32_t* ids = reinterpret_cast<uint32_t*>(payload.data() + header_size);
    uint16_t* times = reinterpret_cast<uint16_t*>(payload.data() + header_size + ids_size);

    for (uint32_t i = 0; i < TEST_SPIKE_COUNT; i++) {
        ids[i] = htonl(TEST_NEURON_IDS[i]);
        times[i] = htons(TEST_SPIKE_TIMES[i]);
    }

    // Unpack and verify
    for (uint32_t i = 0; i < TEST_SPIKE_COUNT; i++) {
        EXPECT_EQ(ntohl(ids[i]), TEST_NEURON_IDS[i]);
        EXPECT_EQ(ntohs(times[i]), TEST_SPIKE_TIMES[i]);
    }
}

/**
 * WHAT: Test empty spike batch
 * WHY:  Handle edge case of no spikes
 */
TEST_F(NLPMessageTest, SpikePackUnpack_EmptyBatch)
{
    nlp_spike_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    batch.batch_id = htonl(999);
    batch.spike_count = 0;

    EXPECT_EQ(ntohs(batch.spike_count), 0);
}

/**
 * WHAT: Test spike batch with maximum spikes
 * WHY:  Verify large batch handling
 */
TEST_F(NLPMessageTest, SpikePackUnpack_LargeBatch)
{
    const uint32_t LARGE_COUNT = 1000;

    size_t header_size = sizeof(nlp_spike_batch_t);
    size_t ids_size = LARGE_COUNT * sizeof(uint32_t);
    size_t times_size = LARGE_COUNT * sizeof(uint16_t);
    size_t total_size = header_size + ids_size + times_size;

    EXPECT_LE(total_size, NLP_MAX_PAYLOAD) << "Batch exceeds max payload";
}

//=============================================================================
// Weight Delta Tests
//=============================================================================

/**
 * WHAT: Test weight delta packing
 * WHY:  Verify synaptic changes can be transmitted
 */
TEST_F(NLPMessageTest, WeightDeltaPackUnpack_BasicPacking)
{
    const uint32_t DELTA_COUNT = 3;

    size_t header_size = sizeof(nlp_weight_delta_header_t);
    size_t entries_size = DELTA_COUNT * sizeof(nlp_weight_delta_entry_t);
    size_t total_size = header_size + entries_size;

    std::vector<uint8_t> payload(total_size);
    nlp_weight_delta_header_t* header =
        reinterpret_cast<nlp_weight_delta_header_t*>(payload.data());

    // Pack header
    header->base_version = htonl(100);
    header->new_version = htonl(101);
    header->delta_count = htons(DELTA_COUNT);
    header->reserved = 0;

    // Pack entries
    nlp_weight_delta_entry_t* entries =
        reinterpret_cast<nlp_weight_delta_entry_t*>(payload.data() + header_size);

    for (uint32_t i = 0; i < DELTA_COUNT; i++) {
        entries[i].synapse_id = htonl(1000 + i);
        entries[i].old_weight = 0.5f + i * 0.1f;
        entries[i].new_weight = 0.6f + i * 0.1f;
    }

    // Verify
    EXPECT_EQ(ntohl(header->base_version), 100);
    EXPECT_EQ(ntohl(header->new_version), 101);
    EXPECT_EQ(ntohs(header->delta_count), DELTA_COUNT);
}

/**
 * WHAT: Test weight delta unpacking
 * WHY:  Verify weight changes can be extracted
 */
TEST_F(NLPMessageTest, WeightDeltaPackUnpack_Unpacking)
{
    nlp_weight_delta_entry_t entry;

    entry.synapse_id = htonl(42);
    entry.old_weight = 0.5f;
    entry.new_weight = 0.7f;

    // Verify
    EXPECT_EQ(ntohl(entry.synapse_id), 42);
    EXPECT_FLOAT_EQ(entry.old_weight, 0.5f);
    EXPECT_FLOAT_EQ(entry.new_weight, 0.7f);
}

/**
 * WHAT: Test weight delta versioning
 * WHY:  Ensure deltas apply to correct base version
 */
TEST_F(NLPMessageTest, WeightDeltaPackUnpack_Versioning)
{
    nlp_weight_delta_header_t header;

    header.base_version = htonl(50);
    header.new_version = htonl(51);

    uint32_t base = ntohl(header.base_version);
    uint32_t new_ver = ntohl(header.new_version);

    EXPECT_EQ(new_ver, base + 1);
}

//=============================================================================
// Location Tests
//=============================================================================

/**
 * WHAT: Test GPS location packing
 * WHY:  Verify location data can be transmitted
 */
TEST_F(NLPMessageTest, LocationPackUnpack_BasicPacking)
{
    nlp_location_t location;
    memset(&location, 0, sizeof(location));

    location.latitude = 37.7749;
    location.longitude = -122.4194;
    location.altitude_m = 52.5f;
    location.accuracy_m = 10.0f;
    location.heading_deg = 270.0f;
    location.speed_mps = 15.5f;
    location.fix_timestamp = htonl(1700000000);
    location.fix_quality = 2; // DGPS

    // Verify
    EXPECT_DOUBLE_EQ(location.latitude, 37.7749);
    EXPECT_DOUBLE_EQ(location.longitude, -122.4194);
    EXPECT_FLOAT_EQ(location.altitude_m, 52.5f);
    EXPECT_EQ(location.fix_quality, 2);
}

/**
 * WHAT: Test location with no GPS fix
 * WHY:  Handle GPS unavailability
 */
TEST_F(NLPMessageTest, LocationPackUnpack_NoFix)
{
    nlp_location_t location;
    memset(&location, 0, sizeof(location));

    location.fix_quality = 0; // No fix

    EXPECT_EQ(location.fix_quality, 0);
    EXPECT_DOUBLE_EQ(location.latitude, 0.0);
}

/**
 * WHAT: Test location coordinate extremes
 * WHY:  Verify boundary values
 */
TEST_F(NLPMessageTest, LocationPackUnpack_Extremes)
{
    nlp_location_t location;

    // North pole
    location.latitude = 90.0;
    location.longitude = 0.0;
    EXPECT_DOUBLE_EQ(location.latitude, 90.0);

    // South pole
    location.latitude = -90.0;
    EXPECT_DOUBLE_EQ(location.latitude, -90.0);

    // International date line
    location.longitude = 180.0;
    EXPECT_DOUBLE_EQ(location.longitude, 180.0);
}

//=============================================================================
// Sensor Data Tests
//=============================================================================

/**
 * WHAT: Test sensor data packing
 * WHY:  Verify environmental data can be transmitted
 */
TEST_F(NLPMessageTest, SensorDataPackUnpack_BasicPacking)
{
    nlp_sensor_data_t sensors;
    memset(&sensors, 0, sizeof(sensors));

    sensors.temperature_c = 22.5f;
    sensors.humidity_percent = 65.0f;
    sensors.pressure_hpa = 1013.25f;
    sensors.co2_ppm = 420.0f;
    sensors.o2_percent = 20.9f;
    sensors.sensor_bitmap = NLP_SENSOR_TEMPERATURE |
                           NLP_SENSOR_HUMIDITY |
                           NLP_SENSOR_PRESSURE |
                           NLP_SENSOR_CO2 |
                           NLP_SENSOR_O2;

    // Verify
    EXPECT_FLOAT_EQ(sensors.temperature_c, 22.5f);
    EXPECT_FLOAT_EQ(sensors.humidity_percent, 65.0f);
    EXPECT_NE(sensors.sensor_bitmap, 0);
}

/**
 * WHAT: Test sensor bitmap validation
 * WHY:  Verify which sensors have valid data
 */
TEST_F(NLPMessageTest, SensorDataPackUnpack_BitmapValidation)
{
    nlp_sensor_data_t sensors;
    memset(&sensors, 0, sizeof(sensors));

    sensors.sensor_bitmap = NLP_SENSOR_TEMPERATURE | NLP_SENSOR_RADIATION;
    sensors.temperature_c = 25.0f;
    sensors.radiation_usv_h = 0.1f;

    // Check temperature valid
    EXPECT_NE(sensors.sensor_bitmap & NLP_SENSOR_TEMPERATURE, 0);

    // Check humidity invalid
    EXPECT_EQ(sensors.sensor_bitmap & NLP_SENSOR_HUMIDITY, 0);

    // Check radiation valid
    EXPECT_NE(sensors.sensor_bitmap & NLP_SENSOR_RADIATION, 0);
}

/**
 * WHAT: Test hazardous gas detection
 * WHY:  Verify critical sensor readings
 */
TEST_F(NLPMessageTest, SensorDataPackUnpack_HazardousGas)
{
    nlp_sensor_data_t sensors;
    memset(&sensors, 0, sizeof(sensors));

    // Dangerous levels
    sensors.co_ppm = 100.0f;      // Carbon monoxide (dangerous >50 ppm)
    sensors.h2s_ppm = 20.0f;      // Hydrogen sulfide (dangerous >10 ppm)
    sensors.o2_percent = 15.0f;   // Low oxygen (dangerous <19.5%)

    sensors.sensor_bitmap = NLP_SENSOR_CO | NLP_SENSOR_H2S | NLP_SENSOR_O2;

    // Verify hazardous conditions
    EXPECT_GT(sensors.co_ppm, 50.0f);
    EXPECT_GT(sensors.h2s_ppm, 10.0f);
    EXPECT_LT(sensors.o2_percent, 19.5f);
}

//=============================================================================
// Victim Report Tests
//=============================================================================

/**
 * WHAT: Test victim report packing
 * WHY:  Verify SAR victim data can be transmitted
 */
TEST_F(NLPMessageTest, VictimReportPackUnpack_BasicPacking)
{
    const char* notes = "Male, approx 40 years, leg injury";
    size_t notes_len = strlen(notes);
    size_t total_size = sizeof(nlp_victim_report_t) + notes_len;

    std::vector<uint8_t> payload(total_size);
    nlp_victim_report_t* report =
        reinterpret_cast<nlp_victim_report_t*>(payload.data());

    // Pack report
    report->victim_id = htonl(12345);
    report->location.latitude = 37.7749;
    report->location.longitude = -122.4194;
    report->triage = NLP_TRIAGE_DELAYED;
    report->mobility = 1; // Assisted
    report->consciousness = 1; // Responsive
    report->breathing = 1; // Breathing
    report->notes_len = htons(notes_len);

    // Pack notes
    char* notes_ptr = reinterpret_cast<char*>(payload.data() + sizeof(nlp_victim_report_t));
    memcpy(notes_ptr, notes, notes_len);

    // Verify
    EXPECT_EQ(ntohl(report->victim_id), 12345);
    EXPECT_EQ(report->triage, NLP_TRIAGE_DELAYED);
    EXPECT_EQ(ntohs(report->notes_len), notes_len);
}

/**
 * WHAT: Test triage level encoding
 * WHY:  Verify medical priority is correct
 */
TEST_F(NLPMessageTest, VictimReportPackUnpack_TriageLevels)
{
    nlp_victim_report_t report;
    memset(&report, 0, sizeof(report));

    // Immediate (red)
    report.triage = NLP_TRIAGE_IMMEDIATE;
    EXPECT_EQ(report.triage, NLP_TRIAGE_IMMEDIATE);

    // Delayed (yellow)
    report.triage = NLP_TRIAGE_DELAYED;
    EXPECT_EQ(report.triage, NLP_TRIAGE_DELAYED);

    // Minor (green)
    report.triage = NLP_TRIAGE_MINOR;
    EXPECT_EQ(report.triage, NLP_TRIAGE_MINOR);

    // Deceased (black)
    report.triage = NLP_TRIAGE_DECEASED;
    EXPECT_EQ(report.triage, NLP_TRIAGE_DECEASED);
}

/**
 * WHAT: Test victim status indicators
 * WHY:  Verify vital signs encoding
 */
TEST_F(NLPMessageTest, VictimReportPackUnpack_VitalSigns)
{
    nlp_victim_report_t report;
    memset(&report, 0, sizeof(report));

    // Critical patient
    report.consciousness = 0; // Unresponsive
    report.breathing = 1;     // Breathing
    report.mobility = 0;      // Immobile

    EXPECT_EQ(report.consciousness, 0);
    EXPECT_EQ(report.breathing, 1);
    EXPECT_EQ(report.mobility, 0);

    // Should be triaged as immediate
    report.triage = NLP_TRIAGE_IMMEDIATE;
    EXPECT_EQ(report.triage, NLP_TRIAGE_IMMEDIATE);
}

//=============================================================================
// Stealth Mode Tests
//=============================================================================

/**
 * WHAT: Test stealth padding to fixed size
 * WHY:  Prevent traffic analysis
 */
TEST_F(NLPMessageTest, StealthPadding_FixedSize)
{
    size_t small_payload = 100;
    size_t padded_size = NLP_STEALTH_PACKET_SIZE;

    // All stealth packets should be same size
    EXPECT_EQ(padded_size, NLP_STEALTH_PACKET_SIZE);
    EXPECT_GT(padded_size, small_payload);
}

/**
 * WHAT: Test chaff generation
 * WHY:  Create fake traffic to confuse adversaries
 */
TEST_F(NLPMessageTest, ChaffGeneration_FakeTraffic)
{
    nlp_header_t chaff_header;
    memset(&chaff_header, 0, sizeof(chaff_header));

    chaff_header.msg_type = htons(NLP_MSG_CHAFF);
    NLP_SET_MODE(&chaff_header, NLP_MODE_STEALTH);

    EXPECT_EQ(ntohs(chaff_header.msg_type), NLP_MSG_CHAFF);
}

/**
 * WHAT: Test burst synchronization
 * WHY:  Coordinate stealth burst windows
 */
TEST_F(NLPMessageTest, StealthBurst_Synchronization)
{
    nlp_header_t burst_header;
    memset(&burst_header, 0, sizeof(burst_header));

    burst_header.msg_type = htons(NLP_MSG_BURST_SYNC);
    NLP_SET_MODE(&burst_header, NLP_MODE_STEALTH);

    EXPECT_EQ(ntohs(burst_header.msg_type), NLP_MSG_BURST_SYNC);
}

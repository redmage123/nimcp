//=============================================================================
// test_event_types.cpp - Comprehensive Event Types Tests
//=============================================================================
// WHAT: Complete test suite for event_types module
// WHY:  Ensure all event creation, copying, and utility functions work correctly
// HOW:  Test all event types, error paths, edge cases, and data integrity

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
// Headers have their own extern "C" guards
#include "middleware/events/nimcp_event_types.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class EventTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Prepare test data
        neuron_ids[0] = 10;
        neuron_ids[1] = 20;
        neuron_ids[2] = 30;

        memory_trace[0] = 0.5f;
        memory_trace[1] = 0.7f;
        memory_trace[2] = 0.9f;

        decision_vector[0] = 0.1f;
        decision_vector[1] = 0.2f;
        decision_vector[2] = 0.3f;
        decision_vector[3] = 0.4f;

        custom_data[0] = 100;
        custom_data[1] = 200;
        custom_data[2] = 300;
    }

    void TearDown() override {
        // Cleanup after tests
    }

    // Test data
    uint32_t neuron_ids[3];
    float memory_trace[3];
    float decision_vector[4];
    uint32_t custom_data[3];
};

//=============================================================================
// EVENT CREATION TESTS - Spike Burst
//=============================================================================

/**
 * WHAT: Test spike burst event creation with valid parameters
 * WHY:  Verify event is correctly initialized with all fields
 * HOW:  Create event and check all data fields match inputs
 */
TEST_F(EventTypesTest, CreateSpikeBurst_ValidParameters) {
    event_t event = event_create_spike_burst(
        neuron_ids, 3, 0.85f, 1000,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_BRAIN
    );

    EXPECT_EQ(event.type, EVENT_TYPE_SPIKE_BURST);
    EXPECT_EQ(event.priority, MW_EVENT_PRIORITY_HIGH);
    EXPECT_EQ(event.source, EVENT_SOURCE_BRAIN);
    EXPECT_EQ(event.data.spike_burst.neuron_ids, neuron_ids);
    EXPECT_EQ(event.data.spike_burst.num_neurons, 3);
    EXPECT_FLOAT_EQ(event.data.spike_burst.synchrony_score, 0.85f);
    EXPECT_EQ(event.data.spike_burst.burst_duration_us, 1000);
    EXPECT_GT(event.timestamp_us, 0);
}

/**
 * WHAT: Test spike burst with NULL neuron_ids
 * WHY:  Verify graceful handling of NULL pointer
 * HOW:  Pass NULL and check event still created
 */
TEST_F(EventTypesTest, CreateSpikeBurst_NullNeuronIds) {
    event_t event = event_create_spike_burst(
        nullptr, 0, 0.5f, 500,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR
    );

    EXPECT_EQ(event.type, EVENT_TYPE_SPIKE_BURST);
    EXPECT_EQ(event.data.spike_burst.neuron_ids, nullptr);
    EXPECT_EQ(event.data.spike_burst.num_neurons, 0);
}

/**
 * WHAT: Test spike burst with edge case values
 * WHY:  Verify boundary conditions are handled
 * HOW:  Test with 0 and 1.0 synchrony, max duration
 */
TEST_F(EventTypesTest, CreateSpikeBurst_EdgeCases) {
    // Zero synchrony
    event_t event1 = event_create_spike_burst(
        neuron_ids, 1, 0.0f, 1,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_BRAIN
    );
    EXPECT_FLOAT_EQ(event1.data.spike_burst.synchrony_score, 0.0f);

    // Max synchrony
    event_t event2 = event_create_spike_burst(
        neuron_ids, 100, 1.0f, UINT64_MAX,
        MW_EVENT_PRIORITY_BACKGROUND, EVENT_SOURCE_BRAIN
    );
    EXPECT_FLOAT_EQ(event2.data.spike_burst.synchrony_score, 1.0f);
    EXPECT_EQ(event2.data.spike_burst.burst_duration_us, UINT64_MAX);
}

//=============================================================================
// EVENT CREATION TESTS - Pattern Detected
//=============================================================================

/**
 * WHAT: Test pattern detected event creation
 * WHY:  Verify all pattern fields are correctly set
 * HOW:  Create event and validate all data members
 */
TEST_F(EventTypesTest, CreatePatternDetected_ValidParameters) {
    const char* pattern_name = "TestPattern";
    event_t event = event_create_pattern_detected(
        42, 0.95f, 5, pattern_name,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_PATTERN_DETECTOR
    );

    EXPECT_EQ(event.type, EVENT_TYPE_PATTERN_DETECTED);
    EXPECT_EQ(event.data.pattern_detected.pattern_id, 42);
    EXPECT_FLOAT_EQ(event.data.pattern_detected.match_confidence, 0.95f);
    EXPECT_EQ(event.data.pattern_detected.pattern_length, 5);
    EXPECT_STREQ(event.data.pattern_detected.pattern_name, pattern_name);
}

/**
 * WHAT: Test pattern detected with NULL name
 * WHY:  Verify NULL string pointers are handled safely
 * HOW:  Create with NULL name and check pointer
 */
TEST_F(EventTypesTest, CreatePatternDetected_NullName) {
    event_t event = event_create_pattern_detected(
        1, 0.5f, 1, nullptr,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_PATTERN_DETECTOR
    );

    EXPECT_EQ(event.data.pattern_detected.pattern_name, nullptr);
}

//=============================================================================
// EVENT CREATION TESTS - Attention Shift
//=============================================================================

/**
 * WHAT: Test attention shift event creation
 * WHY:  Verify attention tracking metadata is correct
 * HOW:  Create event and check prev/current items and strength
 */
TEST_F(EventTypesTest, CreateAttentionShift_ValidParameters) {
    const char* reason = "Novel stimulus detected";
    event_t event = event_create_attention_shift(
        10, 25, 0.8f, reason,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE
    );

    EXPECT_EQ(event.type, EVENT_TYPE_ATTENTION_SHIFT);
    EXPECT_EQ(event.data.attention_shift.previous_item, 10);
    EXPECT_EQ(event.data.attention_shift.current_item, 25);
    EXPECT_FLOAT_EQ(event.data.attention_shift.attention_strength, 0.8f);
    EXPECT_STREQ(event.data.attention_shift.shift_reason, reason);
}

/**
 * WHAT: Test attention shift with same item (no real shift)
 * WHY:  Verify edge case of refocusing on same item
 * HOW:  Create with prev == current
 */
TEST_F(EventTypesTest, CreateAttentionShift_SameItem) {
    event_t event = event_create_attention_shift(
        5, 5, 0.9f, "Refocus",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE
    );

    EXPECT_EQ(event.data.attention_shift.previous_item, 5);
    EXPECT_EQ(event.data.attention_shift.current_item, 5);
}

//=============================================================================
// EVENT CREATION TESTS - Memory Formed
//=============================================================================

/**
 * WHAT: Test memory formed event creation
 * WHY:  Verify memory consolidation data is stored correctly
 * HOW:  Create event with trace and check all fields
 */
TEST_F(EventTypesTest, CreateMemoryFormed_ValidParameters) {
    event_t event = event_create_memory_formed(
        123, memory_trace, 3, 0.75f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_WORKING_MEMORY
    );

    EXPECT_EQ(event.type, EVENT_TYPE_MEMORY_FORMED);
    EXPECT_EQ(event.data.memory_formed.memory_id, 123);
    EXPECT_EQ(event.data.memory_formed.memory_trace, memory_trace);
    EXPECT_EQ(event.data.memory_formed.trace_size, 3);
    EXPECT_FLOAT_EQ(event.data.memory_formed.consolidation_strength, 0.75f);
}

/**
 * WHAT: Test memory formed with NULL trace
 * WHY:  Verify NULL memory trace is handled
 * HOW:  Pass NULL trace pointer
 */
TEST_F(EventTypesTest, CreateMemoryFormed_NullTrace) {
    event_t event = event_create_memory_formed(
        1, nullptr, 0, 0.0f,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_WORKING_MEMORY
    );

    EXPECT_EQ(event.data.memory_formed.memory_trace, nullptr);
    EXPECT_EQ(event.data.memory_formed.trace_size, 0);
}

//=============================================================================
// EVENT CREATION TESTS - Salience Peak
//=============================================================================

/**
 * WHAT: Test salience peak event creation
 * WHY:  Verify all salience components are stored
 * HOW:  Create event and check salience/novelty/surprise/urgency
 */
TEST_F(EventTypesTest, CreateSaliencePeak_ValidParameters) {
    event_t event = event_create_salience_peak(
        0.9f, 0.7f, 0.8f, 0.95f,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE
    );

    EXPECT_EQ(event.type, EVENT_TYPE_SALIENCE_PEAK);
    EXPECT_FLOAT_EQ(event.data.salience_peak.salience_score, 0.9f);
    EXPECT_FLOAT_EQ(event.data.salience_peak.novelty_score, 0.7f);
    EXPECT_FLOAT_EQ(event.data.salience_peak.surprise_score, 0.8f);
    EXPECT_FLOAT_EQ(event.data.salience_peak.urgency_score, 0.95f);
}

/**
 * WHAT: Test salience peak with all zeros
 * WHY:  Verify low salience events can be created
 * HOW:  Set all scores to 0.0
 */
TEST_F(EventTypesTest, CreateSaliencePeak_ZeroScores) {
    event_t event = event_create_salience_peak(
        0.0f, 0.0f, 0.0f, 0.0f,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_SALIENCE
    );

    EXPECT_FLOAT_EQ(event.data.salience_peak.salience_score, 0.0f);
    EXPECT_FLOAT_EQ(event.data.salience_peak.novelty_score, 0.0f);
}

//=============================================================================
// EVENT CREATION TESTS - Oscillation Change
//=============================================================================

/**
 * WHAT: Test oscillation change event creation
 * WHY:  Verify frequency and power changes are recorded
 * HOW:  Create event and check prev/current freq and power
 */
TEST_F(EventTypesTest, CreateOscillationChange_ValidParameters) {
    const char* band = "alpha";
    event_t event = event_create_oscillation_change(
        8.0f, 12.0f, 1.5f, band,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    EXPECT_EQ(event.type, EVENT_TYPE_OSCILLATION_CHANGE);
    EXPECT_FLOAT_EQ(event.data.oscillation_change.previous_freq_hz, 8.0f);
    EXPECT_FLOAT_EQ(event.data.oscillation_change.current_freq_hz, 12.0f);
    EXPECT_FLOAT_EQ(event.data.oscillation_change.power_change, 1.5f);
    EXPECT_STREQ(event.data.oscillation_change.band_name, band);
}

/**
 * WHAT: Test oscillation with frequency decrease
 * WHY:  Verify negative frequency changes work
 * HOW:  Set current < previous frequency
 */
TEST_F(EventTypesTest, CreateOscillationChange_FrequencyDecrease) {
    event_t event = event_create_oscillation_change(
        30.0f, 10.0f, 0.5f, "beta",
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_BRAIN
    );

    EXPECT_GT(event.data.oscillation_change.previous_freq_hz,
              event.data.oscillation_change.current_freq_hz);
}

//=============================================================================
// EVENT CREATION TESTS - Error Detected
//=============================================================================

/**
 * WHAT: Test error detected event creation
 * WHY:  Verify prediction errors are captured correctly
 * HOW:  Create event and check expected/actual/magnitude
 */
TEST_F(EventTypesTest, CreateErrorDetected_ValidParameters) {
    event_t event = event_create_error_detected(
        5.0f, 3.0f, 2.0f, 42,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_PREDICTIVE
    );

    EXPECT_EQ(event.type, EVENT_TYPE_ERROR_DETECTED);
    EXPECT_FLOAT_EQ(event.data.error_detected.expected_value, 5.0f);
    EXPECT_FLOAT_EQ(event.data.error_detected.actual_value, 3.0f);
    EXPECT_FLOAT_EQ(event.data.error_detected.error_magnitude, 2.0f);
    EXPECT_EQ(event.data.error_detected.error_location, 42);
}

/**
 * WHAT: Test error with zero magnitude
 * WHY:  Verify perfect predictions can be recorded
 * HOW:  Set expected == actual, magnitude = 0
 */
TEST_F(EventTypesTest, CreateErrorDetected_ZeroError) {
    event_t event = event_create_error_detected(
        1.0f, 1.0f, 0.0f, 0,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_PREDICTIVE
    );

    EXPECT_FLOAT_EQ(event.data.error_detected.error_magnitude, 0.0f);
}

//=============================================================================
// EVENT CREATION TESTS - Decision Made
//=============================================================================

/**
 * WHAT: Test decision made event creation
 * WHY:  Verify decision data and confidence are stored
 * HOW:  Create event with decision vector and check fields
 */
TEST_F(EventTypesTest, CreateDecisionMade_ValidParameters) {
    event_t event = event_create_decision_made(
        999, 0.88f, decision_vector, 4,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_BRAIN
    );

    EXPECT_EQ(event.type, EVENT_TYPE_DECISION_MADE);
    EXPECT_EQ(event.data.decision_made.decision_id, 999);
    EXPECT_FLOAT_EQ(event.data.decision_made.confidence, 0.88f);
    EXPECT_EQ(event.data.decision_made.decision_vector, decision_vector);
    EXPECT_EQ(event.data.decision_made.vector_size, 4);
}

/**
 * WHAT: Test decision with NULL vector
 * WHY:  Verify decisions without vector data work
 * HOW:  Pass NULL decision_vector
 */
TEST_F(EventTypesTest, CreateDecisionMade_NullVector) {
    event_t event = event_create_decision_made(
        1, 0.5f, nullptr, 0,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    EXPECT_EQ(event.data.decision_made.decision_vector, nullptr);
    EXPECT_EQ(event.data.decision_made.vector_size, 0);
}

//=============================================================================
// EVENT CREATION TESTS - Custom
//=============================================================================

/**
 * WHAT: Test custom event creation
 * WHY:  Verify extensibility with custom data
 * HOW:  Create event with arbitrary data and check storage
 */
TEST_F(EventTypesTest, CreateCustom_ValidParameters) {
    const char* desc = "Custom test event";
    event_t event = event_create_custom(
        custom_data, sizeof(custom_data), desc,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM
    );

    EXPECT_EQ(event.type, EVENT_TYPE_CUSTOM);
    EXPECT_EQ(event.data.custom.data, custom_data);
    EXPECT_EQ(event.data.custom.data_size, sizeof(custom_data));
    EXPECT_STREQ(event.data.custom.description, desc);
}

/**
 * WHAT: Test custom event with NULL data
 * WHY:  Verify custom events can be metadata-only
 * HOW:  Pass NULL data pointer
 */
TEST_F(EventTypesTest, CreateCustom_NullData) {
    event_t event = event_create_custom(
        nullptr, 0, "No data event",
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_CUSTOM
    );

    EXPECT_EQ(event.data.custom.data, nullptr);
    EXPECT_EQ(event.data.custom.data_size, 0);
}

//=============================================================================
// EVENT UTILITY TESTS - Type Names
//=============================================================================

/**
 * WHAT: Test event_type_name returns correct strings
 * WHY:  Verify debugging/logging functions work
 * HOW:  Check all event type enum values
 */
TEST_F(EventTypesTest, EventTypeName_AllTypes) {
    EXPECT_STREQ(event_type_name(EVENT_TYPE_NONE), "NONE");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_SPIKE_BURST), "SPIKE_BURST");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_PATTERN_DETECTED), "PATTERN_DETECTED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_OSCILLATION_CHANGE), "OSCILLATION_CHANGE");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_ATTENTION_SHIFT), "ATTENTION_SHIFT");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_MEMORY_FORMED), "MEMORY_FORMED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_DECISION_MADE), "DECISION_MADE");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_ERROR_DETECTED), "ERROR_DETECTED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_SALIENCE_PEAK), "SALIENCE_PEAK");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_NOVELTY_DETECTED), "NOVELTY_DETECTED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_SURPRISE_DETECTED), "SURPRISE_DETECTED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_THRESHOLD_CROSSED), "THRESHOLD_CROSSED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_STATE_CHANGED), "STATE_CHANGED");
    EXPECT_STREQ(event_type_name(EVENT_TYPE_CUSTOM), "CUSTOM");
}

/**
 * WHAT: Test event_type_name with invalid type
 * WHY:  Verify out-of-range values don't crash
 * HOW:  Pass invalid enum value
 */
TEST_F(EventTypesTest, EventTypeName_Invalid) {
    const char* name = event_type_name(static_cast<event_type_t>(9999));
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// EVENT UTILITY TESTS - Source Names
//=============================================================================

/**
 * WHAT: Test event_source_name returns correct strings
 * WHY:  Verify source identification works
 * HOW:  Check all event source enum values
 */
TEST_F(EventTypesTest, EventSourceName_AllSources) {
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_UNKNOWN), "UNKNOWN");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_ENCODER), "ENCODER");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_FEATURE_EXTRACTOR), "FEATURE_EXTRACTOR");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_PATTERN_DETECTOR), "PATTERN_DETECTOR");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_ROUTER), "ROUTER");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_NORMALIZER), "NORMALIZER");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_BUFFER), "BUFFER");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_BRAIN), "BRAIN");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_ETHICS), "ETHICS");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_SALIENCE), "SALIENCE");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_WORKING_MEMORY), "WORKING_MEMORY");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_PREDICTIVE), "PREDICTIVE");
    EXPECT_STREQ(event_source_name(EVENT_SOURCE_CUSTOM), "CUSTOM");
}

/**
 * WHAT: Test event_source_name with invalid source
 * WHY:  Verify robustness against bad values
 * HOW:  Pass invalid enum value
 */
TEST_F(EventTypesTest, EventSourceName_Invalid) {
    const char* name = event_source_name(static_cast<event_source_t>(8888));
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// EVENT UTILITY TESTS - Priority Names
//=============================================================================

/**
 * WHAT: Test event_priority_name returns correct strings
 * WHY:  Verify priority identification works
 * HOW:  Check all priority enum values
 */
TEST_F(EventTypesTest, EventPriorityName_AllPriorities) {
    EXPECT_STREQ(event_priority_name(MW_EVENT_PRIORITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(event_priority_name(MW_EVENT_PRIORITY_HIGH), "HIGH");
    EXPECT_STREQ(event_priority_name(MW_EVENT_PRIORITY_NORMAL), "NORMAL");
    EXPECT_STREQ(event_priority_name(MW_EVENT_PRIORITY_LOW), "LOW");
    EXPECT_STREQ(event_priority_name(MW_EVENT_PRIORITY_BACKGROUND), "BACKGROUND");
}

/**
 * WHAT: Test event_priority_name with invalid priority
 * WHY:  Verify error handling for bad priority values
 * HOW:  Pass invalid enum value
 */
TEST_F(EventTypesTest, EventPriorityName_Invalid) {
    const char* name = event_priority_name(static_cast<mw_event_priority_t>(7777));
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// EVENT COPY TESTS
//=============================================================================

/**
 * WHAT: Test event_copy with NULL destination
 * WHY:  Verify NULL safety in copy operation
 * HOW:  Pass NULL dest pointer
 */
TEST_F(EventTypesTest, EventCopy_NullDest) {
    event_t src = event_create_salience_peak(
        0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE
    );

    bool result = event_copy(nullptr, &src);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test event_copy with NULL source
 * WHY:  Verify NULL safety in copy operation
 * HOW:  Pass NULL src pointer
 */
TEST_F(EventTypesTest, EventCopy_NullSrc) {
    event_t dest;
    bool result = event_copy(&dest, nullptr);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test event_copy with simple event (no pointers)
 * WHY:  Verify shallow copy works for basic events
 * HOW:  Copy salience event and verify all fields
 */
TEST_F(EventTypesTest, EventCopy_SimpleEvent) {
    event_t src = event_create_salience_peak(
        0.9f, 0.7f, 0.8f, 0.6f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_EQ(dest.type, src.type);
    EXPECT_EQ(dest.priority, src.priority);
    EXPECT_EQ(dest.source, src.source);
    EXPECT_FLOAT_EQ(dest.data.salience_peak.salience_score, 0.9f);
    EXPECT_FLOAT_EQ(dest.data.salience_peak.novelty_score, 0.7f);
}

/**
 * WHAT: Test event_copy with spike burst (has pointer)
 * WHY:  Verify deep copy of neuron_ids array
 * HOW:  Copy event, modify original, check copy unchanged
 */
TEST_F(EventTypesTest, EventCopy_SpikeBurstDeepCopy) {
    event_t src = event_create_spike_burst(
        neuron_ids, 3, 0.8f, 500,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_NE(dest.data.spike_burst.neuron_ids, src.data.spike_burst.neuron_ids);
    EXPECT_EQ(dest.data.spike_burst.num_neurons, 3);

    // Verify data was copied
    for (uint32_t i = 0; i < 3; i++) {
        EXPECT_EQ(dest.data.spike_burst.neuron_ids[i], neuron_ids[i]);
    }

    // Modify original, check copy unchanged
    neuron_ids[0] = 999;
    EXPECT_NE(dest.data.spike_burst.neuron_ids[0], 999);

    event_free(&dest);
}

/**
 * WHAT: Test event_copy with memory formed (has float array)
 * WHY:  Verify deep copy of memory trace
 * HOW:  Copy event and verify independent allocation
 */
TEST_F(EventTypesTest, EventCopy_MemoryFormedDeepCopy) {
    event_t src = event_create_memory_formed(
        456, memory_trace, 3, 0.9f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_WORKING_MEMORY
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_NE(dest.data.memory_formed.memory_trace, src.data.memory_formed.memory_trace);
    EXPECT_EQ(dest.data.memory_formed.trace_size, 3);

    // Verify data copied correctly
    for (uint32_t i = 0; i < 3; i++) {
        EXPECT_FLOAT_EQ(dest.data.memory_formed.memory_trace[i], memory_trace[i]);
    }

    event_free(&dest);
}

/**
 * WHAT: Test event_copy with decision made (has float vector)
 * WHY:  Verify deep copy of decision vector
 * HOW:  Copy event and verify independence
 */
TEST_F(EventTypesTest, EventCopy_DecisionMadeDeepCopy) {
    event_t src = event_create_decision_made(
        789, 0.95f, decision_vector, 4,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_NE(dest.data.decision_made.decision_vector, src.data.decision_made.decision_vector);
    EXPECT_EQ(dest.data.decision_made.vector_size, 4);

    // Verify data copied
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(dest.data.decision_made.decision_vector[i], decision_vector[i]);
    }

    event_free(&dest);
}

/**
 * WHAT: Test event_copy with custom event (has void* data)
 * WHY:  Verify deep copy of custom data
 * HOW:  Copy event and verify independent allocation
 */
TEST_F(EventTypesTest, EventCopy_CustomEventDeepCopy) {
    event_t src = event_create_custom(
        custom_data, sizeof(custom_data), "Test",
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_CUSTOM
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_NE(dest.data.custom.data, src.data.custom.data);
    EXPECT_EQ(dest.data.custom.data_size, sizeof(custom_data));

    // Verify data copied
    uint32_t* dest_data = static_cast<uint32_t*>(dest.data.custom.data);
    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(dest_data[i], custom_data[i]);
    }

    event_free(&dest);
}

/**
 * WHAT: Test event_copy with NULL neuron_ids in spike burst
 * WHY:  Verify NULL pointer handling in deep copy
 * HOW:  Copy event with NULL neuron_ids
 */
TEST_F(EventTypesTest, EventCopy_NullNeuronIds) {
    event_t src = event_create_spike_burst(
        nullptr, 0, 0.5f, 100,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    bool result = event_copy(&dest, &src);

    EXPECT_TRUE(result);
    EXPECT_EQ(dest.data.spike_burst.neuron_ids, nullptr);
}

//=============================================================================
// EVENT FREE TESTS
//=============================================================================

/**
 * WHAT: Test event_free with NULL pointer
 * WHY:  Verify NULL safety in free operation
 * HOW:  Call event_free with NULL
 */
TEST_F(EventTypesTest, EventFree_NullPointer) {
    // Should not crash
    event_free(nullptr);
    EXPECT_TRUE(true);
}

/**
 * WHAT: Test event_free with spike burst
 * WHY:  Verify neuron_ids are freed and nulled
 * HOW:  Create, copy, free, check pointer is NULL
 */
TEST_F(EventTypesTest, EventFree_SpikeBurst) {
    event_t src = event_create_spike_burst(
        neuron_ids, 3, 0.7f, 200,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    event_copy(&dest, &src);

    event_free(&dest);
    EXPECT_EQ(dest.data.spike_burst.neuron_ids, nullptr);
}

/**
 * WHAT: Test event_free with memory formed
 * WHY:  Verify memory trace is freed and nulled
 * HOW:  Create, copy, free, check pointer is NULL
 */
TEST_F(EventTypesTest, EventFree_MemoryFormed) {
    event_t src = event_create_memory_formed(
        111, memory_trace, 3, 0.8f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_WORKING_MEMORY
    );

    event_t dest;
    event_copy(&dest, &src);

    event_free(&dest);
    EXPECT_EQ(dest.data.memory_formed.memory_trace, nullptr);
}

/**
 * WHAT: Test event_free with decision made
 * WHY:  Verify decision vector is freed and nulled
 * HOW:  Create, copy, free, check pointer is NULL
 */
TEST_F(EventTypesTest, EventFree_DecisionMade) {
    event_t src = event_create_decision_made(
        222, 0.85f, decision_vector, 4,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    event_copy(&dest, &src);

    event_free(&dest);
    EXPECT_EQ(dest.data.decision_made.decision_vector, nullptr);
}

/**
 * WHAT: Test event_free with custom event
 * WHY:  Verify custom data is freed and nulled
 * HOW:  Create, copy, free, check pointer is NULL
 */
TEST_F(EventTypesTest, EventFree_CustomEvent) {
    event_t src = event_create_custom(
        custom_data, sizeof(custom_data), "Free test",
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_CUSTOM
    );

    event_t dest;
    event_copy(&dest, &src);

    event_free(&dest);
    EXPECT_EQ(dest.data.custom.data, nullptr);
}

/**
 * WHAT: Test event_free with simple event (no pointers)
 * WHY:  Verify free doesn't crash on events without allocations
 * HOW:  Create salience event and free it
 */
TEST_F(EventTypesTest, EventFree_SimpleEvent) {
    event_t event = event_create_salience_peak(
        0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE
    );

    // Should not crash
    event_free(&event);
    EXPECT_TRUE(true);
}

/**
 * WHAT: Test double free safety
 * WHY:  Verify freeing already-freed event doesn't crash
 * HOW:  Free event twice
 */
TEST_F(EventTypesTest, EventFree_DoubleFree) {
    event_t src = event_create_spike_burst(
        neuron_ids, 3, 0.6f, 300,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
    );

    event_t dest;
    event_copy(&dest, &src);

    event_free(&dest);
    // Second free should not crash (pointer already NULL)
    event_free(&dest);
    EXPECT_TRUE(true);
}

//=============================================================================
// EVENT PRINT TESTS
//=============================================================================

/**
 * WHAT: Test event_print with NULL pointer
 * WHY:  Verify NULL safety in print operation
 * HOW:  Call event_print with NULL
 */
TEST_F(EventTypesTest, EventPrint_NullPointer) {
    // Should not crash
    event_print(nullptr);
    EXPECT_TRUE(true);
}

/**
 * WHAT: Test event_print with all event types
 * WHY:  Verify print works for each event type
 * HOW:  Create and print each type (visual inspection in logs)
 */
TEST_F(EventTypesTest, EventPrint_AllTypes) {
    event_t e1 = event_create_spike_burst(neuron_ids, 3, 0.8f, 100,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_BRAIN);
    event_print(&e1);

    event_t e2 = event_create_pattern_detected(1, 0.9f, 5, "Test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR);
    event_print(&e2);

    event_t e3 = event_create_attention_shift(1, 2, 0.7f, "Novel",
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE);
    event_print(&e3);

    event_t e4 = event_create_salience_peak(0.9f, 0.7f, 0.8f, 0.6f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE);
    event_print(&e4);

    event_t e5 = event_create_error_detected(5.0f, 3.0f, 2.0f, 10,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PREDICTIVE);
    event_print(&e5);

    EXPECT_TRUE(true);  // If we get here, no crashes occurred
}

//=============================================================================
// METADATA TESTS
//=============================================================================

/**
 * WHAT: Test timestamp is set on event creation
 * WHY:  Verify all events get timestamped
 * HOW:  Create event and check timestamp > 0
 */
TEST_F(EventTypesTest, Metadata_TimestampSet) {
    event_t event = event_create_salience_peak(
        0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE
    );

    EXPECT_GT(event.timestamp_us, 0);
}

/**
 * WHAT: Test sequence numbers are monotonically increasing
 * WHY:  Verify global ordering works
 * HOW:  Create multiple events and check sequence numbers
 */
TEST_F(EventTypesTest, Metadata_SequenceNumberOrdering) {
    event_t e1 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);
    event_t e2 = event_create_salience_peak(0.6f, 0.6f, 0.6f, 0.6f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);
    event_t e3 = event_create_salience_peak(0.7f, 0.7f, 0.7f, 0.7f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    EXPECT_LT(e1.sequence_number, e2.sequence_number);
    EXPECT_LT(e2.sequence_number, e3.sequence_number);
}

/**
 * WHAT: Test all priority levels are assignable
 * WHY:  Verify priority system works correctly
 * HOW:  Create events with each priority level
 */
TEST_F(EventTypesTest, Metadata_AllPriorityLevels) {
    event_t e1 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE);
    EXPECT_EQ(e1.priority, MW_EVENT_PRIORITY_CRITICAL);

    event_t e2 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE);
    EXPECT_EQ(e2.priority, MW_EVENT_PRIORITY_HIGH);

    event_t e3 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);
    EXPECT_EQ(e3.priority, MW_EVENT_PRIORITY_NORMAL);

    event_t e4 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_SALIENCE);
    EXPECT_EQ(e4.priority, MW_EVENT_PRIORITY_LOW);

    event_t e5 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_BACKGROUND, EVENT_SOURCE_SALIENCE);
    EXPECT_EQ(e5.priority, MW_EVENT_PRIORITY_BACKGROUND);
}

/**
 * WHAT: Test all event sources are assignable
 * WHY:  Verify source tracking works
 * HOW:  Create events with different sources
 */
TEST_F(EventTypesTest, Metadata_AllEventSources) {
    event_t e1 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_ENCODER);
    EXPECT_EQ(e1.source, EVENT_SOURCE_ENCODER);

    event_t e2 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    EXPECT_EQ(e2.source, EVENT_SOURCE_BRAIN);

    event_t e3 = event_create_salience_peak(0.5f, 0.5f, 0.5f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_WORKING_MEMORY);
    EXPECT_EQ(e3.source, EVENT_SOURCE_WORKING_MEMORY);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

/**
 * WHAT: Test creating many events rapidly
 * WHY:  Verify no memory leaks or crashes under load
 * HOW:  Create 1000 events in a loop
 */
TEST_F(EventTypesTest, Stress_ManyEventCreations) {
    std::vector<event_t> events;
    events.reserve(1000);

    for (int i = 0; i < 1000; i++) {
        event_t event = event_create_salience_peak(
            0.5f, 0.5f, 0.5f, 0.5f,
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE
        );
        events.push_back(event);
    }

    EXPECT_EQ(events.size(), 1000);
}

/**
 * WHAT: Test creating and freeing many events with allocations
 * WHY:  Verify memory management works under load
 * HOW:  Create/copy/free 100 spike burst events
 */
TEST_F(EventTypesTest, Stress_ManyAllocations) {
    for (int i = 0; i < 100; i++) {
        event_t src = event_create_spike_burst(
            neuron_ids, 3, 0.8f, 100,
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN
        );

        event_t dest;
        bool result = event_copy(&dest, &src);
        EXPECT_TRUE(result);

        event_free(&dest);
    }

    EXPECT_TRUE(true);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================

/**
 * WHAT: Test event type enum doesn't exceed expected range
 * WHY:  Prevent accidental enum value changes
 * HOW:  Check EVENT_TYPE_COUNT value
 */
TEST_F(EventTypesTest, Regression_EventTypeCount) {
    // Should be 22 types (includes new event categories added)
    EXPECT_EQ(EVENT_TYPE_COUNT, 22);
}

/**
 * WHAT: Test event_t structure size is reasonable
 * WHY:  Prevent bloat, verify cache-friendly size
 * HOW:  Check sizeof(event_t) is within expected range
 */
TEST_F(EventTypesTest, Regression_EventSize) {
    // Should fit in ~2 cache lines (128 bytes)
    size_t event_size = sizeof(event_t);
    EXPECT_LE(event_size, 256);  // Allow some overhead
    EXPECT_GE(event_size, 32);   // Minimum reasonable size
}

/**
 * WHAT: Test event copy preserves metadata
 * WHY:  Regression check for timestamp/sequence preservation
 * HOW:  Copy event and verify all metadata matches
 */
TEST_F(EventTypesTest, Regression_CopyPreservesMetadata) {
    event_t src = event_create_salience_peak(
        0.9f, 0.8f, 0.7f, 0.6f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE
    );

    event_t dest;
    event_copy(&dest, &src);

    EXPECT_EQ(dest.type, src.type);
    EXPECT_EQ(dest.priority, src.priority);
    EXPECT_EQ(dest.source, src.source);
    EXPECT_EQ(dest.timestamp_us, src.timestamp_us);
    EXPECT_EQ(dest.sequence_number, src.sequence_number);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

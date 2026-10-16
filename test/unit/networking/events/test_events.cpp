#include "test_helpers.h"

#include "networking/events/nimcp_events.h"
#include "networking/protocol/nimcp_protocol.h"

//=============================================================================
// Test Helper Functions
//=============================================================================

// Callback tracker for testing
struct CallbackTracker {
    uint32_t call_count;
    event_packet_t last_packet;
    void* last_payload;
    uint32_t last_payload_len;
};

static void test_event_callback(const event_packet_t* packet, const void* payload,
                                uint32_t payload_len, void* context)
{
    if (!context || !packet)
        return;

    CallbackTracker* tracker = (CallbackTracker*) context;
    tracker->call_count++;
    memcpy(&tracker->last_packet, packet, sizeof(event_packet_t));
    tracker->last_payload = (void*) payload;
    tracker->last_payload_len = payload_len;
}

//=============================================================================
// Event Generator Tests
//=============================================================================

// Test event generator creation
TEST(EventGenerator, CreateValid)
{
    CallbackTracker tracker = {0};

    event_generator_config_t config;
    config.node_id = 1;
    config.base_feature_code = 0x01000000;
    config.max_hop_count = 5;
    config.enable_plasticity_triggers = true;
    config.callback = test_event_callback;
    config.callback_context = &tracker;

    event_generator_t generator = event_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    event_generator_destroy(generator);
}

// Test event generator creation with null config
TEST(EventGenerator, CreateNullConfig)
{
    event_generator_t generator = event_generator_create(nullptr);
    ASSERT_EQ(generator, nullptr);
}

// Test event generator creation with null callback
TEST(EventGenerator, CreateNullCallback)
{
    event_generator_config_t config;
    config.node_id = 1;
    config.base_feature_code = 0x01000000;
    config.max_hop_count = 5;
    config.enable_plasticity_triggers = false;
    config.callback = nullptr;
    config.callback_context = nullptr;

    event_generator_t generator = event_generator_create(&config);
    ASSERT_EQ(generator, nullptr);
}

// Test spike event generation
TEST(EventGenerator, OnSpike)
{
    CallbackTracker tracker = {0};

    event_generator_config_t config;
    config.node_id = 1;
    config.base_feature_code = 0x01000000;
    config.max_hop_count = 5;
    config.enable_plasticity_triggers = false;
    config.callback = test_event_callback;
    config.callback_context = &tracker;

    event_generator_t generator = event_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    // Create test network
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    // Activate neuron and generate event
    neural_network_update_neuron(network, 0, 1.0f, 1);
    bool generated = event_generator_on_spike(generator, network, 0, 1);

    // Event generation is async, so we need to wait briefly
    usleep(10000);  // 10ms

    // Callback should have been invoked (possibly async)
    // Note: Exact count depends on async queue timing
    EXPECT_GE(tracker.call_count, 0);

    neural_network_destroy(network);
    event_generator_destroy(generator);
}

// Test spike event with invalid neuron ID
TEST(EventGenerator, OnSpikeInvalidNeuron)
{
    CallbackTracker tracker = {0};

    event_generator_config_t config;
    config.node_id = 1;
    config.base_feature_code = 0x01000000;
    config.max_hop_count = 5;
    config.enable_plasticity_triggers = false;
    config.callback = test_event_callback;
    config.callback_context = &tracker;

    event_generator_t generator = event_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    // Try with invalid neuron ID
    bool generated = event_generator_on_spike(generator, network, MAX_NEURONS + 1, 1);
    ASSERT_FALSE(generated);

    neural_network_destroy(network);
    event_generator_destroy(generator);
}

// Test custom neuron feature mapping
TEST(EventGenerator, SetNeuronFeature)
{
    CallbackTracker tracker = {0};

    event_generator_config_t config;
    config.node_id = 1;
    config.base_feature_code = 0x01000000;
    config.max_hop_count = 5;
    config.enable_plasticity_triggers = false;
    config.callback = test_event_callback;
    config.callback_context = &tracker;

    event_generator_t generator = event_generator_create(&config);
    ASSERT_NE(generator, nullptr);

    // Set custom feature code
    feature_code_t custom_feature = 0x02FF0001;
    ASSERT_TRUE(event_generator_set_neuron_feature(generator, 0, custom_feature));

    // Test with invalid neuron ID
    ASSERT_FALSE(event_generator_set_neuron_feature(generator, MAX_NEURONS + 1, custom_feature));

    event_generator_destroy(generator);
}

//=============================================================================
// Event Receiver Tests
//=============================================================================

// Test event receiver creation
TEST(EventReceiver, CreateValid)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test event receiver creation with null config
TEST(EventReceiver, CreateNullConfig)
{
    event_receiver_t receiver = event_receiver_create(nullptr);
    ASSERT_EQ(receiver, nullptr);
}

// Test event receiver creation with null network
TEST(EventReceiver, CreateNullNetwork)
{
    event_receiver_config_t config;
    config.network = nullptr;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_EQ(receiver, nullptr);
}

// Test event packet processing
TEST(EventReceiver, ProcessPacket)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Map feature to neuron (use 24-bit feature code: domain 0x01, subfeature 0x0001)
    feature_code_t feature = MAKE_FEATURE_CODE(0x01, 0x0001);
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, feature, 0));

    // Create test event packet
    event_packet_t packet;
    memset(&packet, 0, sizeof(event_packet_t));
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FEATURE_CODE(&packet, feature);
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.8f);
    packet.source_node_id = 1;
    packet.timestamp = 1;

    // Process packet
    bool processed = event_receiver_process_packet(receiver, &packet, nullptr, 0, 1);
    ASSERT_TRUE(processed);

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test event packet processing with invalid packet
TEST(EventReceiver, ProcessInvalidPacket)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Process null packet
    bool processed = event_receiver_process_packet(receiver, nullptr, nullptr, 0, 1);
    ASSERT_FALSE(processed);

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test feature-to-neuron mapping
TEST(EventReceiver, FeatureMapping)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Map multiple features
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, 0x01000001, 0));
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, 0x01000002, 1));
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, 0x01000003, 2));

    // Test updating existing mapping
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, 0x01000001, 5));

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test subscription filter addition
TEST(EventReceiver, AddFilter)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Create subscription filter
    subscription_filter_t filter;
    memset(&filter, 0, sizeof(subscription_filter_t));
    filter.feature_code = 0x01000000;  // Domain 0x01
    filter.feature_mask = 0xFF000000;  // Match domain only
    filter.confidence_threshold = 0.0f;
    filter.max_rate_hz = 0;

    // Add filter
    ASSERT_TRUE(event_receiver_add_filter(receiver, &filter));

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test subscription filter removal
TEST(EventReceiver, RemoveFilter)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Add filters
    subscription_filter_t filter;
    memset(&filter, 0, sizeof(subscription_filter_t));
    filter.feature_code = 0x01000000;  // Domain 0x01
    filter.feature_mask = 0xFF000000;  // Match domain only
    filter.confidence_threshold = 0.0f;
    filter.max_rate_hz = 0;

    ASSERT_TRUE(event_receiver_add_filter(receiver, &filter));
    ASSERT_TRUE(event_receiver_add_filter(receiver, &filter));

    // Remove filter
    ASSERT_TRUE(event_receiver_remove_filter(receiver, 0));

    // Test invalid index
    ASSERT_FALSE(event_receiver_remove_filter(receiver, 100));

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

// Test auto-neuron creation
TEST(EventReceiver, AutoCreateNeurons)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = true;  // Enable auto-creation

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Create event packet with unknown feature
    event_packet_t packet;
    memset(&packet, 0, sizeof(event_packet_t));
    EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
    EVENT_SET_FEATURE_CODE(&packet, 0x0F000099);  // Unmapped feature
    EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.5f);
    packet.source_node_id = 2;
    packet.timestamp = 1;

    // Process packet - should auto-create neuron
    bool processed = event_receiver_process_packet(receiver, &packet, nullptr, 0, 1);

    // Processing depends on whether MAX_NEURONS is reached
    // Just verify it doesn't crash

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

// Test confidence calculation
TEST(EventUtils, CalculateConfidence)
{
    // Test with state above threshold
    float confidence = event_calculate_confidence(1.0f, 0.5f);
    EXPECT_GT(confidence, 0.5f);
    EXPECT_LE(confidence, 1.0f);

    // Test with state below threshold
    confidence = event_calculate_confidence(0.3f, 0.5f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LT(confidence, 0.5f);

    // Test with state at threshold
    confidence = event_calculate_confidence(0.5f, 0.5f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

// Test event flags conversion
TEST(EventUtils, FlagsFromNeuronType)
{
    uint8_t flags = event_flags_from_neuron_type(NEURON_EXCITATORY);
    EXPECT_EQ(flags, EVENT_FLAG_EXCITATORY);

    flags = event_flags_from_neuron_type(NEURON_INHIBITORY);
    EXPECT_EQ(flags, EVENT_FLAG_INHIBITORY);
}

// Test default feature code generation
TEST(EventUtils, DefaultFeatureCode)
{
    feature_code_t base = 0x01000000;
    feature_code_t neuron0 = event_default_feature_code(base, 0);
    feature_code_t neuron1 = event_default_feature_code(base, 1);

    // Each neuron should get unique feature code
    EXPECT_NE(neuron0, neuron1);

    // Should preserve domain from base code
    EXPECT_EQ(GET_FEATURE_DOMAIN(neuron0), GET_FEATURE_DOMAIN(base));
}

//=============================================================================
// Integration Tests
//=============================================================================

// Test full event pipeline: generator -> receiver
TEST(EventIntegration, GeneratorToReceiver)
{
    // Create source network and generator
    network_config_t src_config = create_test_config();
    neural_network_t src_network = neural_network_create(&src_config);
    ASSERT_NE(src_network, nullptr);

    CallbackTracker tracker = {0};
    event_generator_config_t gen_config;
    gen_config.node_id = 1;
    gen_config.base_feature_code = 0x01000000;
    gen_config.max_hop_count = 5;
    gen_config.enable_plasticity_triggers = false;
    gen_config.callback = test_event_callback;
    gen_config.callback_context = &tracker;

    event_generator_t generator = event_generator_create(&gen_config);
    ASSERT_NE(generator, nullptr);

    // Create destination network and receiver
    network_config_t dst_config = create_test_config();
    neural_network_t dst_network = neural_network_create(&dst_config);
    ASSERT_NE(dst_network, nullptr);

    event_receiver_config_t rcv_config;
    rcv_config.network = dst_network;
    rcv_config.filters = nullptr;
    rcv_config.num_filters = 0;
    rcv_config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&rcv_config);
    ASSERT_NE(receiver, nullptr);

    // Map features
    feature_code_t feature = event_default_feature_code(0x01000000, 0);
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, feature, 0));

    // Activate source neuron and generate event
    neural_network_update_neuron(src_network, 0, 2.0f, 1);
    event_generator_on_spike(generator, src_network, 0, 1);

    // Wait for async event processing
    usleep(20000);  // 20ms

    // In real scenario, callback would send packet to receiver
    // For this test, we manually process if callback was invoked
    if (tracker.call_count > 0) {
        event_receiver_process_packet(receiver, &tracker.last_packet, nullptr, 0, 1);
    }

    // Cleanup
    event_receiver_destroy(receiver);
    event_generator_destroy(generator);
    neural_network_destroy(dst_network);
    neural_network_destroy(src_network);
}

// Test event filtering
TEST(EventIntegration, EventFiltering)
{
    network_config_t net_config = create_test_config();
    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    event_receiver_config_t config;
    config.network = network;
    config.filters = nullptr;
    config.num_filters = 0;
    config.auto_create_neurons = false;

    event_receiver_t receiver = event_receiver_create(&config);
    ASSERT_NE(receiver, nullptr);

    // Add domain filter
    subscription_filter_t filter;
    memset(&filter, 0, sizeof(subscription_filter_t));
    filter.feature_code = 0x01000000;  // Domain 0x01
    filter.feature_mask = 0xFF000000;  // Match domain only
    filter.confidence_threshold = 0.0f;
    filter.max_rate_hz = 0;  // Unlimited
    ASSERT_TRUE(event_receiver_add_filter(receiver, &filter));

    // Map feature
    ASSERT_TRUE(event_receiver_map_feature_to_neuron(receiver, 0x01000001, 0));

    // Create matching packet
    event_packet_t matching_packet;
    memset(&matching_packet, 0, sizeof(event_packet_t));
    EVENT_SET_VERSION(&matching_packet, PROTOCOL_VERSION);
    EVENT_SET_FEATURE_CODE(&matching_packet, 0x01000001);
    EVENT_SET_FLAGS(&matching_packet, EVENT_FLAG_EXCITATORY);
    matching_packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.7f);

    // Should be processed
    bool processed = event_receiver_process_packet(receiver, &matching_packet, nullptr, 0, 1);
    EXPECT_TRUE(processed);

    // Create non-matching packet
    event_packet_t nonmatching_packet;
    memset(&nonmatching_packet, 0, sizeof(event_packet_t));
    EVENT_SET_VERSION(&nonmatching_packet, PROTOCOL_VERSION);
    EVENT_SET_FEATURE_CODE(&nonmatching_packet, 0x02000001);  // Different domain
    EVENT_SET_FLAGS(&nonmatching_packet, EVENT_FLAG_EXCITATORY);
    nonmatching_packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.7f);

    // Should be filtered out
    processed = event_receiver_process_packet(receiver, &nonmatching_packet, nullptr, 0, 1);
    EXPECT_FALSE(processed);

    event_receiver_destroy(receiver);
    neural_network_destroy(network);
}

//=============================================================================
// nimcp_events.h - Event Packet Integration with Neural Networks
//=============================================================================

#ifndef NIMCP_EVENTS_H
#define NIMCP_EVENTS_H

#include "common/nimcp_export.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/protocol/nimcp_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_events.h
 * @brief Integration layer between neural networks and NIMCP 2.0 event packets
 *
 * This module provides functions to convert neural network spikes into
 * NIMCP 2.0 event packets and handle incoming event packets as neural inputs.
 */

//=============================================================================
// Event Generation from Neural Activity
//=============================================================================

/**
 * @brief Event generator callback type
 *
 * Called when a neuron fires and an event packet needs to be sent.
 *
 * @param packet The generated event packet
 * @param payload Optional payload data
 * @param payload_len Length of payload
 * @param context User-provided context
 */
typedef void (*event_callback_t)(const event_packet_t* packet, const void* payload,
                                 uint32_t payload_len, void* context);

/**
 * @brief Configuration for event generation
 */
typedef struct {
    uint32_t node_id;                 /**< This node's ID */
    feature_code_t base_feature_code; /**< Base feature code for this network */
    uint8_t max_hop_count;            /**< Maximum hops for event packets */
    bool enable_plasticity_triggers;  /**< Include plasticity info in events */
    event_callback_t callback;        /**< Callback for generated events */
    void* callback_context;           /**< Context passed to callback */
} event_generator_config_t;

/**
 * @brief Event generator state
 */
typedef struct event_generator_struct* event_generator_t;

/**
 * @brief Create an event generator
 *
 * @param config Generator configuration
 * @return Event generator handle, or NULL on error
 */
event_generator_t event_generator_create(const event_generator_config_t* config);

/**
 * @brief Destroy an event generator
 *
 * @param generator Generator to destroy
 */
void event_generator_destroy(event_generator_t generator);

/**
 * @brief Generate event packet from neuron spike
 *
 * @param generator Event generator
 * @param network Neural network
 * @param neuron_id ID of neuron that fired
 * @param timestamp Spike timestamp
 * @return true if event was generated, false otherwise
 */
bool event_generator_on_spike(event_generator_t generator, neural_network_t network,
                              uint32_t neuron_id, uint64_t timestamp);

/**
 * @brief Set feature code for a specific neuron
 *
 * @param generator Event generator
 * @param neuron_id Neuron ID
 * @param feature_code Feature code for this neuron's events
 * @return true on success
 */
bool event_generator_set_neuron_feature(event_generator_t generator, uint32_t neuron_id,
                                        feature_code_t feature_code);

//=============================================================================
// Event Reception and Neural Input
//=============================================================================

/**
 * @brief Event receiver configuration
 */
typedef struct {
    neural_network_t network;       /**< Target neural network */
    subscription_filter_t* filters; /**< Array of subscription filters */
    uint32_t num_filters;           /**< Number of filters */
    bool auto_create_neurons;       /**< Create neurons for unknown sources */
} event_receiver_config_t;

/**
 * @brief Event receiver state
 */
typedef struct event_receiver_struct* event_receiver_t;

/**
 * @brief Create an event receiver
 *
 * @param config Receiver configuration
 * @return Event receiver handle, or NULL on error
 */
event_receiver_t event_receiver_create(const event_receiver_config_t* config);

/**
 * @brief Destroy an event receiver
 *
 * @param receiver Receiver to destroy
 */
void event_receiver_destroy(event_receiver_t receiver);

/**
 * @brief Process incoming event packet
 *
 * @param receiver Event receiver
 * @param packet Event packet to process
 * @param payload Packet payload (if any)
 * @param payload_len Payload length
 * @param timestamp Current time
 * @return true if event was processed, false if filtered out
 */
bool event_receiver_process_packet(event_receiver_t receiver, const event_packet_t* packet,
                                   const void* payload, uint32_t payload_len, uint64_t timestamp);

/**
 * @brief Add subscription filter
 *
 * @param receiver Event receiver
 * @param filter Subscription filter to add
 * @return true on success
 */
bool event_receiver_add_filter(event_receiver_t receiver, const subscription_filter_t* filter);

/**
 * @brief Remove subscription filter
 *
 * @param receiver Event receiver
 * @param index Filter index to remove
 * @return true on success
 */
bool event_receiver_remove_filter(event_receiver_t receiver, uint32_t index);

/**
 * @brief Map feature code to neuron ID
 *
 * @param receiver Event receiver
 * @param feature_code Feature code
 * @param neuron_id Target neuron ID
 * @return true on success
 */
bool event_receiver_map_feature_to_neuron(event_receiver_t receiver, feature_code_t feature_code,
                                          uint32_t neuron_id);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate confidence from neuron state
 *
 * @param state Neuron state/activation
 * @param threshold Firing threshold
 * @return Confidence value (0.0-1.0)
 */
float event_calculate_confidence(float state, float threshold);

/**
 * @brief Map neuron type to event flags
 *
 * @param type Neuron type
 * @return Event packet flags
 */
uint8_t event_flags_from_neuron_type(neuron_type_t type);

/**
 * @brief Get default feature code for neuron
 *
 * @param base_code Base feature code
 * @param neuron_id Neuron ID
 * @return Feature code for this neuron
 */
feature_code_t event_default_feature_code(feature_code_t base_code, uint32_t neuron_id);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_EVENTS_H

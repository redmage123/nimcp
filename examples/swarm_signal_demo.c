/**
 * @file swarm_signal_demo.c
 * @brief Example demonstrating NIMCP Swarm Signal Adapter usage
 *
 * This example shows how to:
 * - Create signal adapters for different radio types
 * - Send and receive messages
 * - Use broadcast functionality
 * - Track statistics
 * - Use simulation mode for testing
 */

#include "swarm/nimcp_swarm_signal.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEMO_MSG_SIZE 64

/**
 * @brief Demo: Basic send and receive with simulation radio
 */
static void demo_simulation_radio(void) {
    printf("\n=== Simulation Radio Demo ===\n");

    /* Create two adapters for simulation */
    swarm_signal_config_t config1 = {
        .radio_type = SWARM_RADIO_SIMULATION,
        .frequency_hz = 915000000,  /* 915 MHz */
        .bandwidth_hz = 125000,     /* 125 kHz */
        .tx_power_dbm = 14,
        .max_packet_size = 128,
        .retry_count = 3,
        .timeout_ms = 1000
    };

    swarm_signal_config_t config2 = config1;

    nimcp_swarm_signal_adapter_t* adapter1 = swarm_signal_adapter_create(&config1);
    nimcp_swarm_signal_adapter_t* adapter2 = swarm_signal_adapter_create(&config2);

    if (!adapter1 || !adapter2) {
        printf("Failed to create adapters\n");
        return;
    }

    printf("Created two simulation adapters\n");

    /* Send message from adapter1 */
    const char* msg = "Hello from Node 1!";
    printf("Adapter 1 sending: '%s'\n", msg);

    if (swarm_signal_broadcast(adapter1, (const uint8_t*)msg, strlen(msg) + 1)) {
        printf("Broadcast successful\n");
    } else {
        printf("Broadcast failed\n");
    }

    /* Small delay for message propagation */
    usleep(10000);

    /* Receive on adapter2 */
    uint8_t recv_buffer[DEMO_MSG_SIZE];
    uint32_t recv_len;
    uint32_t source_id;

    if (swarm_signal_receive(adapter2, recv_buffer, DEMO_MSG_SIZE,
                            &recv_len, &source_id)) {
        printf("Adapter 2 received: '%s' (from node %u, %u bytes)\n",
               (char*)recv_buffer, source_id, recv_len);
    } else {
        printf("No message received\n");
    }

    /* Get statistics */
    swarm_signal_stats_t stats1, stats2;
    swarm_signal_get_stats(adapter1, &stats1);
    swarm_signal_get_stats(adapter2, &stats2);

    printf("\nAdapter 1 Stats:\n");
    printf("  Packets sent: %lu\n", stats1.packets_sent);
    printf("  Bytes sent: %lu\n", stats1.bytes_sent);

    printf("\nAdapter 2 Stats:\n");
    printf("  Packets received: %lu\n", stats2.packets_received);
    printf("  Bytes received: %lu\n", stats2.bytes_received);
    printf("  Avg latency: %.3f ms\n", stats2.avg_latency_ms);

    /* Cleanup */
    swarm_signal_adapter_destroy(adapter1);
    swarm_signal_adapter_destroy(adapter2);
}

/**
 * @brief Demo: Multiple nodes broadcasting
 */
static void demo_multi_node_broadcast(void) {
    printf("\n=== Multi-Node Broadcast Demo ===\n");

    const int NUM_NODES = 4;
    nimcp_swarm_signal_adapter_t* adapters[NUM_NODES];

    /* Create multiple adapters */
    swarm_signal_config_t config = {
        .radio_type = SWARM_RADIO_SIMULATION,
        .frequency_hz = 915000000,
        .bandwidth_hz = 125000,
        .tx_power_dbm = 14,
        .max_packet_size = 128,
        .retry_count = 3,
        .timeout_ms = 1000
    };

    for (int i = 0; i < NUM_NODES; i++) {
        adapters[i] = swarm_signal_adapter_create(&config);
        if (!adapters[i]) {
            printf("Failed to create adapter %d\n", i);
            return;
        }
    }

    printf("Created %d nodes\n", NUM_NODES);

    /* Node 0 broadcasts a message */
    const char* broadcast_msg = "Broadcast from Node 0!";
    printf("\nNode 0 broadcasting: '%s'\n", broadcast_msg);

    if (swarm_signal_broadcast(adapters[0], (const uint8_t*)broadcast_msg,
                              strlen(broadcast_msg) + 1)) {
        printf("Broadcast sent\n");
    }

    /* Small delay */
    usleep(10000);

    /* All other nodes try to receive */
    printf("\nReceiving on other nodes:\n");
    for (int i = 1; i < NUM_NODES; i++) {
        uint8_t recv_buffer[DEMO_MSG_SIZE];
        uint32_t recv_len;
        uint32_t source_id;

        if (swarm_signal_receive(adapters[i], recv_buffer, DEMO_MSG_SIZE,
                                &recv_len, &source_id)) {
            printf("  Node %d received: '%s'\n", i, (char*)recv_buffer);
        } else {
            printf("  Node %d: no message\n", i);
        }
    }

    /* Cleanup */
    for (int i = 0; i < NUM_NODES; i++) {
        swarm_signal_adapter_destroy(adapters[i]);
    }
}

/**
 * @brief Demo: LoRa configuration
 */
static void demo_lora_config(void) {
    printf("\n=== LoRa Configuration Demo ===\n");

    swarm_signal_config_t lora_config = {
        .radio_type = SWARM_RADIO_LORA,
        .frequency_hz = 868000000,  /* 868 MHz (EU band) */
        .bandwidth_hz = 125000,     /* 125 kHz */
        .tx_power_dbm = 20,         /* 20 dBm (100 mW) */
        .max_packet_size = 255,     /* LoRa max payload */
        .retry_count = 5,
        .timeout_ms = 2000
    };

    nimcp_swarm_signal_adapter_t* lora_adapter =
        swarm_signal_adapter_create(&lora_config);

    if (!lora_adapter) {
        printf("Failed to create LoRa adapter\n");
        return;
    }

    printf("Created LoRa adapter:\n");
    printf("  Radio type: %s\n",
           swarm_signal_radio_type_string(lora_config.radio_type));
    printf("  Frequency: %u Hz\n", lora_config.frequency_hz);
    printf("  Bandwidth: %u Hz\n", lora_config.bandwidth_hz);
    printf("  TX Power: %d dBm\n", lora_config.tx_power_dbm);
    printf("  Max packet: %u bytes\n", lora_config.max_packet_size);

    /* Test power control */
    printf("\nTesting power control:\n");
    swarm_signal_set_tx_power(lora_adapter, 10);
    printf("  Set power to 10 dBm\n");
    printf("  Current power: %d dBm\n",
           swarm_signal_get_tx_power(lora_adapter));

    printf("  Operational: %s\n",
           swarm_signal_is_operational(lora_adapter) ? "Yes" : "No");

    swarm_signal_adapter_destroy(lora_adapter);
}

/**
 * @brief Demo: Custom radio callbacks
 */
static bool custom_send_callback(const uint8_t* data, uint32_t len, void* ctx) {
    printf("  [Custom Send] Sending %u bytes\n", len);
    /* In real implementation, would interface with hardware */
    return true;
}

static bool custom_recv_callback(uint8_t* data, uint32_t* len, void* ctx) {
    /* In real implementation, would read from hardware */
    /* For demo, return false (no data) */
    return false;
}

static void demo_custom_radio(void) {
    printf("\n=== Custom Radio Demo ===\n");

    swarm_signal_config_t custom_config = {
        .radio_type = SWARM_RADIO_CUSTOM,
        .frequency_hz = 433000000,  /* 433 MHz */
        .bandwidth_hz = 250000,
        .tx_power_dbm = 10,
        .max_packet_size = 128,
        .retry_count = 3,
        .timeout_ms = 1000,
        .custom_send = custom_send_callback,
        .custom_recv = custom_recv_callback,
        .custom_ctx = NULL
    };

    nimcp_swarm_signal_adapter_t* custom_adapter =
        swarm_signal_adapter_create(&custom_config);

    if (!custom_adapter) {
        printf("Failed to create custom adapter\n");
        return;
    }

    printf("Created custom radio adapter\n");
    printf("Radio type: %s\n",
           swarm_signal_radio_type_string(custom_config.radio_type));

    /* Test send with custom callback */
    const char* msg = "Test message";
    printf("\nSending via custom radio:\n");
    swarm_signal_send(custom_adapter, (const uint8_t*)msg,
                     strlen(msg) + 1, 12345);

    swarm_signal_adapter_destroy(custom_adapter);
}

/**
 * @brief Demo: Statistics tracking
 */
static void demo_statistics(void) {
    printf("\n=== Statistics Demo ===\n");

    swarm_signal_config_t config = {
        .radio_type = SWARM_RADIO_SIMULATION,
        .frequency_hz = 915000000,
        .bandwidth_hz = 125000,
        .tx_power_dbm = 14,
        .max_packet_size = 64,
        .retry_count = 2,
        .timeout_ms = 1000
    };

    nimcp_swarm_signal_adapter_t* adapter1 = swarm_signal_adapter_create(&config);
    nimcp_swarm_signal_adapter_t* adapter2 = swarm_signal_adapter_create(&config);

    if (!adapter1 || !adapter2) {
        printf("Failed to create adapters\n");
        return;
    }

    /* Send multiple messages */
    printf("Sending 10 messages...\n");
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        swarm_signal_broadcast(adapter1, (const uint8_t*)msg, strlen(msg) + 1);
        usleep(5000);
    }

    /* Receive messages */
    usleep(10000);
    printf("Receiving messages...\n");
    uint8_t recv_buffer[DEMO_MSG_SIZE];
    uint32_t recv_len;
    uint32_t source_id;
    int received_count = 0;

    while (swarm_signal_receive(adapter2, recv_buffer, DEMO_MSG_SIZE,
                                &recv_len, &source_id)) {
        received_count++;
    }

    printf("Received %d messages\n", received_count);

    /* Display detailed statistics */
    swarm_signal_stats_t stats1, stats2;
    swarm_signal_get_stats(adapter1, &stats1);
    swarm_signal_get_stats(adapter2, &stats2);

    printf("\nSender Statistics:\n");
    printf("  Packets sent: %lu\n", stats1.packets_sent);
    printf("  Bytes sent: %lu\n", stats1.bytes_sent);
    printf("  Packets dropped: %lu\n", stats1.packets_dropped);
    printf("  Retransmits: %lu\n", stats1.retransmits);

    printf("\nReceiver Statistics:\n");
    printf("  Packets received: %lu\n", stats2.packets_received);
    printf("  Bytes received: %lu\n", stats2.bytes_received);
    printf("  Avg latency: %.3f ms\n", stats2.avg_latency_ms);
    printf("  Min latency: %.3f ms\n", stats2.min_latency_ms);
    printf("  Max latency: %.3f ms\n", stats2.max_latency_ms);
    printf("  CRC errors: %u\n", stats2.crc_errors);

    swarm_signal_adapter_destroy(adapter1);
    swarm_signal_adapter_destroy(adapter2);
}

int main(int argc, char** argv) {
    printf("NIMCP Swarm Signal Adapter Demo\n");
    printf("================================\n");

    /* Run all demos */
    demo_simulation_radio();
    demo_multi_node_broadcast();
    demo_lora_config();
    demo_custom_radio();
    demo_statistics();

    printf("\n=== All demos completed ===\n");

    return 0;
}

# NIMCP Swarm Signal Adapter

## Overview

The NIMCP Swarm Signal Adapter provides a unified, hardware-agnostic interface for radio and transport layer communication in swarm robotics and IoT applications. It abstracts different physical radio types (LoRa, WiFi, Bluetooth, etc.) behind a consistent API, enabling portable swarm communication code.

## Features

- **Multiple Radio Types**: Support for LoRa, WiFi, Bluetooth, Ultrasonic, Optical, and custom radios
- **Simulation Mode**: Built-in simulation for testing without hardware
- **LoRa Protocol**: Complete LoRa framing with preamble, sync word, CRC validation
- **Statistics Tracking**: Comprehensive TX/RX statistics and latency metrics
- **Broadcast Support**: Efficient broadcast to all nodes in swarm
- **Retry Logic**: Configurable retransmission for reliability
- **Thread-Safe**: Mutex-protected operations for concurrent access
- **Custom Callbacks**: Extensible for proprietary radio implementations

## Architecture

```
┌─────────────────────────────────────┐
│   Swarm Application Layer           │
├─────────────────────────────────────┤
│   Signal Adapter API                │
│   (Unified Interface)               │
├─────────────────────────────────────┤
│  ┌──────┬──────┬──────┬──────────┐  │
│  │ LoRa │ WiFi │  BT  │ Custom   │  │
│  └──────┴──────┴──────┴──────────┘  │
│   Radio Type Implementations        │
├─────────────────────────────────────┤
│   Physical/Simulation Layer         │
└─────────────────────────────────────┘
```

## Radio Types

### SWARM_RADIO_LORA
Long-range, low-power radio optimized for IoT:
- Frequency: Typically 868 MHz (EU) or 915 MHz (US)
- Bandwidth: 125 kHz - 500 kHz
- Max Payload: 255 bytes
- Range: Up to 10+ km (line of sight)
- Power: ~100 mW (20 dBm)
- Protocol: Custom framing with preamble, sync word, CRC

### SWARM_RADIO_WIFI
High bandwidth, medium range:
- Frequency: 2.4 GHz / 5 GHz
- Bandwidth: 20-160 MHz
- Max Payload: MTU-dependent (typically 1500 bytes)
- Range: Up to 100m indoor, 300m outdoor
- Power: ~100-200 mW

### SWARM_RADIO_BLUETOOTH
Short-range mesh networking:
- Frequency: 2.4 GHz
- Bandwidth: 1-2 MHz
- Max Payload: 244 bytes (BLE)
- Range: Up to 100m
- Power: ~10 mW

### SWARM_RADIO_ULTRASONIC
Acoustic/underwater communication:
- Frequency: 20-100 kHz
- Bandwidth: Variable
- Max Payload: Limited by bandwidth
- Range: Meters (air), hundreds of meters (water)
- Power: Low

### SWARM_RADIO_OPTICAL
Line-of-sight optical communication:
- Frequency: Visible/IR light
- Bandwidth: Very high
- Max Payload: Limited by protocol
- Range: Up to kilometers (line of sight)
- Power: Variable

### SWARM_RADIO_SIMULATION
In-memory queue-based simulation:
- For testing without hardware
- Supports multiple virtual nodes
- Realistic latency simulation
- Thread-safe queue operations

### SWARM_RADIO_CUSTOM
User-provided callbacks:
- Full control over send/receive
- Integration with proprietary radios
- Custom framing and protocols

## API Reference

### Creating an Adapter

```c
swarm_signal_config_t config = {
    .radio_type = SWARM_RADIO_LORA,
    .frequency_hz = 915000000,      // 915 MHz
    .bandwidth_hz = 125000,         // 125 kHz
    .tx_power_dbm = 14,             // 14 dBm (~25 mW)
    .max_packet_size = 128,         // Max 128 bytes payload
    .retry_count = 3,               // Retry up to 3 times
    .timeout_ms = 1000              // 1 second timeout
};

nimcp_swarm_signal_adapter_t* adapter =
    swarm_signal_adapter_create(&config);
```

### Sending Messages

```c
// Unicast to specific node
const char* msg = "Hello Node 5!";
bool success = swarm_signal_send(adapter,
                                (const uint8_t*)msg,
                                strlen(msg) + 1,
                                5);  // Destination ID

// Broadcast to all nodes
success = swarm_signal_broadcast(adapter,
                                 (const uint8_t*)msg,
                                 strlen(msg) + 1);
```

### Receiving Messages

```c
uint8_t buffer[256];
uint32_t received_len;
uint32_t source_id;

// Non-blocking receive
if (swarm_signal_receive(adapter, buffer, sizeof(buffer),
                        &received_len, &source_id)) {
    printf("Received %u bytes from node %u\n",
           received_len, source_id);
}

// Blocking receive with timeout
if (swarm_signal_receive_blocking(adapter, buffer, sizeof(buffer),
                                  &received_len, &source_id, 2000)) {
    printf("Received within 2 seconds\n");
}
```

### Statistics

```c
swarm_signal_stats_t stats;
swarm_signal_get_stats(adapter, &stats);

printf("Packets sent: %lu\n", stats.packets_sent);
printf("Packets received: %lu\n", stats.packets_received);
printf("Packets dropped: %lu\n", stats.packets_dropped);
printf("Avg latency: %.3f ms\n", stats.avg_latency_ms);
printf("CRC errors: %u\n", stats.crc_errors);

// Reset statistics
swarm_signal_reset_stats(adapter);
```

### Power Control

```c
// Set transmit power
swarm_signal_set_tx_power(adapter, 20);  // 20 dBm

// Get current power
int8_t power = swarm_signal_get_tx_power(adapter);
```

### Custom Radio Implementation

```c
bool my_send(const uint8_t* data, uint32_t len, void* ctx) {
    // Interface with your radio hardware
    // Return true on success
    return my_radio_transmit(data, len);
}

bool my_recv(uint8_t* data, uint32_t* len, void* ctx) {
    // Interface with your radio hardware
    // Return true if data available
    return my_radio_receive(data, len);
}

swarm_signal_config_t config = {
    .radio_type = SWARM_RADIO_CUSTOM,
    .max_packet_size = 128,
    .custom_send = my_send,
    .custom_recv = my_recv,
    .custom_ctx = my_radio_context
};
```

## LoRa Protocol Details

### Packet Structure

```
┌──────────┬───────────┬────────┬─────────┬─────┐
│ Preamble │ Sync Word │ Header │ Payload │ CRC │
└──────────┴───────────┴────────┴─────────┴─────┘
  8 bytes     1 byte     8 bytes  N bytes  2 bytes
```

### Preamble
- 8 bytes of 0xAA
- Used for synchronization

### Sync Word
- 1 byte: 0x34
- Distinguishes network

### Header
- Source ID (4 bytes)
- Destination ID (4 bytes)
- Sequence number (4 bytes)
- Payload length (2 bytes)
- CRC placeholder (2 bytes)

### Payload
- Application data
- Max 255 bytes

### CRC
- CRC16 checksum
- Covers header + payload
- Polynomial: 0x1021

## Performance Characteristics

### LoRa
- Typical latency: 100-500 ms
- Packet error rate: <1% (good conditions)
- Air time: ~50-300 ms (depends on spreading factor)

### WiFi
- Typical latency: 1-10 ms
- Packet error rate: <0.1%
- Throughput: Up to 150 Mbps

### Bluetooth
- Typical latency: 10-100 ms
- Packet error rate: <1%
- Throughput: Up to 2 Mbps

### Simulation
- Latency: <1 ms (in-memory)
- No packet loss
- Unlimited throughput

## Thread Safety

All public API functions are thread-safe:
- Statistics protected by mutex
- Simulation queues have per-queue mutexes
- Safe for concurrent send/receive operations

## Memory Management

- Uses NIMCP memory allocator (`nimcp_malloc`/`nimcp_free`)
- Automatic cleanup on adapter destruction
- Simulation queues limited to 256 packets per node
- Max 64 simulation nodes globally

## Error Handling

Functions return `bool` for success/failure:
- `true`: Operation successful
- `false`: Operation failed (check logs)

NULL checks on all pointer parameters.

## Integration Example

```c
#include "swarm/nimcp_swarm_signal.h"

void swarm_communication_loop(void) {
    // Create adapter
    swarm_signal_config_t config = {
        .radio_type = SWARM_RADIO_LORA,
        .frequency_hz = 915000000,
        .bandwidth_hz = 125000,
        .tx_power_dbm = 14,
        .max_packet_size = 128,
        .retry_count = 3,
        .timeout_ms = 1000
    };

    nimcp_swarm_signal_adapter_t* adapter =
        swarm_signal_adapter_create(&config);

    // Main loop
    while (running) {
        // Send status
        send_status_update(adapter);

        // Receive commands
        uint8_t buffer[256];
        uint32_t len;
        uint32_t source;

        if (swarm_signal_receive(adapter, buffer, sizeof(buffer),
                                &len, &source)) {
            process_command(buffer, len, source);
        }

        sleep(1);
    }

    // Cleanup
    swarm_signal_adapter_destroy(adapter);
}
```

## Testing

See `examples/swarm_signal_demo.c` for comprehensive examples:
- Simulation mode testing
- Multi-node broadcast
- LoRa configuration
- Custom radio callbacks
- Statistics tracking

## Future Enhancements

- [ ] Implement WiFi radio backend
- [ ] Implement Bluetooth mesh backend
- [ ] Add frequency hopping for LoRa
- [ ] Implement adaptive data rate
- [ ] Add encryption layer
- [ ] Support for time synchronization
- [ ] Network discovery protocol
- [ ] Multi-hop routing

## References

- LoRa: Semtech SX127x datasheet
- WiFi: IEEE 802.11 standards
- Bluetooth: Bluetooth SIG specifications
- NIMCP coding standards: `docs/REFACTORING_SUMMARY.md`

## Authors

NIMCP Development Team, 2025

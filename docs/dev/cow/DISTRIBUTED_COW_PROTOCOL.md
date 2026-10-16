# Distributed Copy-on-Write (COW) Network Protocol

## Overview

The Distributed COW protocol extends NIMCP's local Copy-on-Write brain cloning to work across network nodes, enabling efficient distributed deployment of neural models with minimal bandwidth and memory overhead.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    MASTER NODE                            │
│  ┌──────────────────────────────────────────────┐        │
│  │  Original Brain (Full Network)               │        │
│  │  - Complete network weights                  │        │
│  │  - Reference count tracking                  │        │
│  │  - Serves network segments on demand         │        │
│  └──────────────────────────────────────────────┘        │
│           │                                               │
│           │ P2P Network (NIMCP Protocol)                 │
└───────────┼───────────────────────────────────────────────┘
            │
  ┌─────────┴─────────┬──────────────┬──────────────┐
  │                   │              │              │
  ▼                   ▼              ▼              ▼
┌───────┐           ┌───────┐      ┌───────┐      ┌───────┐
│REMOTE │           │REMOTE │      │REMOTE │      │REMOTE │
│NODE 1 │           │NODE 2 │      │NODE 3 │      │NODE N │
│       │           │       │      │       │      │       │
│COW    │           │COW    │      │COW    │      │COW    │
│Clone  │           │Clone  │      │Clone  │      │Clone  │
│       │           │       │      │       │      │       │
│Cache  │           │Cache  │      │Cache  │      │Cache  │
└───────┘           └───────┘      └───────┘      └───────┘
```

## Protocol Messages

### 1. Create Clone Request (CTRL_MSG_COW_CREATE_CLONE = 0x20)

**Purpose:** Initialize a new distributed COW clone on a remote node.

**Request Payload:**
```c
typedef struct {
    uint64_t brain_id;          // Identifier of brain to clone
    uint64_t requested_clone_id; // Requested clone ID (0 = auto-assign)
    uint32_t cache_capacity_mb; // Cache size in MB
    bool enable_compression;    // Enable network compression
    bool enable_prefetch;       // Enable predictive prefetching
} cow_create_clone_request_t;
```

**Response Payload:**
```c
typedef struct {
    uint64_t clone_id;          // Assigned clone ID
    uint64_t brain_id;          // Brain identifier
    uint32_t total_neurons;     // Total neurons in network
    uint32_t total_synapses;    // Total synapses in network
    uint32_t segment_size;      // Recommended segment size
    bool success;               // Operation successful
} cow_create_clone_response_t;
```

**Flow:**
1. Remote node sends create clone request to master
2. Master increments remote reference count
3. Master assigns unique clone ID
4. Master sends response with network metadata
5. Remote node initializes local cache

---

### 2. Fetch Segment Request (CTRL_MSG_COW_FETCH_SEGMENT = 0x21)

**Purpose:** Fetch a segment of network data (neurons + synapses) from master.

**Request Payload:**
```c
typedef struct {
    uint64_t brain_id;          // Brain identifier
    uint64_t clone_id;          // Clone identifier
    uint32_t start_neuron_id;   // First neuron to fetch
    uint32_t num_neurons;       // Number of neurons
    bool enable_compression;    // Compress response?
    uint8_t reserved[3];        // Alignment
} cow_fetch_segment_request_t;
```

**Response Payload:**
```c
typedef struct {
    uint64_t segment_id;        // Segment identifier
    uint32_t start_neuron_id;   // First neuron in segment
    uint32_t num_neurons;       // Number of neurons
    uint32_t num_synapses;      // Number of synapses
    bool is_compressed;         // Is data compressed?
    uint8_t reserved[3];        // Alignment
    uint32_t data_length;       // Length of following data
    // Followed by serialized network data (neurons + synapses)
} cow_segment_data_response_t;
```

**Network Data Format:**
```
[Header: 8 bytes]
  - start_neuron_id: 4 bytes
  - num_neurons: 4 bytes

[For each neuron:]
  - activation: 4 bytes (float)
  - membrane_potential: 4 bytes (float)
  - num_synapses: 4 bytes (uint32_t)

  [For each synapse:]
    - target_neuron_id: 4 bytes (uint32_t)
    - weight: 4 bytes (float)
```

**Compression:**
- Uses zlib (deflate) when enabled
- Compression threshold: Only compress if data > 1KB
- Compression flag in first byte of response data
- Typical compression ratio: ~70% reduction

**Flow:**
1. Remote node checks local cache
2. If cache miss, send fetch segment request
3. Master serializes requested neurons and synapses
4. Master compresses data (if enabled and beneficial)
5. Master sends segment data response
6. Remote node decompresses (if compressed)
7. Remote node deserializes and caches segment
8. Remote node updates cache statistics

---

### 3. Reference Count Update (CTRL_MSG_COW_REFCOUNT_INC/DEC = 0x22/0x23)

**Purpose:** Synchronize reference counts across nodes.

**Payload:**
```c
typedef struct {
    uint64_t brain_id;          // Brain identifier
    uint64_t clone_id;          // Clone identifier
    int32_t delta;              // Refcount change (+1 or -1)
} cow_refcount_msg_t;
```

**Flow:**
- **Increment (0x22):** Sent when new clone created
- **Decrement (0x23):** Sent when clone destroyed
- Master maintains total remote reference count
- When remote refcount reaches 0, master can free network

---

### 4. Full Network Fetch (CTRL_MSG_COW_FULL_FETCH = 0x24)

**Purpose:** Fetch entire network for write operations (learning, fine-tuning).

**Request Payload:**
```c
typedef struct {
    uint64_t brain_id;          // Brain identifier
    uint64_t clone_id;          // Clone identifier
    bool enable_compression;    // Compress response?
} cow_full_fetch_request_t;
```

**Response:** Series of CTRL_MSG_COW_SEGMENT_DATA messages covering entire network.

**Flow:**
1. Remote node sends full fetch request
2. Master sends all segments sequentially
3. Remote node caches all segments
4. Remote node creates local writable network copy
5. Remote node transitions from distributed to local COW
6. Remote node sends CTRL_MSG_COW_REFCOUNT_DEC

---

### 5. Error Response (CTRL_MSG_COW_ERROR = 0x26)

**Purpose:** Report errors during distributed COW operations.

**Payload:**
```c
typedef struct {
    uint64_t brain_id;          // Brain identifier
    uint64_t clone_id;          // Clone identifier
    uint32_t error_code;        // Error code
    char error_message[256];    // Human-readable error
} cow_error_response_t;
```

**Error Codes:**
- `0x01`: Brain not found
- `0x02`: Clone not found
- `0x03`: Segment out of bounds
- `0x04`: Compression failed
- `0x05`: Insufficient memory
- `0x06`: Network timeout
- `0x07`: Protocol version mismatch

---

## Performance Characteristics

### Memory Savings

**Without Distributed COW:**
```
N nodes × M bytes = Total memory
Example: 10 nodes × 50MB = 500MB
```

**With Distributed COW:**
```
M bytes (master) + N × C bytes (caches) = Total memory
Example: 50MB + 10 × 7MB = 120MB
Savings: 76% reduction
```

### Network Bandwidth

**Lazy Loading:**
- Only fetch segments needed for inference
- Typical inference: 10-20% of network accessed
- Bandwidth: ~5-10MB per inference (first time)
- Bandwidth: ~0MB per inference (cached)

**Compression:**
- Reduces network transfer by ~70%
- Example: 10MB uncompressed → 3MB compressed
- Trade-off: ~2-5ms compression overhead

### Latency

**Fetch Segment:**
- Network RTT: 1-10ms (LAN) or 10-100ms (WAN)
- Serialization: 1-5ms
- Compression: 2-10ms (if enabled)
- Deserialization: 1-5ms
- **Total: 5-50ms per segment**

**Cache Hit:**
- O(1) lookup in hash table
- **Total: <0.1ms**

**Prefetching:**
- Reduces latency by fetching ahead
- Background thread fetches adjacent segments
- Typical prefetch lookahead: 2K neurons

---

## Security Considerations

### Authentication
- P2P nodes use mutual TLS authentication
- Clone ID includes cryptographic signature
- Master validates all requests

### Encryption
- All network traffic encrypted (TLS 1.3)
- Network weights can be encrypted at rest
- See `nimcp_encryption.h` for key management

### Authorization
- Master maintains ACL for allowed clone nodes
- Rate limiting prevents DoS attacks
- Segment fetch quota per clone

---

## Example Usage

### Master Node Setup

```c
// Create master brain
brain_t master = brain_create("model", BRAIN_SIZE_MEDIUM,
                              BRAIN_TASK_CLASSIFICATION, 256, 10);

// Create P2P node
node_config_t config = {
    .listen_port = 5000,
    .max_peers = 10,
    .ping_interval = 5000
};
p2p_node_t p2p_node = p2p_node_create(&config);
p2p_node_start(p2p_node);

// Enable distributed COW serving
brain_enable_distributed_cow_master(master, p2p_node);

printf("Master ready on port 5000\n");
```

### Remote Node Setup

```c
// Create distributed COW clone
distributed_cow_config_t config = distributed_cow_default_config();
config.cache_capacity_mb = 10;  // 10MB cache
config.enable_compression = true;
config.enable_prefetch = true;

brain_t clone = brain_clone_cow_distributed(
    NULL,                    // Original (not needed on remote)
    "192.168.1.100",         // Master host
    5000,                    // Master port
    &config
);

// Use clone for inference
float input[256] = {/* ... */};
brain_decision_t* decision = brain_decide(clone, input, 256);

// Network segments fetched automatically on demand
```

### Performance Monitoring

```c
distributed_cow_stats_t stats;
brain_get_distributed_cow_stats(clone, &stats);

printf("Cache size: %.2f MB\n", stats.cache_size_bytes / (1024.0 * 1024.0));
printf("Cache hit rate: %.2f%%\n", stats.cache_hit_rate * 100.0);
printf("Avg fetch latency: %.2f ms\n", stats.avg_fetch_latency_ms);
printf("Network bandwidth: %.2f Mbps\n", stats.network_bandwidth_mbps);
```

---

## Implementation Notes

### Cache Eviction Policy

**LRU (Least Recently Used):**
- Segments tracked with access timestamps
- Evict oldest when cache full
- Typical cache size: 10MB (holds ~5-10% of network)

### Prefetching Strategy

**Predictive Prefetching:**
- Analyzes recent access patterns
- Fetches adjacent segments likely to be needed
- Configurable lookahead (default: 2K neurons)
- Runs in background thread

### Compression Trade-offs

**When to Compress:**
- Segment size > 1KB
- Network bandwidth limited
- CPU cycles available

**When NOT to Compress:**
- Segment size < 1KB (overhead not worth it)
- Low-latency requirements (<5ms)
- CPU-constrained environment

---

## Testing

### Unit Tests
- Segment serialization/deserialization
- Compression/decompression round-trip
- Cache hit/miss logic
- Reference counting

### Integration Tests
- Master-clone handshake
- Lazy segment fetching
- Full network fetch
- Multi-clone coordination

### Performance Tests
- Bandwidth measurement
- Latency profiling
- Cache efficiency
- Compression ratio

---

## Future Enhancements

### Planned Features

1. **Hierarchical Caching**
   - Multi-level cache (L1/L2)
   - Cluster-wide distributed cache
   - Cache coherency protocol

2. **Delta Compression**
   - Only send changed weights
   - Reduces bandwidth for fine-tuning
   - Version tracking

3. **Peer-to-Peer Segment Sharing**
   - Clones can fetch from each other
   - Reduces master load
   - BitTorrent-style distribution

4. **Adaptive Segment Sizing**
   - Dynamic segment size based on access patterns
   - Smaller segments for sparse access
   - Larger segments for dense access

5. **Smart Prefetching**
   - ML-based access prediction
   - Learns inference patterns
   - Proactive fetching

---

## References

- NIMCP Protocol Specification: `nimcp_protocol.h`
- P2P Networking: `nimcp_p2pnode.h`
- Local COW Implementation: `nimcp_brain.c` (Phase 2)
- Distributed Cognition: `nimcp_distributed_cognition.h` (Phase 3)

---

**Version:** NIMCP 2.8.0
**Author:** NIMCP Development Team
**Date:** 2025-11-09

# Pass 6 Walkthrough: Networking Protocol, P2P, Events, Replication

**Date**: 2026-02-15
**Scope**: `src/networking/protocol/`, `src/networking/p2p/`, `src/networking/events/`, `src/networking/replication/`
**Files reviewed**: 6 files, ~5,700 lines total

## Summary

- **P1 (crashes/deadlocks/overflows)**: 1
- **P2 (wrong error codes, false positive throws, leaks, races)**: 37

## Issues

| # | File | Line | Pri | Issue |
|---|------|------|-----|-------|
| 1 | `src/networking/p2p/nimcp_p2pnode.c` | 1837 | P1 | **DEADLOCK**: `attempt_peer_reconnect()` calls `p2p_node_connect_peer()` which calls `nimcp_mutex_lock(&node->lock)` at line 1094, but it is called from `p2p_node_reconnect_unhealthy()` which already holds `node->lock` (acquired at line 1879). Mutex is NORMAL type (NULL attr at line 691), so this is a guaranteed deadlock. Need to either make the mutex recursive or extract an unlocked `_connect_peer_unlocked()` helper. |
| 2 | `src/networking/protocol/nimcp_msg_framing.c` | 69 | P2 | **False positive throw**: `NIMCP_THROW_TO_IMMUNE` on invalid magic bytes in `nimcp_msg_header_validate` - this is validation of untrusted network input, not an immune-level error. High-volume on malformed data. |
| 3 | `src/networking/protocol/nimcp_msg_framing.c` | 341 | P2 | **Wrong error message**: `"nimcp_msg_header_validate is NULL"` - the function returned false, it is not NULL. Should say `"header validation failed"`. |
| 4 | `src/networking/protocol/nimcp_msg_framing.c` | 387 | P2 | **Wrong error message**: `"nimcp_msg_is_fast_path is NULL"` - the function returned false, not NULL. Should say `"message is not fast path"`. |
| 5 | `src/networking/protocol/nimcp_msg_router.c` | 241 | P2 | **Wrong error message**: `"is_fast_path_type is NULL"` - should say `"msg_type is not a fast path type"`. |
| 6 | `src/networking/protocol/nimcp_msg_router.c` | 295 | P2 | **False positive throw**: `NIMCP_THROW_TO_IMMUNE` in `nimcp_msg_router_unregister` when handler not found - this is a normal lookup miss, not an immune-level error. |
| 7 | `src/networking/protocol/nimcp_msg_router.c` | 425 | P2 | **False positive throw**: `NIMCP_THROW_TO_IMMUNE` when no handler found for fast message type - unhandled message types are normal in routing, not errors. |
| 8 | `src/networking/protocol/nimcp_msg_router.c` | 500 | P2 | **False positive throw**: Same as #7 but in `nimcp_msg_router_route_parsed` - unhandled message type is normal. |
| 9 | `src/networking/protocol/nimcp_msg_router.c` | 523 | P2 | **False positive throw**: `NIMCP_THROW_TO_IMMUNE` on queue overflow - this is intentional backpressure behavior, not an immune error. |
| 10 | `src/networking/protocol/nimcp_msg_router.c` | 375-381 | P2 | **Race condition**: Router stats (`messages_routed`, `fast_messages_routed`, `bytes_routed`, `handler_errors`) are accessed without synchronization. The struct has no mutex, and stats are plain `uint64_t` not atomics. |
| 11 | `src/networking/protocol/nimcp_msg_router.c` | 508-547 | P2 | **Race condition**: Queue operations (`queue_head`, `queue_tail`, `queue_count`) in `nimcp_msg_router_queue` and `nimcp_msg_router_process_queue` have no mutex protection. Concurrent enqueue/dequeue will corrupt state. |
| 12 | `src/networking/protocol/nimcp_protocol.c` | 256 | P2 | **Int return truncation**: `return sizeof(msg_header_t) + payload_len` - sum is `size_t` + `uint32_t`, returned as `int`. Safe with current `MAX_PAYLOAD_SIZE=65536` but fragile if max payload ever increases past `INT_MAX`. |
| 13 | `src/networking/protocol/nimcp_protocol.c` | 919 | P2 | **Int return truncation**: Same pattern in `event_packet_serialize` - `return sizeof(event_packet_t) + packet->payload_length` returned as `int`. |
| 14 | `src/networking/protocol/nimcp_protocol.c` | 1060 | P2 | **Int return truncation**: Same pattern in `event_packet_deserialize`. |
| 15 | `src/networking/protocol/nimcp_protocol.c` | 1453 | P2 | **Int return truncation**: `return msg->message_length` in `control_message_deserialize` - `message_length` is `uint32_t`, function returns `int`. Values > `INT_MAX` produce negative return (indistinguishable from error). |
| 16 | `src/networking/protocol/nimcp_protocol.c` | 1600 | P2 | **False positive throw**: `subscription_matches` throws `NIMCP_THROW_TO_IMMUNE` when `feature_code_matches` returns false - this is normal filter non-match behavior, called O(N*M) times during event processing. |
| 17 | `src/networking/protocol/nimcp_protocol.c` | 1607 | P2 | **False positive throw**: `subscription_matches` throws when confidence below threshold - normal filter behavior, called for every event packet. |
| 18 | `src/networking/p2p/nimcp_p2pnode.c` | 64 | P2 | **Race condition**: `g_bbb_system` static global - `p2pnode_security_init()` is not thread-safe. Two threads could race on initialization, creating duplicate BBB systems (one leaked). |
| 19 | `src/networking/p2p/nimcp_p2pnode.c` | 193 | P2 | **False positive throw**: `validate_port_number` throws `NIMCP_THROW_TO_IMMUNE` for port == 0. Zero port is sometimes a valid "any port" request; at minimum this is a validation rejection, not an immune error. |
| 20 | `src/networking/p2p/nimcp_p2pnode.c` | 326 | P2 | **Wrong error code**: `create_tcp_socket` uses `NIMCP_ERROR_INVALID_PARAM` for socket creation failure - should be `NIMCP_ERROR_IO`. |
| 21 | `src/networking/p2p/nimcp_p2pnode.c` | 353 | P2 | **Wrong error code**: `set_socket_nonblocking` uses `NIMCP_ERROR_INVALID_PARAM` for `fcntl` failure - should be `NIMCP_ERROR_IO`. |
| 22 | `src/networking/p2p/nimcp_p2pnode.c` | 374 | P2 | **Wrong error code**: `enable_socket_reuse` uses `NIMCP_ERROR_INVALID_PARAM` for `setsockopt` failure - should be `NIMCP_ERROR_IO`. |
| 23 | `src/networking/p2p/nimcp_p2pnode.c` | 580 | P2 | **Wrong error code**: `allocate_peer_storage` uses `NIMCP_ERROR_NO_MEMORY` when `node` is NULL - should be `NIMCP_ERROR_NULL_POINTER`. |
| 24 | `src/networking/p2p/nimcp_p2pnode.c` | 637-638 | P2 | **Wrong error code**: `p2p_node_create` uses `NIMCP_ERROR_NO_MEMORY` for `validate_config` failure - should be `NIMCP_ERROR_INVALID_PARAM`. |
| 25 | `src/networking/p2p/nimcp_p2pnode.c` | 712 | P2 | **Redundant throw**: Double `NIMCP_THROW_TO_IMMUNE` after `NIMCP_THROW_MEMORY` already thrown for topology graph allocation failure. |
| 26 | `src/networking/p2p/nimcp_p2pnode.c` | 1224 | P2 | **False positive throw**: `find_peer_index` throws `NIMCP_THROW_TO_IMMUNE` when peer not found in array - this is normal lookup behavior. |
| 27 | `src/networking/p2p/nimcp_p2pnode.c` | 1494 | P2 | **Wrong error code**: `p2p_node_stop` uses `NIMCP_ERROR_NULL_POINTER` for "not running" state check - should be `NIMCP_ERROR_INVALID_STATE`. Error message also says `"node->running is NULL"` but it's a bool, not a pointer. |
| 28 | `src/networking/p2p/nimcp_p2pnode.c` | 1531 | P2 | **Wrong error code**: `send_ping_to_peer` uses `NIMCP_ERROR_NULL_POINTER` for disconnected/invalid-socket peer - should be `NIMCP_ERROR_INVALID_STATE`. |
| 29 | `src/networking/p2p/nimcp_p2pnode.c` | 1823 | P2 | **False positive throw**: `attempt_peer_reconnect` throws `NIMCP_THROW_TO_IMMUNE` when peer is healthy - this is a logic guard, not an error condition. |
| 30 | `src/networking/events/nimcp_events.c` | 469 | P2 | **False positive throw**: `event_worker_thread` throws `NIMCP_THROW_TO_IMMUNE` on normal thread exit after shutdown. The throw at the bottom of the function fires every time the worker thread exits normally. |
| 31 | `src/networking/events/nimcp_events.c` | 860 | P2 | **False positive throw**: `event_generator_on_spike` throws when queue is full - this is intentional backpressure (documented as "This is intentional backpressure behavior" in comment at line 858). |
| 32 | `src/networking/events/nimcp_events.c` | 1183 | P2 | **Wrong error code**: `resolve_target_neuron` uses `NIMCP_ERROR_NULL_POINTER` when `auto_create_neurons` is false and no mapping exists - should be `NIMCP_ERROR_NOT_FOUND` or `NIMCP_ERROR_OUT_OF_RANGE`. Error message says `"receiver->auto_create_neurons is NULL"` but it's a bool. |
| 33 | `src/networking/events/nimcp_events.c` | 1253 | P2 | **False positive throw**: `event_receiver_process_packet` throws when subscription filter rejects packet - this is normal filter behavior, not an error. |
| 34 | `src/networking/events/nimcp_events.c` | 339 | P2 | **Wrong error code**: `allocate_feature_storage` uses `NIMCP_ERROR_NO_MEMORY` when `gen` is NULL - should be `NIMCP_ERROR_NULL_POINTER`. |
| 35 | `src/networking/replication/nimcp_replication.c` | 722 | P2 | **False positive throw**: `heartbeat_thread_fn` throws `NIMCP_THROW_TO_IMMUNE` on normal thread exit after heartbeat loop completes. Fires every time heartbeat thread shuts down. |
| 36 | `src/networking/replication/nimcp_replication.c` | 784,797,809 | P2 | **Redundant throws**: Double `NIMCP_THROW_TO_IMMUNE` in error cleanup paths of `replication_create_cluster` - the first throw (e.g., `NIMCP_ERROR_NOT_IMPLEMENTED`) is followed by a second `NIMCP_ERROR_NULL_POINTER` throw before return. |
| 37 | `src/networking/replication/nimcp_replication.c` | 846 | P2 | **Wrong error code**: `replication_create_cluster` uses `NIMCP_ERROR_NULL_POINTER` for thread creation failure - should be `NIMCP_ERROR_THREAD_CREATE`. |
| 38 | `src/networking/replication/nimcp_replication.c` | 886 | P2 | **Race condition**: `cluster->heartbeat_running` is set to `false` at line 886 in `replication_destroy_cluster` without holding `heartbeat_lock`. The heartbeat thread reads this flag at line 708 also without the lock (outside the lock region). Should use atomic or hold lock for write. |

## P1 Detail

### #1: DEADLOCK in `p2p_node_reconnect_unhealthy` -> `attempt_peer_reconnect` -> `p2p_node_connect_peer`

**Call chain**:
1. `p2p_node_reconnect_unhealthy()` acquires `node->lock` at line 1879
2. Calls `attempt_peer_reconnect()` at line 1898
3. `attempt_peer_reconnect()` calls `p2p_node_connect_peer()` at line 1837
4. `p2p_node_connect_peer()` calls `nimcp_mutex_lock(&node->lock)` at line 1094

The mutex is initialized with `nimcp_mutex_init(&node->lock, NULL)` at line 691, which uses default (NORMAL) type. A NORMAL mutex deadlocks on recursive locking.

**Fix**: Create `p2p_node_connect_peer_unlocked()` internal helper that does the actual work without taking the lock. Have `p2p_node_connect_peer()` call it after acquiring the lock, and have `attempt_peer_reconnect()` call the unlocked version directly.

# Pass 6 Review: Mesh Ordering, Channel, Participant, Topology, Bootstrap, Cross-Channel

**Files reviewed:**
- `src/mesh/nimcp_mesh_ordering.c`
- `src/mesh/nimcp_mesh_channel.c`
- `src/mesh/nimcp_mesh_participant.c`
- `src/mesh/nimcp_mesh_topology.c`
- `src/mesh/nimcp_mesh_bootstrap.c`
- `src/mesh/nimcp_mesh_cross_channel.c`

**Date:** 2026-02-15
**Reviewer:** Claude Opus 4.6

---

## Summary

| Severity | Count |
|----------|-------|
| P1 (crash/race/overflow) | 16 |
| P2 (wrong error code/false positive throw/leak) | 16 |
| **Total** | **32** |

---

## Findings

| # | Sev | File | Line | Issue | Description |
|---|-----|------|------|-------|-------------|
| 1 | P1 | nimcp_mesh_ordering.c | 573-579 | Memory leak | `mesh_ordering_sequence_batch`: when Raft log is full (`log_size >= log_capacity`), `entry.tx_ids` allocated at line 565 is never freed -- the stack-local `entry` is silently discarded with its heap-allocated `tx_ids`. |
| 2 | P1 | nimcp_mesh_ordering.c | 644-655 | Memory leak on realloc failure | `mesh_ordering_create_block`: if `nimcp_realloc` for blocks array fails, the newly allocated `block` is returned to the caller but never stored in `service->blocks[]`. The service destructor will not free it, and the block is orphaned if the caller doesn't manually free it (per ownership convention, service owns blocks). |
| 3 | P1 | nimcp_mesh_ordering.c | 294-298 | channel_count can exceed capacity | `mesh_ordering_create`: `service->channel_count = config->channel_count` is set to the raw config value even though the copy loop clamps at `channel_capacity`. If `config->channel_count > MESH_MAX_CHANNELS`, `channel_count` will be larger than the actual number of channels stored, causing OOB reads in `mesh_ordering_has_channel` and similar. |
| 4 | P1 | nimcp_mesh_ordering.c | 474 | Data race | `mesh_ordering_get_pending_count` reads `service->pending_count` without locking the mutex, while `mesh_ordering_submit` and `mesh_ordering_create_batch` modify it under lock. |
| 5 | P1 | nimcp_mesh_ordering.c | 932-948 | Data race on Raft state getters | `mesh_ordering_get_role`, `mesh_ordering_get_term`, `mesh_ordering_get_leader`, `mesh_ordering_is_leader` all read Raft state fields without mutex protection while `mesh_ordering_update`, `mesh_ordering_start_election`, and `mesh_ordering_handle_append_entries` mutate them under lock. |
| 6 | P1 | nimcp_mesh_ordering.c | 1237-1251 | Stale batch state in update loop | `mesh_ordering_update`: `batch_full` and `batch_timeout` booleans are computed at lines 1237-1241 while holding the lock, but the decisions using them at lines 1247-1249 happen AFTER the mutex is unlocked (line 1244) and `mesh_ordering_create_batch` has modified the batch. The batch state may have changed, causing sequence/block creation on stale conditions. |
| 7 | P1 | nimcp_mesh_channel.c | 1153-1158 | Data race | `mesh_channel_get_consensus_beliefs` reads `channel->beliefs` and `belief_count` without locking the mutex, while `mesh_channel_gossip_round` concurrently modifies and compacts the beliefs array. |
| 8 | P1 | nimcp_mesh_channel.c | 698-701 | Data race | `mesh_channel_get_world_state_coherence` calls `collective_workspace_get_coherence` on `channel->world_state` without mutex, while gossip rounds modify the workspace concurrently. |
| 9 | P1 | nimcp_mesh_channel.c | 49-50 | Thread-unsafe globals | `g_mesh_channel_mesh_id` and `g_mesh_channel_mesh_registry` are plain (non-atomic) globals accessed from `mesh_channel_mesh_register` and `mesh_channel_mesh_unregister` without any synchronization. |
| 10 | P1 | nimcp_mesh_participant.c | 161 | False positive throw on lookup path | `find_entry_by_id` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)` on the "not found" path. This fires on EVERY failed ID lookup (used by `mesh_participant_get`, `mesh_participant_unregister`, channel membership checks, etc.). Not-found is normal behavior, not an error. This generates spurious immune events on routine operations. |
| 11 | P1 | nimcp_mesh_participant.c | 183 | False positive throw on lookup path | `find_entry_by_name` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)` on the not-found path. Same issue as #10 -- fires on every name-not-found lookup. Used by `mesh_participant_register` to check for duplicates, meaning EVERY successful registration triggers a throw. |
| 12 | P1 | nimcp_mesh_topology.c | 575-593 | Data race - no mutex | `mesh_topology_clear` modifies `node_count`, `connection_count`, `hub_count`, `num_clusters`, `metrics_valid`, and frees adjacency lists without locking the mutex. Concurrent calls to `add_participant`, `add_connection`, etc. will race. |
| 13 | P1 | nimcp_mesh_topology.c | 599-689 | Data race - no mutex | `mesh_topology_compute_metrics` reads and modifies topology state (iterates over nodes, modifies `is_hub` flags via `identify_hubs`, sets `cached_metrics`, `metrics_valid`) without locking the mutex. |
| 14 | P1 | nimcp_mesh_topology.c | 562-563 | Unsigned underflow | `mesh_topology_remove_participant`: `ctx->connection_count -= node->degree` can underflow. `node->degree` counts outgoing edges from this node, but `remove_neighbor` at line 558 also removes incoming edges from other nodes (decrementing their degrees). The total connection_count may not have accounted for all of these edges symmetrically, leading to underflow of the unsigned `size_t`. |
| 15 | P1 | nimcp_mesh_cross_channel.c | 595-596 | False positive throw on non-conflict path | `mesh_cross_transactions_conflict` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "operation failed")` on the NORMAL return path when transactions do NOT conflict. Every non-conflicting pair of transactions triggers a spurious immune event. |
| 16 | P1 | nimcp_mesh_cross_channel.c | 53-91 | No thread safety | Neither `mesh_system_coordinator_internal` nor `mesh_cross_router_internal` have mutex protection. All state mutations (stats, channel arrays, pending lists) are unprotected against concurrent access. |
| 17 | P2 | nimcp_mesh_ordering.c | 592 | Wrong error code | `mesh_ordering_create_block`: NULL service guard uses `NIMCP_ERROR_NO_MEMORY` (should be `NIMCP_ERROR_NULL_POINTER` or `NIMCP_ERROR_INVALID_PARAM`). |
| 18 | P2 | nimcp_mesh_ordering.c | 600 | Wrong error code + wrong message | `mesh_ordering_create_block`: empty batch (count==0) uses `NIMCP_ERROR_NULL_POINTER` with message "batch is NULL". The batch pointer is valid but empty. Should be `NIMCP_ERROR_INVALID_STATE` with "batch is empty". |
| 19 | P2 | nimcp_mesh_ordering.c | 1022-1025 | Wrong message | `mesh_ordering_log_get`: conflates NULL service and out-of-bounds index into a single guard with message "service is NULL" even when the actual issue is `index >= log_size`. |
| 20 | P2 | nimcp_mesh_participant.c | 201-202 | Wrong error code + wrong message + false positive throw | `find_free_slot`: when registry is full (no free slot), throws `NIMCP_ERROR_NULL_POINTER` with message "registry->entries is NULL". The entries pointer is valid -- the issue is capacity exhaustion. Should not throw at all (caller handles NULL return), and if it did, should use `NIMCP_ERROR_CAPACITY_EXCEEDED`. |
| 21 | P2 | nimcp_mesh_participant.c | 711-712 | False positive throw | `mesh_participant_get_credential`: throws `NIMCP_THROW_TO_IMMUNE` when participant has no credential. "No credential set" is a valid state, not an error worth immune notification. |
| 22 | P2 | nimcp_mesh_participant.c | 733-734 | False positive throw | `mesh_participant_validate_credential`: same as #21 -- throws when credential not found. Callers using this to CHECK validity will trigger immune events on every uncredentialed participant. |
| 23 | P2 | nimcp_mesh_channel.c | 430 | Wrong error code | `mesh_channel_create`: module_wirings allocation failure uses `NIMCP_ERROR_NULL_POINTER` (should be `NIMCP_ERROR_NO_MEMORY`). |
| 24 | P2 | nimcp_mesh_channel.c | 444 | Wrong error code | `mesh_channel_create`: mutex creation failure uses `NIMCP_ERROR_NULL_POINTER` (should be `NIMCP_ERROR_NO_MEMORY`). |
| 25 | P2 | nimcp_mesh_channel.c | 1338 | Wrong error code | `mesh_channel_manager_create_channel`: parameter validation failure uses `NIMCP_ERROR_NO_MEMORY` (should be `NIMCP_ERROR_INVALID_PARAM` or `NIMCP_ERROR_NULL_POINTER`). |
| 26 | P2 | nimcp_mesh_topology.c | 237-239 | False positive throw for empty topology | `find_node`: throws `NIMCP_ERROR_INVALID_PARAM` when `node_count == 0`. An empty topology is a valid state; querying a node in an empty topology should just return NULL without an immune event. |
| 27 | P2 | nimcp_mesh_topology.c | 691-711 | Missing mutex | `mesh_topology_get_node_info` reads node data without mutex, inconsistent with `add_participant`/`add_connection` which lock. P2 because a stale read is unlikely to crash but returns wrong data. |
| 28 | P2 | nimcp_mesh_cross_channel.c | 97-101 | Platform-specific time function | `get_time_ns()` uses raw `clock_gettime(CLOCK_MONOTONIC, ...)` instead of `nimcp_time_now_ns()`. Inconsistent with rest of codebase and not portable to non-POSIX platforms. |
| 29 | P2 | nimcp_mesh_cross_channel.c | 220 | False positive throw on unregister-not-found | `mesh_system_coord_unregister_channel` throws `NIMCP_THROW_TO_IMMUNE` when channel not found. Attempting to unregister a non-existent channel is a benign idempotency case, not worthy of immune notification. |
| 30 | P2 | nimcp_mesh_cross_channel.c | 632 | False positive throw on health lookup | `mesh_system_coord_channel_healthy` throws `NIMCP_THROW_TO_IMMUNE` when queried channel is not registered. A health query for an unknown channel should simply return false, not fire an immune event. |
| 31 | P2 | nimcp_mesh_bootstrap.c | 127-155 | Potential adapter leak | `register_generic_module` allocates `mesh_adapter_base_t* base` on heap. On success, base is passed to `mesh_integration_register_adapter`. If integration does NOT take ownership (store and later free), this leaks. Ownership semantics unclear from this file alone. |
| 32 | P2 | nimcp_mesh_ordering.c | 1032-1043 | Data race on log accessors | `mesh_ordering_log_last_index` and `mesh_ordering_log_last_term` read `service->raft.log_size` and `service->raft.log[]` without mutex, while log append/truncate/compact modify these under lock. |

---

## Top Priority Fixes

### Critical (fix first)
1. **#10, #11 (participant.c false positive throws)**: These fire on EVERY registration and EVERY lookup miss -- extremely high frequency immune noise. `find_entry_by_id` and `find_entry_by_name` should just return NULL without throwing on not-found.

2. **#15 (cross_channel.c false positive on non-conflict path)**: `mesh_cross_transactions_conflict` throws on the normal non-conflict return path. Remove the throw at line 595-596.

3. **#1 (ordering.c log-full leak)**: `entry.tx_ids` leaks when Raft log is full. Add `nimcp_free(entry.tx_ids)` before the early-out or grow the log.

4. **#3 (ordering.c channel_count overflow)**: Clamp `service->channel_count` to match the actual number of channels copied.

### High (fix soon)
5. **#12, #13 (topology.c missing mutex)**: `mesh_topology_clear` and `mesh_topology_compute_metrics` need mutex protection to match the pattern used by `add_participant`/`add_connection`.

6. **#7, #8 (channel.c data races in getters)**: `get_consensus_beliefs` and `get_world_state_coherence` need mutex protection.

7. **#20 (participant.c find_free_slot wrong throw)**: Remove the throw and fix the error code.

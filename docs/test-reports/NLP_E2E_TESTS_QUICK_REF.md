# NLP E2E Tests Quick Reference

## Test Files Created

### File Summary
```
e2e_test_nlp_swarm_coordination.cpp    648 lines   5 tests   Swarm Formation & Coordination
e2e_test_nlp_war_zone.cpp              645 lines   5 tests   Hostile Environment Operations
e2e_test_nlp_disaster_sar.cpp          727 lines   5 tests   Search & Rescue Scenarios
e2e_test_nlp_neural_distributed.cpp    793 lines   5 tests   Distributed Neural Processing
                                      ──────────────────────────────────────────────────────
                                       2,813 lines  20 tests   Total
```

## Test Matrix

| Suite | Test Name | What It Tests | Duration | Port Range |
|-------|-----------|---------------|----------|------------|
| **Swarm Coordination** | | | | **19000-19007** |
| | SwarmFormation | 8 nodes form mesh network from scratch | 10s | |
| | MasterElection | Leader election on master failure | 5s | |
| | SplitBrainRecovery | Network partition healing | 10s | |
| | RoleAssignment | Master assigns roles to sub-brains | 5s | |
| | ConsensusDecision | Byzantine-fault-tolerant voting | 5s | |
| **War Zone** | | | | **19100-19107** |
| | JammingResilience | 70% packet loss under RF jamming | 15s | |
| | NodeDestruction | Swarm continues after 50% node loss | 15s | |
| | TacticalModeOps | Masterless tactical operations | 15s | |
| | StealthInfiltration | EMCON emission control modes | 20s | |
| | EmergencyBreakGlass | Critical msg in SILENT mode | 20s | |
| **Disaster SAR** | | | | **19200-19207** |
| | SearchPattern | Coordinated 100m² search grid | 10s | |
| | VictimDiscovery | Victim triage and aggregation | 10s | |
| | HazardAvoidance | Swarm reroutes from hazards | 10s | |
| | ResourceDepletion | Low battery behavior change | 10s | |
| | CommunicationsBlackout | Store-and-forward recovery | 10s | |
| **Neural Distributed** | | | | **19300-19307** |
| | DistributedLearning | Weight delta propagation | 10s | |
| | SpikeCoordination | Sub-ms spike synchronization | 10s | |
| | StateMigration | 64KB state transfer on failover | 10s | |
| | GradientAggregation | Federated learning | 10s | |
| | CognitiveConsensus | Perception agreement | 10s | |

## Key Features by Test

### Swarm Coordination
- ✅ Full mesh networking (n² connections)
- ✅ Master/sub-brain architecture
- ✅ Byzantine consensus protocol
- ✅ Dynamic role assignment
- ✅ Split-brain detection and healing

### War Zone
- ✅ Configurable RF jamming (70% default loss)
- ✅ Node destruction simulation
- ✅ Pre-shared key tactical mode
- ✅ 5-level EMCON (emissions control)
- ✅ Emergency message override

### Disaster SAR
- ✅ GPS coordinate tracking
- ✅ 5-level triage system (red/yellow/green/black/unknown)
- ✅ Environmental sensors (CO, CO2, radiation, etc.)
- ✅ Hazard zone avoidance (20m radius)
- ✅ Message buffering during blackout

### Neural Distributed
- ✅ 1,000 neurons × 8 nodes = 8,000 total
- ✅ 5,000 synapses per node
- ✅ Microsecond spike timing precision
- ✅ 64KB+ state migration
- ✅ Gradient-based federated learning

## NLP Message Types Used

### Session Management
- `NLP_MSG_HANDSHAKE_REQ/RESP/FINAL` - Session establishment
- `NLP_MSG_DISCONNECT` - Graceful disconnect

### Neural Synchronization
- `NLP_MSG_SPIKE_BATCH` - Neural spike events
- `NLP_MSG_WEIGHT_DELTA` - Synaptic weight changes
- `NLP_MSG_STATE_SYNC` - Full brain state
- `NLP_MSG_GRADIENT_PUSH` - Learning gradients

### Swarm Coordination
- `NLP_MSG_HEARTBEAT` - Keepalive
- `NLP_MSG_PEER_ANNOUNCE` - Presence announcement
- `NLP_MSG_MASTER_ELECTION` - Leader election
- `NLP_MSG_CONSENSUS_VOTE` - Byzantine voting
- `NLP_MSG_ROLE_ASSIGN` - Role assignment

### Tactical/Emergency
- `NLP_MSG_PRIORITY_CMD` - High-priority command
- `NLP_MSG_EMERGENCY` - Break-glass critical
- `NLP_MSG_RELAY` - Mesh relay

### Stealth Mode
- `NLP_MSG_BURST_SYNC` - Compressed burst
- `NLP_MSG_EMCON_CHANGE` - Change EMCON level

### Disaster/SAR
- `NLP_MSG_LOCATION_UPDATE` - GPS position
- `NLP_MSG_VICTIM_REPORT` - SAR victim data
- `NLP_MSG_HAZARD_ALERT` - Hazard warning
- `NLP_MSG_RESOURCE_STATUS` - Battery/fuel status

## Pre-Shared Keys

Each test suite uses unique PSK for isolation:
```c
Swarm Coordination:  0x42 + (i % 16)  // Key ID: 1
War Zone:           0x5A + (i % 16)  // Key ID: 1
Disaster SAR:       0x73 + (i % 16)  // Key ID: 100
Neural Distributed: 0x6E + (i % 16)  // Key ID: 200
```

## Running Tests

### Build Individual Test
```bash
cd /home/bbrelin/nimcp/build
make e2e_test_nlp_swarm_coordination
make e2e_test_nlp_war_zone
make e2e_test_nlp_disaster_sar
make e2e_test_nlp_neural_distributed
```

### Run All NLP Tests
```bash
ctest -L e2e -R nlp -V
```

### Run Specific Test Case
```bash
./test/e2e/e2e_test_nlp_swarm_coordination --gtest_filter="*SwarmFormation*"
```

### Debug Single Test
```bash
gdb --args ./test/e2e/e2e_test_nlp_war_zone --gtest_filter="*JammingResilience*"
```

## Expected Output

### Successful Test
```
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from NLPSwarmCoordinationTest
[ RUN      ] NLPSwarmCoordinationTest.SwarmFormation

Pipeline: NLP Swarm Formation from Scratch
  Stage: Connect Mesh Network (2000ms) - 145ms ✓
  Stage: Wait for Network Convergence (10000ms) - 3421ms ✓
  Stage: Verify All Nodes Connected (1000ms) - 12ms ✓
  Stage: Verify Heartbeat Exchange (3000ms) - 2003ms ✓
  Stage: Verify Peer Discovery (1000ms) - 8ms ✓
Total: 5589ms

[       OK ] NLPSwarmCoordinationTest.SwarmFormation (5590 ms)
[----------] 1 test from NLPSwarmCoordinationTest (5590 ms total)
[==========] 1 test from 1 test suite ran. (5590 ms total)
[  PASSED  ] 1 test.
```

## Test Verification Checklist

For each test file:
- ✅ Includes e2e_test_framework.h
- ✅ Uses Google Test (TEST_F macros)
- ✅ Has PipelineTracker for stages
- ✅ Includes realistic timeouts
- ✅ Tests actual behavior, not just "no crash"
- ✅ Uses localhost with unique ports
- ✅ Has proper cleanup in TearDown()
- ✅ Verifies message counts, latencies, convergence
- ✅ Tests failure recovery scenarios
- ✅ Includes timing-sensitive tests with tolerances

## Dependencies

### Headers Required
```c
#include "e2e_test_framework.h"
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
```

### System Libraries
```cmake
nimcp (full library)
GTest::GTest
GTest::Main
Python3::Python
pthread
m
```

## Test Isolation

### Port Ranges (Non-Overlapping)
- Swarm Coordination: 19000-19007 (8 ports)
- War Zone: 19100-19107 (8 ports)
- Disaster SAR: 19200-19207 (8 ports)
- Neural Distributed: 19300-19307 (8 ports)

### Sequential Execution
All e2e tests run sequentially (RUN_SERIAL=TRUE) to avoid:
- Port conflicts
- Resource contention
- Timing interference

## Performance Targets

### Network Formation
- Mesh formation (8 nodes): < 5 seconds
- Session handshake: < 1 second per peer
- Peer discovery: < 3 seconds

### Message Delivery
- Localhost latency: < 10ms average
- Broadcast to 7 peers: < 50ms total
- Spike timing jitter: < 2ms

### Resilience
- Recovery from jamming: < 3 seconds
- Split-brain healing: < 10 seconds
- Blackout recovery: < 5 seconds

### Learning
- Weight update propagation: < 3 seconds
- State transfer (64KB): < 5 seconds
- Consensus convergence: < 5 seconds

## Troubleshooting

### Port Already in Use
```bash
# Check for lingering processes
lsof -i :19000-19307
kill -9 <PID>
```

### Test Timeout
- Increase timeout in CMakeLists.txt (default 120s)
- Check system load
- Run on dedicated test machine

### Intermittent Failures
- Tests sensitive to system timing
- Disable auto-mode-switch for determinism
- Increase tolerance margins

### Memory Leaks
```bash
# Run with leak detection
LSAN_OPTIONS=suppressions=lsan.supp ./test/e2e/e2e_test_nlp_*
```

## Next Steps

After tests pass:
1. Run full test suite: `ctest -L e2e -V`
2. Check coverage: `gcov` on test binaries
3. Profile performance: `perf record ./test/e2e/e2e_test_nlp_*`
4. Stress test: Increase node count, packet loss, state size
5. Integration test: Connect to real NLP implementation

---

**File Locations**:
- Test source: `/home/bbrelin/nimcp/test/e2e/e2e_test_nlp_*.cpp`
- CMake config: `/home/bbrelin/nimcp/test/e2e/CMakeLists.txt`
- Test binaries: `/home/bbrelin/nimcp/build/test/e2e/e2e_test_nlp_*`
- Summary doc: `/home/bbrelin/nimcp/test/e2e/NLP_E2E_TESTS_SUMMARY.md`

**Created**: 2025-12-08
**Status**: ✅ Ready for testing

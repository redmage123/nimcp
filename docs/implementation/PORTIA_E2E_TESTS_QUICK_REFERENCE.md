# Portia E2E Tests - Quick Reference

## Quick Start

```bash
# Build
cd /home/bbrelin/nimcp/build
cmake ..
make

# Run all Portia E2E tests
ctest -L e2e -R portia

# Run specific test
./test/e2e/e2e_test_portia_constrained_platform
```

## Test Files

| File | Purpose | Key Tests |
|------|---------|-----------|
| `e2e_test_portia_constrained_platform.cpp` | Resource-constrained adaptation | Platform startup, memory budget, resource exhaustion |
| `e2e_test_portia_tier_lifecycle.cpp` | Tier transitions | Full lifecycle, subsystem coordination, data preservation |
| `e2e_test_portia_power_lifecycle.cpp` | Power management | Battery drain, emergency mode, power recovery |
| `e2e_test_portia_learning_adaptation.cpp` | Learning mechanisms | Habituation, association, trial-error learning |
| `e2e_test_portia_sensor_fusion_pipeline.cpp` | Sensor integration | Multi-sensor fusion, failure handling |
| `e2e_test_portia_degradation_scenario.cpp` | Graceful degradation | Progressive degradation, core function preservation |
| `e2e_test_portia_bio_async_pipeline.cpp` | Message flow | Bio-async propagation, load handling |
| `e2e_test_portia_security_integration.cpp` | Security | BBB validation, audit logging |
| `e2e_test_portia_planning_mission.cpp` | Mission planning | Waypoint navigation, obstacle avoidance |
| `e2e_test_portia_threat_response.cpp` | Threat handling | Detection, classification, response |

## Test Statistics

- **Total Files:** 10
- **Total Tests:** ~35-40
- **Total LOC:** ~3,000+
- **Coverage:** Comprehensive

## Key Features Tested

✅ Platform tier management
✅ Resource adaptation
✅ Power management
✅ Learning and adaptation
✅ Sensor fusion
✅ Graceful degradation
✅ Bio-async integration
✅ Security integration
✅ Planning and navigation
✅ Threat response

## Running Tests

### All Portia Tests
```bash
ctest -L e2e -R portia
```

### Verbose Output
```bash
ctest -L e2e -R portia -V
```

### Individual Tests
```bash
./test/e2e/e2e_test_portia_constrained_platform
./test/e2e/e2e_test_portia_tier_lifecycle
./test/e2e/e2e_test_portia_power_lifecycle
./test/e2e/e2e_test_portia_learning_adaptation
./test/e2e/e2e_test_portia_sensor_fusion_pipeline
./test/e2e/e2e_test_portia_degradation_scenario
./test/e2e/e2e_test_portia_bio_async_pipeline
./test/e2e/e2e_test_portia_security_integration
./test/e2e/e2e_test_portia_planning_mission
./test/e2e/e2e_test_portia_threat_response
```

## Test Pattern

```cpp
class PortiaXxxE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async, logging, etc.
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(PortiaXxxE2ETest, TestScenario) {
    // GIVEN: Setup
    // WHEN: Action
    // THEN: Verify
}
```

## Important Notes

1. **Complete Tests:** No stubs or placeholders - all tests are fully implemented
2. **Realistic Scenarios:** Tests simulate real-world conditions
3. **Sequential Execution:** E2E tests run sequentially to avoid resource contention
4. **2-Minute Timeout:** Each test has a 2-minute timeout
5. **Bio-Async Integration:** All tests properly initialize bio-async system

## Status

✅ **COMPLETE** - All 10 test files created and integrated
✅ **CMakeLists.txt** - Updated with all Portia tests
✅ **Documentation** - Comprehensive summary available

---

For detailed information, see: `/home/bbrelin/nimcp/docs/PORTIA_E2E_TESTS_SUMMARY.md`

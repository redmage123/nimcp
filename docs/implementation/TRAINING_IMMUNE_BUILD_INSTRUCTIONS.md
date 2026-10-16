# Training Immune Integration - Build Instructions

## Manual CMakeLists.txt Edit Required

The Training Immune Integration module has been created successfully. However, one manual edit is required to add it to the main build system.

### File to Edit

`/home/bbrelin/nimcp/src/lib/CMakeLists.txt`

### Location

After line 617 (which contains `nimcp_brain_training_integration.c`), add the following lines:

```cmake
    # Middleware - Training Immune Integration (Phase TM-4: Training-Immune Coordination)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_training_immune.c
```

### Context

The edit should be inserted here:

```cmake
    # Middleware - Brain Training Integration (Phase TM-3: Brain-Security Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_brain_training_integration.c

    # Middleware - Training Immune Integration (Phase TM-4: Training-Immune Coordination)   <-- ADD THIS LINE
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_training_immune.c                 <-- ADD THIS LINE

    # NOTE: nimcp_training_plasticity_bridge_bioasync_handlers.c is included via #include
    # in nimcp_training_plasticity_bridge.c, not compiled separately (uses internal types)
```

## Alternative: Use sed command

If you prefer, you can use this sed command from the `/home/bbrelin/nimcp` directory:

```bash
sed -i '620a\    \n    # Middleware - Training Immune Integration (Phase TM-4: Training-Immune Coordination)\n    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_training_immune.c' src/lib/CMakeLists.txt
```

## Files Created

The following files have been successfully created:

### Header
- `/home/bbrelin/nimcp/include/middleware/immune/nimcp_training_immune.h`

### Implementation
- `/home/bbrelin/nimcp/src/middleware/immune/nimcp_training_immune.c`

### Tests
- `/home/bbrelin/nimcp/test/unit/middleware/immune/test_training_immune.cpp`
- Test CMakeLists.txt already updated at: `/home/bbrelin/nimcp/test/unit/middleware/immune/CMakeLists.txt`

## Building and Testing

Once the CMakeLists.txt edit is complete, build and test:

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4
make unit_middleware_immune_training_immune -j4
./test/unit/middleware/immune/unit_middleware_immune_training_immune --gtest_brief=1
```

## Expected Test Results

The test suite includes 30+ tests covering:

- Lifecycle (create, destroy, start, stop)
- Integration with brain immune system, optimizer, gradient manager
- Learning rate modulation based on inflammation levels
- Gradient scaling during immune responses
- Training instability detection (NaN, Inf, explosions, plateaus)
- Automatic immune response triggering
- Statistics and monitoring
- Edge cases and error handling

All tests should pass.

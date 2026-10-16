# NIMCP Suggested Commands

## Build Commands
```bash
# Full build
cd /home/bbrelin/nimcp/build && cmake .. && make nimcp -j4

# Build specific test target
cd /home/bbrelin/nimcp/build && cmake .. && make <target_name> -j4
```

## Git Commands
```bash
git add -A && git commit --no-verify -m "message" && git push
```

## Test Execution
```bash
# Run specific test binary
./test/e2e/<test_binary> --gtest_brief=1

# Run with filter
./test/e2e/<test_binary> --gtest_filter=TestSuite.TestName
```

## System Utilities
- `ls`, `cd`, `grep`, `find`, `cat`, `head`, `tail` - standard Unix commands

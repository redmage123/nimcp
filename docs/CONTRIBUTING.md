# Contributing to NIMCP

Thank you for your interest in contributing to NIMCP! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Documentation](#documentation)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)
- [Community](#community)

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors, regardless of:
- Experience level
- Gender identity and expression
- Sexual orientation
- Disability
- Personal appearance
- Body size
- Race or ethnicity
- Age
- Religion or lack thereof
- Nationality

### Expected Behavior

- Be respectful and professional in all interactions
- Use welcoming and inclusive language
- Accept constructive criticism gracefully
- Focus on what is best for the community and project
- Show empathy towards other community members
- Provide helpful and actionable feedback

### Unacceptable Behavior

- Harassment, trolling, or insulting comments
- Personal or political attacks
- Publishing others' private information without permission
- Spam or off-topic discussions
- Any conduct that would be inappropriate in a professional setting

### Enforcement

Violations of the Code of Conduct may result in:
1. Warning from maintainers
2. Temporary ban from project communication
3. Permanent ban from the project

Report violations to: braun.brelin@ai-elevate.ai

## Getting Started

### Prerequisites

Before contributing, ensure you have:

1. **Development Tools**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential cmake git
   sudo apt-get install python3-dev libjansson-dev liblz4-dev

   # Optional: CUDA for GPU support
   sudo apt-get install nvidia-cuda-toolkit

   # Optional: Encryption support
   sudo apt-get install libsodium-dev
   ```

2. **Testing Tools**
   ```bash
   sudo apt-get install valgrind cppcheck clang-tools
   ```

3. **Code Coverage Tools**
   ```bash
   sudo apt-get install lcov
   ```

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/nimcp.git
   cd nimcp
   ```

3. Add upstream remote:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/nimcp.git
   ```

4. Create a development branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

### Build for Development

```bash
# Create build directory
mkdir build && cd build

# Configure with debug and sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON

# Build
make -j$(nproc)

# Run tests to verify setup
ctest -j$(nproc)
```

## Development Workflow

### 1. Choose an Issue

- Browse [open issues](https://github.com/yourusername/nimcp/issues)
- Look for issues tagged `good-first-issue` or `help-wanted`
- Comment on the issue to express interest
- Wait for maintainer assignment before starting work

### 2. Create a Branch

```bash
# Update your fork
git checkout master
git pull upstream master

# Create feature branch
git checkout -b feature/descriptive-name

# Branch naming conventions:
# - feature/description  (new features)
# - fix/description      (bug fixes)
# - docs/description     (documentation)
# - refactor/description (code refactoring)
# - test/description     (test additions)
```

### 3. Make Changes

- Follow [Coding Standards](#coding-standards)
- Write tests for new functionality
- Update documentation as needed
- Keep commits focused and atomic

### 4. Test Your Changes

```bash
# Run all tests
ctest -j$(nproc)

# Run specific test category
ctest -L unit -V

# Check for memory leaks
valgrind --leak-check=full ./build/bin/your_test

# Static analysis
cppcheck --enable=all src/
```

### 5. Commit Changes

```bash
# Stage changes
git add src/file1.c src/file2.h

# Commit with descriptive message
git commit -m "Add feature: brief description

Detailed explanation of what changed and why.

- Bullet point 1
- Bullet point 2

Fixes #123"
```

### 6. Push and Create Pull Request

```bash
# Push to your fork
git push origin feature/your-feature-name

# Create pull request on GitHub
```

## Coding Standards

NIMCP follows strict coding standards to maintain code quality and consistency.

### C/C++ Style Guide

#### 1. Comment Style (WHAT/WHY/HOW)

All functions must include comprehensive comments:

```c
/**
 * @brief WHAT: Calculate synaptic weight update using STDP
 *
 * WHY: Implements spike-timing dependent plasticity for biologically
 *      realistic learning. This enables the network to learn temporal
 *      correlations between pre- and post-synaptic spikes.
 *
 * HOW: Uses exponential STDP kernel with configurable time constants.
 *      Positive weight change if pre fires before post (potentiation),
 *      negative if post fires before pre (depression).
 *
 * @param synapse Pointer to synapse structure
 * @param pre_spike_time Time of presynaptic spike (ms)
 * @param post_spike_time Time of postsynaptic spike (ms)
 * @param params STDP parameters (tau_plus, tau_minus, A_plus, A_minus)
 * @return Weight change delta (can be positive or negative)
 */
float calculate_stdp_update(
    synapse_t* synapse,
    uint64_t pre_spike_time,
    uint64_t post_spike_time,
    const stdp_params_t* params
);
```

#### 2. Error Handling

Always check for NULL pointers and invalid inputs:

```c
float calculate_stdp_update(
    synapse_t* synapse,
    uint64_t pre_spike_time,
    uint64_t post_spike_time,
    const stdp_params_t* params
) {
    // NULL checks
    if (!synapse) {
        LOG_ERROR("calculate_stdp_update: synapse is NULL");
        return 0.0f;
    }
    if (!params) {
        LOG_ERROR("calculate_stdp_update: params is NULL");
        return 0.0f;
    }

    // Validation
    if (params->tau_plus <= 0.0f || params->tau_minus <= 0.0f) {
        LOG_ERROR("calculate_stdp_update: invalid time constants");
        return 0.0f;
    }

    // Implementation...
}
```

#### 3. Memory Management

Always check allocations and free resources:

```c
neuron_t* create_neuron(uint32_t id) {
    neuron_t* neuron = (neuron_t*)malloc(sizeof(neuron_t));
    if (!neuron) {
        LOG_ERROR("create_neuron: malloc failed");
        return NULL;
    }

    // Initialize
    memset(neuron, 0, sizeof(neuron_t));
    neuron->id = id;

    return neuron;
}

void destroy_neuron(neuron_t* neuron) {
    if (!neuron) {
        return;  // Safe to call with NULL
    }

    // Free any internal allocations
    if (neuron->type_params) {
        free(neuron->type_params);
    }

    free(neuron);
}
```

#### 4. Naming Conventions

```c
// Functions: snake_case with module prefix
bool neural_network_add_neuron(neural_network_t* network, neuron_t* neuron);

// Types: snake_case_t suffix
typedef struct neuron_t neuron_t;
typedef enum learning_rule_t learning_rule_t;

// Constants: UPPER_SNAKE_CASE
#define MAX_NEURONS 100000
#define DEFAULT_LEARNING_RATE 0.01f

// Variables: snake_case
uint32_t neuron_count = 0;
float learning_rate = 0.01f;
```

#### 5. Code Formatting

Use clang-format with provided `.clang-format` file:

```bash
# Format single file
clang-format -i src/core/brain/nimcp_brain.c

# Format all C/C++ files
find src -name "*.c" -o -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```

#### 6. Prohibited Practices

**Never include in commits:**
- `TODO` comments (create issues instead)
- `FIXME` comments (fix before committing)
- Commented-out code (use version control)
- Debug print statements (use logging system)
- Hardcoded paths or credentials
- Compiler warnings

### Python Style Guide

Follow PEP 8 with these additions:

```python
def train_brain(brain, examples, epochs=10):
    """
    WHAT: Train brain on provided examples

    WHY: Enables supervised learning from labeled data using
         backpropagation through spiking neurons

    HOW: Iterates through epochs, presenting each example and
         updating weights using STDP and backprop

    Args:
        brain: NIMCP brain instance
        examples: List of (input, target) tuples
        epochs: Number of training iterations

    Returns:
        List of loss values for each epoch

    Raises:
        ValueError: If examples list is empty
        TypeError: If brain is not a Brain instance
    """
    if not examples:
        raise ValueError("examples list cannot be empty")

    # Implementation...
```

## Testing Requirements

### Test Coverage

- **Minimum coverage**: 100% (strictly enforced)
- All new code must include tests
- Coverage must not decrease with new commits

### Test Categories

1. **Unit Tests** (`test/unit/`)
   - Test individual functions in isolation
   - Mock dependencies
   - Fast execution (<1ms per test)

2. **Integration Tests** (`test/integration/`)
   - Test module interactions
   - Use real dependencies
   - Moderate execution (<100ms per test)

3. **End-to-End Tests** (`test/e2e/`)
   - Test complete workflows
   - Full system integration
   - Slower execution (<1s per test)

### Writing Tests

```c
// test/unit/test_stdp.cpp
#include <gtest/gtest.h>
extern "C" {
    #include "plasticity/stdp/nimcp_stdp.h"
}

class STDPTest : public ::testing::Test {
protected:
    void SetUp() override {
        synapse = create_test_synapse();
        params = create_default_stdp_params();
    }

    void TearDown() override {
        destroy_synapse(synapse);
    }

    synapse_t* synapse;
    stdp_params_t* params;
};

TEST_F(STDPTest, PotentiationWhenPreBeforePost) {
    // Arrange
    uint64_t pre_time = 100;
    uint64_t post_time = 110;  // 10ms after pre

    // Act
    float delta = calculate_stdp_update(synapse, pre_time, post_time, params);

    // Assert
    EXPECT_GT(delta, 0.0f);  // Should potentiate
    EXPECT_LT(delta, params->A_plus);  // Should be bounded
}

TEST_F(STDPTest, DepressionWhenPostBeforePre) {
    // Arrange
    uint64_t pre_time = 110;
    uint64_t post_time = 100;  // 10ms before pre

    // Act
    float delta = calculate_stdp_update(synapse, pre_time, post_time, params);

    // Assert
    EXPECT_LT(delta, 0.0f);  // Should depress
    EXPECT_GT(delta, -params->A_minus);  // Should be bounded
}

TEST_F(STDPTest, HandlesNullSynapse) {
    // Act & Assert
    float delta = calculate_stdp_update(NULL, 100, 110, params);
    EXPECT_EQ(delta, 0.0f);
}
```

### Running Tests

```bash
# All tests
ctest -j$(nproc)

# Specific test
ctest -R test_stdp -V

# With memory checking
ctest -T memcheck

# Generate coverage report
cmake .. -DENABLE_COVERAGE=ON
make && ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## Documentation

### Requirements

- Update documentation for all user-facing changes
- Include code examples that compile and run
- Update API reference for new functions
- Add entries to CHANGELOG.md

### Documentation Types

1. **Code Comments**: See [Coding Standards](#coding-standards)
2. **API Reference**: `docs/api/API_REFERENCE.md`
3. **Guides**: `docs/GETTING_STARTED.md`, etc.
4. **Examples**: `examples/` directory

### Documentation Style

```markdown
## Function Name

### Description
Brief description of what the function does.

### Signature
```c
return_type function_name(param1_type param1, param2_type param2);
```

### Parameters
- `param1`: Description of parameter 1
- `param2`: Description of parameter 2

### Returns
Description of return value

### Example
```c
// Example usage
result = function_name(value1, value2);
```

### Notes
- Important note 1
- Important note 2
```

## Pull Request Process

### Before Submitting

1. **Run full test suite**
   ```bash
   ctest -j$(nproc)
   ```

2. **Check code coverage**
   ```bash
   # Coverage must be 100%
   lcov --capture --directory . --output-file coverage.info
   lcov --summary coverage.info
   ```

3. **Run static analysis**
   ```bash
   cppcheck --enable=all src/
   clang-tidy -p build src/**/*.c
   ```

4. **Format code**
   ```bash
   find src -name "*.c" -o -name "*.h" | xargs clang-format -i
   ```

5. **Update documentation**
   - API reference
   - User guides
   - CHANGELOG.md

6. **Rebase on master**
   ```bash
   git fetch upstream
   git rebase upstream/master
   ```

### PR Template

```markdown
## Description
Brief description of changes

## Motivation
Why are these changes needed?

## Changes Made
- Change 1
- Change 2
- Change 3

## Testing
- [ ] All existing tests pass
- [ ] New tests added for new functionality
- [ ] Coverage remains at 100%
- [ ] Tested on Ubuntu 22.04
- [ ] Tested with AddressSanitizer
- [ ] Static analysis clean

## Documentation
- [ ] API reference updated
- [ ] User guides updated
- [ ] CHANGELOG.md updated
- [ ] Code examples added

## Checklist
- [ ] Code follows style guide
- [ ] WHAT/WHY/HOW comments added
- [ ] NULL checks for all pointers
- [ ] No TODO/FIXME in code
- [ ] No compiler warnings
- [ ] Memory leaks checked with valgrind

## Related Issues
Fixes #123
Related to #456
```

### Review Process

1. **Automated Checks**
   - CI/CD builds and tests
   - Code coverage check
   - Static analysis
   - Formatting verification

2. **Maintainer Review**
   - Code quality assessment
   - Architecture alignment
   - Documentation review
   - Test coverage verification

3. **Feedback and Iteration**
   - Address review comments
   - Update based on feedback
   - Re-request review when ready

4. **Merge**
   - Squash commits if requested
   - Update commit message
   - Maintainer merges to master

## Reporting Issues

### Bug Reports

Use the following template:

```markdown
## Bug Description
Clear description of the bug

## Steps to Reproduce
1. Step 1
2. Step 2
3. Step 3

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- OS: Ubuntu 22.04
- NIMCP Version: 2.6.2
- Compiler: GCC 11.3.0
- CMake Version: 3.22.1
- CUDA Version (if applicable): 11.8

## Logs/Output
```
Paste relevant logs or error messages
```

## Additional Context
Any other relevant information
```

### Feature Requests

```markdown
## Feature Description
Clear description of proposed feature

## Motivation
Why is this feature needed?

## Proposed Implementation
High-level implementation approach

## Alternatives Considered
Other approaches you've considered

## Additional Context
Any other relevant information
```

## Community

### Communication Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and discussions
- **Email**: braun.brelin@ai-elevate.ai (for sensitive matters)

### Getting Help

- Check [documentation](docs/)
- Search [existing issues](https://github.com/yourusername/nimcp/issues)
- Ask in GitHub Discussions
- Join community meetings (TBD)

### Recognition

Contributors are recognized in:
- CHANGELOG.md for each release
- Contributors section of README.md
- Release notes

---

## Thank You!

Your contributions make NIMCP better for everyone. We appreciate your time and effort in helping build biologically-inspired artificial intelligence!

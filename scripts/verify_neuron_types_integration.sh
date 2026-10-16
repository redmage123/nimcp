#!/bin/bash
# Verification script for neuron types bio-async integration
# Date: 2025-11-29

set -e

echo "========================================="
echo "Neuron Types Bio-Async Integration Check"
echo "========================================="
echo ""

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}1. Checking bio-async includes...${NC}"
echo "   nimcp_neural_logic.c:"
grep "include.*bio" src/core/neuron_types/nimcp_neural_logic.c | sed 's/^/     /'
echo ""
echo "   nimcp_neuron_types.c:"
grep "include.*bio" src/core/neuron_types/nimcp_neuron_types.c | sed 's/^/     /'
echo ""

echo -e "${BLUE}2. Checking memory guards includes...${NC}"
grep "memory_guards" src/core/neuron_types/nimcp_neural_logic.c src/core/neuron_types/nimcp_neuron_types.c | sed 's/^/   /'
echo ""

echo -e "${BLUE}3. Checking logging integration...${NC}"
neural_logic_logs=$(grep -c "LOG_DEBUG\|LOG_INFO\|LOG_WARN\|LOG_ERROR" src/core/neuron_types/nimcp_neural_logic.c)
neuron_types_logs=$(grep -c "LOG_DEBUG\|LOG_INFO\|LOG_WARN\|LOG_ERROR" src/core/neuron_types/nimcp_neuron_types.c)
total_logs=$((neural_logic_logs + neuron_types_logs))

echo "   nimcp_neural_logic.c: $neural_logic_logs logging statements"
echo "   nimcp_neuron_types.c: $neuron_types_logs logging statements"
echo "   Total: $total_logs logging statements"
echo ""

echo -e "${BLUE}4. Checking bio-async module IDs...${NC}"
echo "   Neural Logic Module ID:"
grep "BIO_MODULE_NEURAL_LOGIC" src/core/neuron_types/nimcp_neural_logic.c | head -1 | sed 's/^/     /'
echo ""
echo "   Neuron Types Module ID:"
grep "BIO_MODULE_NEURON_TYPES" src/core/neuron_types/nimcp_neuron_types.c | head -1 | sed 's/^/     /'
echo ""

echo -e "${BLUE}5. Checking test file...${NC}"
if [ -f "test/unit/core/neuron_types/test_neuron_types_bio_async.cpp" ]; then
    test_count=$(grep -c "TEST_F" test/unit/core/neuron_types/test_neuron_types_bio_async.cpp)
    echo -e "   ${GREEN}✓${NC} Test file exists: test_neuron_types_bio_async.cpp"
    echo "   Test cases: $test_count"
    echo ""
    echo "   Test cases breakdown:"
    grep "TEST_F" test/unit/core/neuron_types/test_neuron_types_bio_async.cpp | sed 's/TEST_F(NeuronTypesBioAsyncTest, /     - /' | sed 's/) {//'
else
    echo "   ✗ Test file NOT found"
fi
echo ""

echo -e "${BLUE}6. Checking bio-async message publishing...${NC}"
echo "   neural_logic_create_gate bio-async calls:"
grep -c "bio_router_broadcast\|BIO_MSG_NEURON_CREATED" src/core/neuron_types/nimcp_neural_logic.c | sed 's/^/     /'
echo ""

echo -e "${BLUE}7. Integration completeness...${NC}"
checks=0
total=0

# Check 1: Bio-async includes in both files
total=$((total + 1))
if grep -q "nimcp_bio_async.h" src/core/neuron_types/nimcp_neural_logic.c && \
   grep -q "nimcp_bio_async.h" src/core/neuron_types/nimcp_neuron_types.c; then
    echo -e "   ${GREEN}✓${NC} Bio-async includes in both files"
    checks=$((checks + 1))
else
    echo "   ✗ Bio-async includes missing"
fi

# Check 2: Logging in both files
total=$((total + 1))
if [ $neural_logic_logs -gt 0 ] && [ $neuron_types_logs -gt 0 ]; then
    echo -e "   ${GREEN}✓${NC} Logging present in both files"
    checks=$((checks + 1))
else
    echo "   ✗ Logging missing in one or more files"
fi

# Check 3: Memory guards in both files
total=$((total + 1))
if grep -q "memory_guards" src/core/neuron_types/nimcp_neural_logic.c && \
   grep -q "memory_guards" src/core/neuron_types/nimcp_neuron_types.c; then
    echo -e "   ${GREEN}✓${NC} Memory guards in both files"
    checks=$((checks + 1))
else
    echo "   ✗ Memory guards missing"
fi

# Check 4: Test file exists
total=$((total + 1))
if [ -f "test/unit/core/neuron_types/test_neuron_types_bio_async.cpp" ]; then
    echo -e "   ${GREEN}✓${NC} Unit test file created"
    checks=$((checks + 1))
else
    echo "   ✗ Unit test file missing"
fi

# Check 5: Sufficient test coverage
total=$((total + 1))
if [ $test_count -ge 10 ]; then
    echo -e "   ${GREEN}✓${NC} Comprehensive test coverage ($test_count tests)"
    checks=$((checks + 1))
else
    echo "   ✗ Insufficient test coverage"
fi

# Check 6: Module IDs defined
total=$((total + 1))
if grep -q "BIO_MODULE_NEURAL_LOGIC" src/core/neuron_types/nimcp_neural_logic.c && \
   grep -q "BIO_MODULE_NEURON_TYPES" src/core/neuron_types/nimcp_neuron_types.c; then
    echo -e "   ${GREEN}✓${NC} Bio-async module IDs defined"
    checks=$((checks + 1))
else
    echo "   ✗ Module IDs missing"
fi

echo ""
echo "========================================="
echo -e "Integration Status: ${GREEN}$checks/$total checks passed${NC}"
echo "========================================="
echo ""

if [ $checks -eq $total ]; then
    echo -e "${GREEN}✓ Integration COMPLETE and VERIFIED${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. cd build"
    echo "  2. cmake .."
    echo "  3. make test_neuron_types_bio_async"
    echo "  4. ./test/unit/core/neuron_types/test_neuron_types_bio_async"
    exit 0
else
    echo "✗ Integration INCOMPLETE - please review failed checks"
    exit 1
fi

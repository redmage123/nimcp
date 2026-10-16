#!/bin/bash
# Verification script for plasticity bio-async integration

set -e

echo "=========================================="
echo "PLASTICITY BIO-ASYNC INTEGRATION VERIFICATION"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS=0
FAIL=0

check_file() {
    local file=$1
    local pattern=$2
    local description=$3

    if grep -q "$pattern" "$file" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} $description"
        ((PASS++))
        return 0
    else
        echo -e "${RED}✗${NC} $description"
        ((FAIL++))
        return 1
    fi
}

echo "Checking Module Files..."
echo "----------------------------------------"

# Attention module
check_file "src/plasticity/attention/nimcp_attention.c" "bio_module_context_t bio_ctx" \
    "Attention: bio_ctx field added"
check_file "src/plasticity/attention/nimcp_attention.c" "#define LOG_MODULE" \
    "Attention: LOG_MODULE defined"
check_file "src/plasticity/attention/nimcp_attention.c" "nimcp_blood_brain_barrier.h" \
    "Attention: Security header included"

# Dendritic module
check_file "src/plasticity/dendritic/nimcp_dendritic.c" "bio_module_context_t bio_ctx" \
    "Dendritic: bio_ctx field added"
check_file "src/plasticity/dendritic/nimcp_dendritic.c" "#define LOG_MODULE" \
    "Dendritic: LOG_MODULE defined"
check_file "src/plasticity/dendritic/nimcp_dendritic.c" "nimcp_blood_brain_barrier.h" \
    "Dendritic: Security header included"

# Adaptive module
check_file "src/plasticity/adaptive/nimcp_adaptive.c" "bio_module_context_t bio_ctx" \
    "Adaptive: bio_ctx field added"
check_file "src/plasticity/adaptive/nimcp_adaptive.c" "#define LOG_MODULE" \
    "Adaptive: LOG_MODULE defined"
check_file "src/plasticity/adaptive/nimcp_adaptive.c" "nimcp_blood_brain_barrier.h" \
    "Adaptive: Security header included"

# Predictive coding module
check_file "src/plasticity/predictive/nimcp_predictive_coding.c" "bio_module_context_t bio_ctx" \
    "Predictive: bio_ctx field added"
check_file "src/plasticity/predictive/nimcp_predictive_coding.c" "#define LOG_MODULE" \
    "Predictive: LOG_MODULE defined"
check_file "src/plasticity/predictive/nimcp_predictive_coding.c" "nimcp_blood_brain_barrier.h" \
    "Predictive: Security header included"

# Pink noise module
check_file "src/plasticity/noise/nimcp_pink_noise.c" "bio_module_context_t bio_ctx" \
    "Pink Noise: bio_ctx field added"
check_file "src/plasticity/noise/nimcp_pink_noise.c" "#define LOG_MODULE" \
    "Pink Noise: LOG_MODULE defined"
check_file "src/plasticity/noise/nimcp_pink_noise.c" "nimcp_blood_brain_barrier.h" \
    "Pink Noise: Security header included"

# Receptor subtypes module
check_file "src/plasticity/neuromodulators/nimcp_receptor_subtypes.c" "bio_module_context_t bio_ctx" \
    "Receptor: bio_ctx field added"
check_file "src/plasticity/neuromodulators/nimcp_receptor_subtypes.c" "#define LOG_MODULE" \
    "Receptor: LOG_MODULE defined"
check_file "src/plasticity/neuromodulators/nimcp_receptor_subtypes.c" "nimcp_blood_brain_barrier.h" \
    "Receptor: Security header included"

# Phasic/Tonic module
check_file "src/plasticity/neuromodulators/nimcp_phasic_tonic.c" "bio_module_context_t bio_ctx" \
    "Phasic/Tonic: bio_ctx field added"
check_file "src/plasticity/neuromodulators/nimcp_phasic_tonic.c" "#define LOG_MODULE" \
    "Phasic/Tonic: LOG_MODULE defined"
check_file "src/plasticity/neuromodulators/nimcp_phasic_tonic.c" "nimcp_blood_brain_barrier.h" \
    "Phasic/Tonic: Security header included"

# Metabolic pathways module
check_file "src/plasticity/neuromodulators/nimcp_metabolic_pathways.c" "bio_module_context_t bio_ctx" \
    "Metabolic: bio_ctx field added"
check_file "src/plasticity/neuromodulators/nimcp_metabolic_pathways.c" "#define LOG_MODULE" \
    "Metabolic: LOG_MODULE defined"
check_file "src/plasticity/neuromodulators/nimcp_metabolic_pathways.c" "nimcp_blood_brain_barrier.h" \
    "Metabolic: Security header included"

# Vesicle packaging module
check_file "src/plasticity/neuromodulators/nimcp_vesicle_packaging.c" "bio_module_context_t bio_ctx" \
    "Vesicle: bio_ctx field added"
check_file "src/plasticity/neuromodulators/nimcp_vesicle_packaging.c" "#define LOG_MODULE" \
    "Vesicle: LOG_MODULE defined"
check_file "src/plasticity/neuromodulators/nimcp_vesicle_packaging.c" "nimcp_blood_brain_barrier.h" \
    "Vesicle: Security header included"

echo ""
echo "Checking Test Files..."
echo "----------------------------------------"

# Unit tests
test -f "test/unit/plasticity/attention/test_attention_bioasync.cpp" && \
    echo -e "${GREEN}✓${NC} Attention unit test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Attention unit test missing" && ((FAIL++)))

test -f "test/unit/plasticity/dendritic/test_dendritic_bioasync.cpp" && \
    echo -e "${GREEN}✓${NC} Dendritic unit test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Dendritic unit test missing" && ((FAIL++)))

test -f "test/unit/plasticity/adaptive/test_adaptive_bioasync.cpp" && \
    echo -e "${GREEN}✓${NC} Adaptive unit test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Adaptive unit test missing" && ((FAIL++)))

test -f "test/unit/plasticity/predictive/test_predictive_bioasync.cpp" && \
    echo -e "${GREEN}✓${NC} Predictive unit test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Predictive unit test missing" && ((FAIL++)))

# Integration tests
test -f "test/integration/plasticity/test_plasticity_bioasync_integration.cpp" && \
    echo -e "${GREEN}✓${NC} Integration test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Integration test missing" && ((FAIL++)))

# Regression tests
test -f "test/regression/plasticity/test_learning_stability.cpp" && \
    echo -e "${GREEN}✓${NC} Regression test exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Regression test missing" && ((FAIL++)))

echo ""
echo "Checking CMake Build Files..."
echo "----------------------------------------"

test -f "test/unit/plasticity/CMakeLists.txt" && \
    echo -e "${GREEN}✓${NC} Unit test CMakeLists.txt exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Unit test CMakeLists.txt missing" && ((FAIL++)))

test -f "test/integration/plasticity/CMakeLists.txt" && \
    echo -e "${GREEN}✓${NC} Integration test CMakeLists.txt exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Integration test CMakeLists.txt missing" && ((FAIL++)))

test -f "test/regression/plasticity/CMakeLists.txt" && \
    echo -e "${GREEN}✓${NC} Regression test CMakeLists.txt exists" && ((PASS++)) || \
    (echo -e "${RED}✗${NC} Regression test CMakeLists.txt missing" && ((FAIL++)))

echo ""
echo "=========================================="
echo "RESULTS"
echo "=========================================="
echo -e "${GREEN}Passed:${NC} $PASS"
echo -e "${RED}Failed:${NC} $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ ALL CHECKS PASSED${NC}"
    echo "Integration is complete and verified!"
    exit 0
else
    echo -e "${RED}✗ SOME CHECKS FAILED${NC}"
    echo "Please review the failed items above."
    exit 1
fi

#!/bin/bash

echo "========================================"
echo "Cognitive Memory Integration Verification"
echo "========================================"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS=0
FAIL=0

check_file() {
    local file=$1
    local pattern=$2
    local desc=$3
    
    if grep -q "$pattern" "$file" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} $desc: $file"
        ((PASS++))
        return 0
    else
        echo -e "${RED}✗${NC} $desc: $file"
        ((FAIL++))
        return 1
    fi
}

echo "Checking bio-async includes..."
check_file "src/cognitive/memory/nimcp_engram.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/memory/nimcp_semantic_memory.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/memory/nimcp_systems_consolidation.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/memory/nimcp_wm_transfer.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/working_memory/nimcp_working_memory.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/meta_learning/nimcp_meta_learning.c" "nimcp_bio_async.h" "Bio-async header"
check_file "src/cognitive/predictive/nimcp_predictive.c" "nimcp_bio_async.h" "Bio-async header"

echo ""
echo "Checking unified memory includes..."
check_file "src/cognitive/memory/nimcp_engram.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/memory/nimcp_semantic_memory.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/memory/nimcp_systems_consolidation.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/memory/nimcp_wm_transfer.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/working_memory/nimcp_working_memory.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/meta_learning/nimcp_meta_learning.c" "nimcp_unified_memory.h" "Unified memory"
check_file "src/cognitive/predictive/nimcp_predictive.c" "nimcp_unified_memory.h" "Unified memory"

echo ""
echo "Checking logging includes..."
check_file "src/cognitive/memory/nimcp_engram.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/memory/nimcp_semantic_memory.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/memory/nimcp_systems_consolidation.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/memory/nimcp_wm_transfer.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/working_memory/nimcp_working_memory.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/meta_learning/nimcp_meta_learning.c" "nimcp_logging.h" "Logging header"
check_file "src/cognitive/predictive/nimcp_predictive.c" "nimcp_logging.h" "Logging header"

echo ""
echo "Checking module ID definitions..."
check_file "src/cognitive/memory/nimcp_engram.c" "BIO_MODULE_MEMORY" "Module ID 0x0330"
check_file "src/cognitive/memory/nimcp_semantic_memory.c" "BIO_MODULE_SEMANTIC_MEMORY" "Module ID 0x0331"
check_file "src/cognitive/memory/nimcp_systems_consolidation.c" "BIO_MODULE_SYSTEMS_CONSOLIDATION" "Module ID 0x0332"
check_file "src/cognitive/memory/nimcp_wm_transfer.c" "BIO_MODULE_WM_TRANSFER" "Module ID 0x0333"
check_file "src/cognitive/working_memory/nimcp_working_memory.c" "BIO_MODULE_WORKING_MEMORY" "Module ID 0x0334"
check_file "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c" "BIO_MODULE_AUTOBIOGRAPHICAL_MEMORY" "Module ID 0x0335"
check_file "src/cognitive/meta_learning/nimcp_meta_learning.c" "BIO_MODULE_META_LEARNING" "Module ID 0x0336"
check_file "src/cognitive/predictive/nimcp_predictive.c" "BIO_MODULE_PREDICTIVE" "Module ID 0x0337"

echo ""
echo "Checking LOG_MODULE definitions..."
check_file "src/cognitive/memory/nimcp_engram.c" '#define LOG_MODULE "engram"' "LOG_MODULE"
check_file "src/cognitive/memory/nimcp_semantic_memory.c" '#define LOG_MODULE "semantic_memory"' "LOG_MODULE"
check_file "src/cognitive/memory/nimcp_systems_consolidation.c" '#define LOG_MODULE "systems_consolidation"' "LOG_MODULE"
check_file "src/cognitive/memory/nimcp_wm_transfer.c" '#define LOG_MODULE "wm_transfer"' "LOG_MODULE"
check_file "src/cognitive/working_memory/nimcp_working_memory.c" '#define LOG_MODULE "working_memory"' "LOG_MODULE"
check_file "src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c" '#define LOG_MODULE "autobiographical_memory"' "LOG_MODULE"
check_file "src/cognitive/meta_learning/nimcp_meta_learning.c" '#define LOG_MODULE "meta_learning"' "LOG_MODULE"
check_file "src/cognitive/predictive/nimcp_predictive.c" '#define LOG_MODULE "predictive"' "LOG_MODULE"

echo ""
echo "========================================"
echo -e "Results: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}"
echo "========================================"

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All checks passed! Integration complete.${NC}"
    exit 0
else
    echo -e "${RED}Some checks failed. Please review.${NC}"
    exit 1
fi

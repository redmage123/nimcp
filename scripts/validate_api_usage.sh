#!/bin/bash
#=============================================================================
# validate_api_usage.sh - Check for common API mistakes before commit
#=============================================================================
# WHAT: Scans source files for known incorrect API patterns
# WHY:  Prevent compilation failures from API mismatches
# HOW:  Grep for wrong patterns and report violations
#
# Usage:
#   ./scripts/validate_api_usage.sh [directory]
#   ./scripts/validate_api_usage.sh src/networking/nlp/
#
# Run before committing new code to catch API mistakes early.
#=============================================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SEARCH_DIR="${1:-src}"
ERRORS=0
WARNINGS=0

echo "========================================"
echo "NIMCP API Usage Validator"
echo "========================================"
echo "Scanning: $SEARCH_DIR"
echo ""

#-----------------------------------------------------------------------------
# Check function: search for pattern and report
#-----------------------------------------------------------------------------
check_pattern() {
    local pattern="$1"
    local correct="$2"
    local severity="$3"  # ERROR or WARN

    local matches=$(grep -rn "$pattern" "$SEARCH_DIR" --include="*.c" --include="*.h" 2>/dev/null | grep -v "// WRONG" | grep -v "DO NOT USE" || true)

    if [ -n "$matches" ]; then
        if [ "$severity" = "ERROR" ]; then
            echo -e "${RED}ERROR: Found '$pattern' (use '$correct' instead)${NC}"
            ((ERRORS++))
        else
            echo -e "${YELLOW}WARN: Found '$pattern' (prefer '$correct')${NC}"
            ((WARNINGS++))
        fi
        echo "$matches" | head -5
        echo ""
    fi
}

#-----------------------------------------------------------------------------
# Memory API Checks
#-----------------------------------------------------------------------------
echo "--- Memory API ---"
check_pattern "nimcp_unified_calloc" "nimcp_calloc" "ERROR"
check_pattern "nimcp_unified_malloc" "nimcp_malloc" "ERROR"
check_pattern "nimcp_unified_free" "nimcp_free" "ERROR"
check_pattern "nimcp_unified_realloc" "nimcp_realloc" "ERROR"

#-----------------------------------------------------------------------------
# Logging API Checks
#-----------------------------------------------------------------------------
echo "--- Logging API ---"
check_pattern "NIMCP_LOG_DEBUG" "LOG_DEBUG or LOG_MODULE_DEBUG" "ERROR"
check_pattern "NIMCP_LOG_ERROR" "LOG_ERROR or LOG_MODULE_ERROR" "ERROR"
check_pattern "NIMCP_LOG_INFO" "LOG_INFO or LOG_MODULE_INFO" "ERROR"
check_pattern "NIMCP_LOG_WARN" "LOG_WARN or LOG_MODULE_WARN" "ERROR"
check_pattern "NIMCP_LOG_TRACE" "LOG_TRACE or LOG_MODULE_TRACE" "ERROR"

#-----------------------------------------------------------------------------
# Bio-Async API Checks
#-----------------------------------------------------------------------------
echo "--- Bio-Async API ---"
check_pattern "nimcp_bio_message_t" "bio_message_header_t" "ERROR"
check_pattern "NIMCP_BIO_MSG_" "BIO_MSG_" "ERROR"
check_pattern "nimcp_bio_async_inbox_t" "void*" "ERROR"

#-----------------------------------------------------------------------------
# Time API Checks
#-----------------------------------------------------------------------------
echo "--- Time API ---"
check_pattern "nimcp_platform_time_ms(" "nimcp_platform_time_monotonic_ms()" "ERROR"
check_pattern "nimcp_time_ms(" "nimcp_platform_time_monotonic_ms()" "ERROR"
check_pattern "nimcp_get_time_ms(" "nimcp_platform_time_monotonic_ms()" "ERROR"

#-----------------------------------------------------------------------------
# Include Path Checks
#-----------------------------------------------------------------------------
echo "--- Include Paths ---"
check_pattern '"nimcp_broca.h"' '"nimcp_broca_adapter.h"' "ERROR"
check_pattern '"nimcp_unified_memory.h"' '"nimcp_memory.h"' "ERROR"

#-----------------------------------------------------------------------------
# NLP-Specific Checks
#-----------------------------------------------------------------------------
echo "--- NLP Protocol API ---"
check_pattern "NLP_MSG_NEURAL_SPIKE_BATCH" "NLP_MSG_SPIKE_BATCH" "ERROR"
check_pattern "NLP_MSG_NEURAL_WEIGHT_DELTA" "NLP_MSG_WEIGHT_DELTA" "ERROR"
check_pattern "NLP_MSG_NEURAL_GRADIENT_PUSH" "NLP_MSG_GRADIENT_PUSH" "ERROR"
check_pattern "NLP_MSG_NEURAL_STATE_SYNC" "NLP_MSG_STATE_SYNC" "ERROR"
check_pattern "nlp_node_send(" "nlp_send(" "ERROR"
check_pattern "nlp_node_broadcast(" "nlp_broadcast(" "ERROR"

#-----------------------------------------------------------------------------
# Summary
#-----------------------------------------------------------------------------
echo "========================================"
echo "SUMMARY"
echo "========================================"

if [ $ERRORS -gt 0 ]; then
    echo -e "${RED}Errors: $ERRORS${NC}"
fi

if [ $WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}Warnings: $WARNINGS${NC}"
fi

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}No API issues found!${NC}"
fi

echo ""
echo "See docs/API_REFERENCE.md for correct API usage."
echo ""

# Exit with error code if errors found
if [ $ERRORS -gt 0 ]; then
    exit 1
fi

exit 0

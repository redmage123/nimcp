#!/bin/bash
#==============================================================================
# verify_integration.sh - Verify bio-async, logging, and unified memory integration
#==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

log_error() {
    echo -e "${RED}[✗]${NC} $1"
}

echo "================================================================================"
echo "  Middleware and IO Integration Verification"
echo "================================================================================"
echo ""

cd "$PROJECT_ROOT"

# Expected number of integrated files
EXPECTED_FILES=35

log_info "Checking bio-async integration..."
bio_async=$(find src/middleware src/io -name "*.c" ! -path "*/CMakeFiles/*" ! -name "*.backup" -exec grep -l "nimcp_bio_async.h" {} \; | wc -l)
if [ $bio_async -eq $EXPECTED_FILES ]; then
    log_success "Bio-async includes: $bio_async/$EXPECTED_FILES"
else
    log_warning "Bio-async includes: $bio_async/$EXPECTED_FILES (expected $EXPECTED_FILES)"
fi

log_info "Checking unified memory integration..."
unified_mem=$(find src/middleware src/io -name "*.c" ! -path "*/CMakeFiles/*" ! -name "*.backup" -exec grep -l "nimcp_unified_memory.h" {} \; | wc -l)
if [ $unified_mem -eq $EXPECTED_FILES ]; then
    log_success "Unified memory includes: $unified_mem/$EXPECTED_FILES"
else
    log_warning "Unified memory includes: $unified_mem/$EXPECTED_FILES (expected $EXPECTED_FILES)"
fi

log_info "Checking LOG_MODULE_ID definitions..."
module_ids=$(find src/middleware src/io -name "*.c" ! -path "*/CMakeFiles/*" ! -name "*.backup" -exec grep -l "LOG_MODULE_ID" {} \; | wc -l)
if [ $module_ids -ge $EXPECTED_FILES ]; then
    log_success "LOG_MODULE_ID defines: $module_ids (>= $EXPECTED_FILES)"
else
    log_warning "LOG_MODULE_ID defines: $module_ids (expected >= $EXPECTED_FILES)"
fi

echo ""
if [ $bio_async -eq $EXPECTED_FILES ] && [ $unified_mem -eq $EXPECTED_FILES ] && [ $module_ids -ge $EXPECTED_FILES ]; then
    log_success "✅ ALL INTEGRATIONS VERIFIED SUCCESSFULLY!"
else
    log_warning "⚠️  INTEGRATION INCOMPLETE"
fi

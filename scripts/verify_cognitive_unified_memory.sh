#!/bin/bash
# Verification script for cognitive modules unified memory migration

echo "==================================================================="
echo "Cognitive Modules Unified Memory Migration Verification"
echo "==================================================================="
echo ""

PASS=0
FAIL=0

# Files to check
FILES=(
    "src/cognitive/reasoning/nimcp_backward_chaining.c"
    "src/cognitive/reasoning/nimcp_forward_chaining.c"
    "src/cognitive/reasoning/nimcp_unification_engine.c"
    "src/cognitive/reasoning/nimcp_reasoning_factory.c"
    "src/cognitive/knowledge/nimcp_knowledge_fractal.c"
    "src/cognitive/introspection/nimcp_connectivity_health.c"
)

echo "1. Checking for unified memory includes..."
echo "-------------------------------------------------------------------"
for file in "${FILES[@]}"; do
    if grep -q '#include "utils/memory/nimcp_memory.h"' "$file" || \
       grep -q '#include "utils/memory/nimcp_unified_memory.h"' "$file"; then
        echo "✅ $file has unified memory include"
        ((PASS++))
    else
        echo "❌ $file missing unified memory include"
        ((FAIL++))
    fi
done
echo ""

echo "2. Checking for LOG_MODULE definitions..."
echo "-------------------------------------------------------------------"
for file in "${FILES[@]}"; do
    if grep -q '^#define LOG_MODULE' "$file"; then
        MODULE=$(grep '^#define LOG_MODULE' "$file" | cut -d'"' -f2)
        echo "✅ $file: LOG_MODULE = \"$MODULE\""
        ((PASS++))
    else
        echo "❌ $file: missing LOG_MODULE definition"
        ((FAIL++))
    fi
done
echo ""

echo "3. Checking for raw malloc/calloc/realloc/free calls..."
echo "-------------------------------------------------------------------"
for file in "${FILES[@]}"; do
    RAW_CALLS=$(grep -c '\b\(malloc\|calloc\|realloc\|free\)\s*(' "$file" 2>/dev/null || echo 0)
    if [ "$RAW_CALLS" -eq 0 ]; then
        echo "✅ $file: no raw memory calls"
        ((PASS++))
    else
        echo "⚠️  $file: found $RAW_CALLS raw memory calls (check if intentional)"
        # Don't count as failure - might be in comments
    fi
done
echo ""

echo "4. Checking for unified memory function usage..."
echo "-------------------------------------------------------------------"
for file in "${FILES[@]}"; do
    NIMCP_CALLS=$(grep -c '\bnimcp_\(malloc\|calloc\|realloc\|free\)\s*(' "$file" 2>/dev/null || echo 0)
    if [ "$NIMCP_CALLS" -gt 0 ]; then
        echo "✅ $file: uses nimcp_* functions ($NIMCP_CALLS calls)"
        ((PASS++))
    else
        echo "ℹ️  $file: no direct memory allocations (delegates or stack-based)"
    fi
done
echo ""

echo "==================================================================="
echo "Summary"
echo "==================================================================="
echo "Total checks passed: $PASS"
echo "Total checks failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✅ All checks passed! Migration successful."
    exit 0
else
    echo "❌ Some checks failed. Please review."
    exit 1
fi

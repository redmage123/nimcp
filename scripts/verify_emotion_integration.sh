#!/bin/bash
# Verification script for emotion module integration

set -e

echo "========================================="
echo "  Emotion Module Integration Verification"
echo "========================================="
echo ""

PASS=0
FAIL=0

# Function to check and report
check() {
    local test_name="$1"
    local command="$2"

    echo -n "Testing: $test_name... "
    if eval "$command" > /dev/null 2>&1; then
        echo "✓ PASS"
        ((PASS++))
    else
        echo "✗ FAIL"
        ((FAIL++))
    fi
}

# 1. Check all modules have unified memory includes
echo "1. Unified Memory Integration"
echo "------------------------------"
check "Emotional System has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/emotions/nimcp_emotional_system.c"
check "Grief has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/grief/nimcp_grief_and_loss.c"
check "Joy has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/joy/nimcp_joy_euphoria.c"
check "Remorse has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/remorse/nimcp_remorse_regret.c"
check "Empathy has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/empathetic_response/nimcp_empathetic_response.c"
check "Emotional Tagging has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/emotional_tagging/nimcp_emotional_tagging.c"
check "Emotion Recognition has unified memory" \
    "grep -q 'nimcp_unified_memory.h' src/cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c"
echo ""

# 2. Check all modules have logging
echo "2. Logging Integration"
echo "----------------------"
check "Emotional System has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/emotions/nimcp_emotional_system.c"
check "Grief has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/grief/nimcp_grief_and_loss.c"
check "Joy has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/joy/nimcp_joy_euphoria.c"
check "Remorse has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/remorse/nimcp_remorse_regret.c"
check "Empathy has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/empathetic_response/nimcp_empathetic_response.c"
check "Emotional Tagging has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/emotional_tagging/nimcp_emotional_tagging.c"
check "Emotion Recognition has logging" \
    "grep -q 'nimcp_logging.h' src/cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c"
echo ""

# 3. Check all modules have bio-async
echo "3. Bio-Async Integration"
echo "------------------------"
check "Emotional System has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/emotions/nimcp_emotional_system.c"
check "Grief has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/grief/nimcp_grief_and_loss.c"
check "Joy has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/joy/nimcp_joy_euphoria.c"
check "Remorse has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/remorse/nimcp_remorse_regret.c"
check "Empathy has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/empathetic_response/nimcp_empathetic_response.c"
check "Emotional Tagging has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/emotional_tagging/nimcp_emotional_tagging.c"
check "Emotion Recognition has bio-async" \
    "grep -q 'nimcp_bio_async.h' src/cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c"
echo ""

# 4. Check module IDs are defined
echo "4. Module IDs"
echo "-------------"
check "EMOTIONS module ID (0x0320)" \
    "grep -q 'BIO_MODULE_EMOTIONS 0x0320' src/cognitive/emotions/nimcp_emotional_system.c"
check "EMOTION_RECOGNITION module ID (0x0321)" \
    "grep -q 'BIO_MODULE_EMOTION_RECOGNITION 0x0321' src/cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c"
check "EMPATHY module ID (0x0322)" \
    "grep -q 'BIO_MODULE_EMPATHY 0x0322' src/cognitive/empathetic_response/nimcp_empathetic_response.c"
check "GRIEF module ID (0x0323)" \
    "grep -q 'BIO_MODULE_GRIEF 0x0323' src/cognitive/grief/nimcp_grief_and_loss.c"
check "JOY module ID (0x0324)" \
    "grep -q 'BIO_MODULE_JOY 0x0324' src/cognitive/joy/nimcp_joy_euphoria.c"
check "REMORSE module ID (0x0325)" \
    "grep -q 'BIO_MODULE_REMORSE 0x0325' src/cognitive/remorse/nimcp_remorse_regret.c"
check "EMOTIONAL_TAGGING module ID (0x0326)" \
    "grep -q 'BIO_MODULE_EMOTIONAL_TAGGING 0x0326' src/cognitive/emotional_tagging/nimcp_emotional_tagging.c"
echo ""

# 5. Check no old memory functions remain
echo "5. Memory Function Replacement"
echo "-------------------------------"
check "No malloc in emotion modules" \
    "! grep -r '\\bmalloc(' src/cognitive/emotion* src/cognitive/grief src/cognitive/joy src/cognitive/remorse src/cognitive/empathetic_response 2>/dev/null | grep -v nimcp_malloc"
check "No calloc in emotion modules" \
    "! grep -r '\\bcalloc(' src/cognitive/emotion* src/cognitive/grief src/cognitive/joy src/cognitive/remorse src/cognitive/empathetic_response 2>/dev/null | grep -v nimcp_calloc"
check "No free in emotion modules" \
    "! grep -r '\\bfree(' src/cognitive/emotion* src/cognitive/grief src/cognitive/joy src/cognitive/remorse src/cognitive/empathetic_response 2>/dev/null | grep -v nimcp_free | grep -v 'free_list' | grep -v 'carefree'"
echo ""

# 6. Check for duplicate defines
echo "6. No Duplicate Defines"
echo "-----------------------"
check "Emotional System no duplicates" \
    "[ \$(grep -c '^#define LOG_MODULE' src/cognitive/emotions/nimcp_emotional_system.c) -eq 1 ]"
check "Grief no duplicates" \
    "[ \$(grep -c '^#define LOG_MODULE' src/cognitive/grief/nimcp_grief_and_loss.c) -eq 1 ]"
check "Joy no duplicates" \
    "[ \$(grep -c '^#define LOG_MODULE' src/cognitive/joy/nimcp_joy_euphoria.c) -eq 1 ]"
check "Remorse no duplicates" \
    "[ \$(grep -c '^#define LOG_MODULE' src/cognitive/remorse/nimcp_remorse_regret.c) -eq 1 ]"
check "Empathy no duplicates" \
    "[ \$(grep -c '^#define LOG_MODULE' src/cognitive/empathetic_response/nimcp_empathetic_response.c) -eq 1 ]"
echo ""

# Summary
echo "========================================="
echo "  Summary"
echo "========================================="
echo "PASSED: $PASS"
echo "FAILED: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✓ All checks passed! Integration successful."
    exit 0
else
    echo "✗ Some checks failed. Review the output above."
    exit 1
fi

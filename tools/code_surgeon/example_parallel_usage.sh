#!/bin/bash
#
# Example Usage - Code Surgeon Parallel Execution
#
# This script demonstrates how to use the parallel execution feature
# to generate tests for multiple source files simultaneously.

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

NIMCP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CODE_SURGEON="${NIMCP_ROOT}/tools/code_surgeon/code_surgeon.py"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}║ Code Surgeon - Parallel Execution Examples${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════${NC}"

#==============================================================================
# Example 1: Basic Parallel Test Generation (3 files, 3 workers)
#==============================================================================

example_1() {
    echo -e "\n${GREEN}[Example 1] Basic Parallel Test Generation${NC}"
    echo "Generate tests for 3 core files in parallel"
    echo ""

    python3 "${CODE_SURGEON}" \
        --parallel 3 \
        --target-files \
            src/networking/p2p/nimcp_p2pnode.c \
            src/security/nimcp_security.c \
            src/api/nimcp.c

    echo -e "\n${GREEN}✓ Example 1 completed${NC}"
}

#==============================================================================
# Example 2: High Parallelism (8 workers for multiple files)
#==============================================================================

example_2() {
    echo -e "\n${GREEN}[Example 2] High Parallelism Test Generation${NC}"
    echo "Generate tests for 8 files using 8 parallel workers"
    echo ""

    python3 "${CODE_SURGEON}" \
        --parallel 8 \
        --target-files \
            src/networking/p2p/nimcp_p2pnode.c \
            src/security/nimcp_security.c \
            src/api/nimcp.c \
            src/core/brain/nimcp_distributed_cow.c \
            src/utils/memory/nimcp_memory.c \
            src/utils/logging/nimcp_logging.c \
            src/cognitive/ethics/nimcp_ethics.c \
            src/plasticity/stdp/nimcp_stdp.c

    echo -e "\n${GREEN}✓ Example 2 completed${NC}"
}

#==============================================================================
# Example 3: Debug Mode (verbose output)
#==============================================================================

example_3() {
    echo -e "\n${GREEN}[Example 3] Debug Mode Parallel Execution${NC}"
    echo "Run with debug output to see worker activity"
    echo ""

    python3 "${CODE_SURGEON}" \
        --parallel 2 \
        --target-files \
            src/networking/p2p/nimcp_p2pnode.c \
            src/security/nimcp_security.c \
        --debug

    echo -e "\n${GREEN}✓ Example 3 completed${NC}"
}

#==============================================================================
# Example 4: Auto-detect CPU count
#==============================================================================

example_4() {
    echo -e "\n${GREEN}[Example 4] Auto-Detect Worker Count${NC}"
    echo "Let Code Surgeon auto-detect optimal worker count"
    echo ""

    # Using --parallel 0 triggers auto-detection (uses CPU count)
    python3 "${CODE_SURGEON}" \
        --parallel 0 \
        --target-files \
            src/networking/p2p/nimcp_p2pnode.c \
            src/security/nimcp_security.c \
            src/api/nimcp.c

    echo -e "\n${GREEN}✓ Example 4 completed${NC}"
}

#==============================================================================
# Example 5: Compare Serial vs Parallel Performance
#==============================================================================

example_5() {
    echo -e "\n${GREEN}[Example 5] Performance Comparison${NC}"
    echo "Compare serial vs parallel execution time"
    echo ""

    TARGET_FILES=(
        "src/networking/p2p/nimcp_p2pnode.c"
        "src/security/nimcp_security.c"
        "src/api/nimcp.c"
    )

    # Serial execution (baseline)
    echo -e "${YELLOW}Running SERIAL mode...${NC}"
    SERIAL_START=$(date +%s)

    for file in "${TARGET_FILES[@]}"; do
        echo "Processing $file..."
        # Would run serial Code Surgeon here
        sleep 5  # Simulate 5 seconds per file
    done

    SERIAL_END=$(date +%s)
    SERIAL_TIME=$((SERIAL_END - SERIAL_START))

    echo -e "${BLUE}Serial time: ${SERIAL_TIME}s${NC}"

    # Parallel execution
    echo -e "\n${YELLOW}Running PARALLEL mode (3 workers)...${NC}"
    PARALLEL_START=$(date +%s)

    python3 "${CODE_SURGEON}" \
        --parallel 3 \
        --target-files "${TARGET_FILES[@]}"

    PARALLEL_END=$(date +%s)
    PARALLEL_TIME=$((PARALLEL_END - PARALLEL_START))

    echo -e "${BLUE}Parallel time: ${PARALLEL_TIME}s${NC}"

    # Calculate speedup
    if [ $PARALLEL_TIME -gt 0 ]; then
        SPEEDUP=$(echo "scale=2; $SERIAL_TIME / $PARALLEL_TIME" | bc)
        echo -e "\n${GREEN}Speedup: ${SPEEDUP}x${NC}"
    fi

    echo -e "\n${GREEN}✓ Example 5 completed${NC}"
}

#==============================================================================
# Example 6: Python API Usage
#==============================================================================

example_6() {
    echo -e "\n${GREEN}[Example 6] Python API Usage${NC}"
    echo "Use Code Surgeon parallel API programmatically"
    echo ""

    python3 << 'EOF'
import sys
from pathlib import Path

# Add Code Surgeon to path
sys.path.insert(0, str(Path.cwd() / "tools" / "code_surgeon"))

from parallel_executor import execute_parallel_test_generation
from result_aggregator import aggregate_results, generate_text_report

# Execute parallel test generation
nimcp_root = Path.cwd()
target_files = (
    "src/networking/p2p/nimcp_p2pnode.c",
    "src/security/nimcp_security.c",
    "src/api/nimcp.c"
)

print("Executing parallel test generation...")
results = execute_parallel_test_generation(
    target_files=target_files,
    nimcp_root=nimcp_root,
    num_workers=3,
    debug_mode=True
)

# Aggregate and display results
report = aggregate_results(results)
print("\n" + generate_text_report(report))

print(f"\nGenerated {report.metrics.total_tests_created} tests")
print(f"Coverage: {report.coverage.line_coverage_percent:.1f}%")
EOF

    echo -e "\n${GREEN}✓ Example 6 completed${NC}"
}

#==============================================================================
# Example 7: Generate Tests for Entire Module
#==============================================================================

example_7() {
    echo -e "\n${GREEN}[Example 7] Module-Wide Test Generation${NC}"
    echo "Generate tests for all files in networking module"
    echo ""

    # Find all .c files in networking directory
    NETWORKING_FILES=$(find src/networking -name "*.c" -type f | head -10)

    if [ -z "$NETWORKING_FILES" ]; then
        echo -e "${RED}No .c files found in src/networking${NC}"
        return 1
    fi

    # Count files
    FILE_COUNT=$(echo "$NETWORKING_FILES" | wc -l)
    echo "Found $FILE_COUNT files in networking module"

    # Use number of workers = min(file_count, 8)
    WORKERS=$((FILE_COUNT < 8 ? FILE_COUNT : 8))

    python3 "${CODE_SURGEON}" \
        --parallel "$WORKERS" \
        --target-files $NETWORKING_FILES

    echo -e "\n${GREEN}✓ Example 7 completed${NC}"
}

#==============================================================================
# Example 8: View Generated Reports
#==============================================================================

example_8() {
    echo -e "\n${GREEN}[Example 8] View Generated Reports${NC}"
    echo "Explore the generated reports from parallel execution"
    echo ""

    REPORT_DIR="${NIMCP_ROOT}/.code_surgeon/reports"

    if [ ! -d "$REPORT_DIR" ]; then
        echo -e "${RED}No reports found. Run a parallel execution first.${NC}"
        return 1
    fi

    echo "Reports directory: $REPORT_DIR"
    echo ""

    # List recent reports
    echo "Recent reports:"
    ls -lht "$REPORT_DIR" | head -10

    echo ""
    echo "Report formats available:"
    echo "  - Text: report_*.txt (console-friendly)"
    echo "  - JSON: report_*.json (machine-readable)"
    echo "  - HTML: report_*.html (interactive, open in browser)"

    # Find most recent HTML report
    LATEST_HTML=$(ls -t "$REPORT_DIR"/*.html 2>/dev/null | head -1)

    if [ -n "$LATEST_HTML" ]; then
        echo ""
        echo -e "${YELLOW}Latest HTML report: $LATEST_HTML${NC}"
        echo "Open in browser: file://$LATEST_HTML"

        # Try to open in browser (Linux)
        if command -v xdg-open &> /dev/null; then
            read -p "Open in browser? (y/n) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                xdg-open "$LATEST_HTML"
            fi
        fi
    fi

    echo -e "\n${GREEN}✓ Example 8 completed${NC}"
}

#==============================================================================
# Main Menu
#==============================================================================

show_menu() {
    echo ""
    echo -e "${BLUE}Available Examples:${NC}"
    echo "  1) Basic Parallel Test Generation (3 workers)"
    echo "  2) High Parallelism (8 workers)"
    echo "  3) Debug Mode (verbose output)"
    echo "  4) Auto-Detect Worker Count"
    echo "  5) Performance Comparison (Serial vs Parallel)"
    echo "  6) Python API Usage"
    echo "  7) Module-Wide Test Generation"
    echo "  8) View Generated Reports"
    echo "  9) Run All Examples"
    echo "  0) Exit"
    echo ""
}

run_example() {
    case $1 in
        1) example_1 ;;
        2) example_2 ;;
        3) example_3 ;;
        4) example_4 ;;
        5) example_5 ;;
        6) example_6 ;;
        7) example_7 ;;
        8) example_8 ;;
        9)
            echo -e "${YELLOW}Running all examples...${NC}"
            example_1
            example_2
            example_3
            example_4
            example_5
            example_6
            example_7
            example_8
            echo -e "\n${GREEN}All examples completed!${NC}"
            ;;
        0)
            echo "Exiting..."
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid option${NC}"
            ;;
    esac
}

# Check if running in NIMCP root
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo -e "${RED}Error: Must run from NIMCP root directory${NC}"
    echo "Current directory: $(pwd)"
    exit 1
fi

# Check if Code Surgeon exists
if [ ! -f "$CODE_SURGEON" ]; then
    echo -e "${RED}Error: Code Surgeon not found at $CODE_SURGEON${NC}"
    exit 1
fi

# Interactive mode if no arguments
if [ $# -eq 0 ]; then
    while true; do
        show_menu
        read -p "Select example (0-9): " choice
        run_example "$choice"
        echo ""
        read -p "Press Enter to continue..."
    done
else
    # Run specific example
    run_example "$1"
fi

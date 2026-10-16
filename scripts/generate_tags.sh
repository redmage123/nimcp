#!/bin/bash
# Generate ctags file for NIMCP codebase
# This enables fast symbol lookup when navigating code

set -e

cd "$(dirname "$0")/.."

echo "Generating ctags index for NIMCP codebase..."

# Check if ctags is installed
if ! command -v ctags &> /dev/null; then
    echo "ERROR: ctags not installed"
    echo "Install with: sudo apt-get install universal-ctags"
    echo "             or: sudo apt-get install exuberant-ctags"
    exit 1
fi

# Generate tags file
# --recurse: scan all subdirectories
# --exclude: skip build/coverage directories
# --languages: C, C++, Python
# --extra=+f: include filename tags
# --fields=+iaS: include inheritance, access, signature
ctags \
    --recurse \
    --exclude=build \
    --exclude=build-fuzz \
    --exclude=build_test \
    --exclude=coverage \
    --exclude=coverage_report* \
    --exclude=.venv \
    --exclude=_deps \
    --languages=C,C++,Python \
    --extra=+f \
    --fields=+iaS \
    --c-kinds=+px \
    --c++-kinds=+px \
    src/ include/ test/ examples/

echo "✅ Tags file generated: tags"
echo "   Use: grep -A2 '^symbol_name' tags to find definitions"
echo "   Or integrate with your editor (vim, emacs, vscode)"

# Generate cscope database for cross-referencing
if command -v cscope &> /dev/null; then
    echo "Generating cscope database..."
    find src include test examples -name '*.c' -o -name '*.cpp' -o -name '*.h' > cscope.files
    cscope -b -q -k
    echo "✅ Cscope database generated: cscope.out"
fi

echo ""
echo "Quick usage examples:"
echo "  Find definition:     grep '^brain_enable_astrocytes' tags"
echo "  Find struct:         grep '^brain_config_t' tags"
echo "  Find error code:     grep '^NIMCP_ERROR_MEMORY' tags"
echo "  All in file:         grep 'nimcp_brain.h' tags"

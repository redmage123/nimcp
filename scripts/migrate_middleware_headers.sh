#!/bin/bash
# Migrate middleware headers from src/ to include/

echo "=== NIMCP Middleware Header Migration ==="
echo "Moving public headers to include/middleware/"

# Create directory structure
mkdir -p include/middleware/{buffering,normalization,routing,events,patterns,pipeline,encoding,features}

# Move headers while preserving subdirectory structure
echo "Moving headers..."
find src/middleware -name "*.h" -type f | while read src_file; do
    # Get relative path from src/middleware/
    rel_path="${src_file#src/middleware/}"
    dest_file="include/middleware/$rel_path"

    echo "  $src_file -> $dest_file"
    cp "$src_file" "$dest_file"
done

echo ""
echo "=== Updating #include statements in source files ==="

# Update all .c files in src/middleware to use new include paths
find src/middleware -name "*.c" -type f | while read c_file; do
    echo "Updating $c_file"
    # Change local includes to use middleware/ prefix
    sed -i 's|#include "\([a-z_/]*\)nimcp_\([a-z_]*\)\.h"|#include "middleware/\1nimcp_\2.h"|g' "$c_file"
done

# Update test files
find test -name "*.cpp" -type f | grep middleware | while read test_file; do
    echo "Updating $test_file"
    sed -i 's|#include "../../../src/middleware/\([a-z_/]*\)nimcp_\([a-z_]*\)\.h"|#include "middleware/\1nimcp_\2.h"|g' "$test_file"
done

echo ""
echo "=== Migration complete! ==="
echo "Next steps:"
echo "1. Update src/middleware/CMakeLists.txt to add include_directories(../include)"
echo "2. Remove old .h files from src/middleware after verifying build"
echo "3. Run: cmake --build build && ctest"

#!/bin/bash
#
# Gemini MCP "Direct Node" Fixer
# Resolves the "sh: 1: {method:initialize...}: not found" error by bypassing npx.

set -e

REPO_ROOT="/home/bbrelin/nimcp"
SETTINGS_FILE="$REPO_ROOT/.gemini/settings.json"

echo "--- 🛠️ Gemini MCP Fixer ---"

# 1. Ensure packages are installed globally so we can find their paths
echo "1. Installing MCP servers globally to ensure stable paths..."
npm install -g @modelcontextprotocol/server-memory \
               @modelcontextprotocol/server-filesystem \
               @modelcontextprotocol/server-sequential-thinking

# 2. Function to find the absolute path of a node module's executable
get_server_path() {
    local package="$1"
    # Try to find the build/index.js or dist/index.js
    local path=$(npm root -g)/$package/dist/index.js
    
    if [ ! -f "$path" ]; then
        path=$(npm root -g)/$package/build/index.js
    fi
    
    if [ ! -f "$path" ]; then
        echo "Error: Could not locate executable for $package" >&2
        return 1
    fi
    echo "$path"
}

echo "2. Locating server executables..."
PATH_MEMORY=$(get_server_path "@modelcontextprotocol/server-memory")
PATH_FILESYSTEM=$(get_server_path "@modelcontextprotocol/server-filesystem")
PATH_THINKING=$(get_server_path "@modelcontextprotocol/server-sequential-thinking")

echo "   - Memory: $PATH_MEMORY"
echo "   - Filesystem: $PATH_FILESYSTEM"
echo "   - Thinking: $PATH_THINKING"

# 3. Re-write settings.json with DIRECT 'node' commands and timeouts
echo "3. Updating $SETTINGS_FILE..."

# Create backup
cp "$SETTINGS_FILE" "${SETTINGS_FILE}.bak"

cat > "$SETTINGS_FILE" << EOF
{
  "general": {
    "previewFeatures": true,
    "contextFileName": "GEMINI.md"
  },
  "mcpServers": {
    "memory": {
      "command": "node",
      "args": ["$PATH_MEMORY"],
      "trust": true,
      "timeout": 120000
    },
    "filesystem": {
      "command": "node",
      "args": ["$PATH_FILESYSTEM", "$REPO_ROOT"],
      "trust": true,
      "timeout": 120000
    },
    "sequential-thinking": {
      "command": "node",
      "args": ["$PATH_THINKING"],
      "trust": true,
      "timeout": 120000
    }
  }
}
EOF

echo "---------------------------------------------------"
echo "✅ Fix Complete!"
echo "   - Replaced 'npx' with direct 'node' commands."
echo "   - Increased timeouts to 120s."
echo "   - Settings file updated at: $SETTINGS_FILE"
echo ""
echo "👉 Try running 'gemini' now. The 'sh: 1' error should be gone."
echo "---------------------------------------------------"

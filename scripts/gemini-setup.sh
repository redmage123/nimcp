#!/bin/bash
#
# Project-Local Gemini CLI Configuration Script
# Sets up Gemini CLI for seamless agentic coding within a specific project directory.

set -e

# --- USER CONFIGURATION ---
REPO_ROOT="/home/bbrelin/nimcp"
MCP_SERVER_NAME="mcp-memory"
# Assuming your MCP Memory Server is globally available via 'npx' or a command like below.
# Adjust the 'command' and 'args' if your server is run differently (e.g., Python script, HTTP URL).
MCP_COMMAND="npx"
MCP_ARGS="-y @modelcontextprotocol/server-memory"
# --------------------------

echo "--- Gemini CLI Local Project Setup ---"

# 1. Basic Checks
if ! command -v gemini &> /dev/null
then
    echo "ERROR: Gemini CLI is not installed or not in PATH."
    echo "Please run 'npm install -g @google/gemini-cli@latest' first."
    exit 1
fi

if [ ! -d "$REPO_ROOT" ]; then
    echo "ERROR: Repository root directory not found at $REPO_ROOT"
    echo "Please create the directory or update the REPO_ROOT variable in the script."
    exit 1
fi

# 2. Create Project-Local Configuration Directory
PROJECT_CONFIG_DIR="$REPO_ROOT/.gemini"
mkdir -p "$PROJECT_CONFIG_DIR"
echo "Created project configuration directory: $PROJECT_CONFIG_DIR"

# 3. Create Project-Local Settings.json
PROJECT_SETTINGS_FILE="$PROJECT_CONFIG_DIR/settings.json"

echo "Creating local settings file: $PROJECT_SETTINGS_FILE"

# This JSON structure configures the project-specific behavior and MCP server.
cat > "$PROJECT_SETTINGS_FILE" << EOF
{
  "general": {
    "previewFeatures": true,
    "contextFileName": "GEMINI.md"
  },
  "mcpServers": {
    "$MCP_SERVER_NAME": {
      "command": "$MCP_COMMAND",
      "args": ["$MCP_ARGS"],
      "trust": true
    }
  }
}
EOF

# 4. Create Initial GEMINI.md for Context/Memory
PROJECT_CONTEXT_FILE="$REPO_ROOT/GEMINI.md"

echo "Creating initial project context file: $PROJECT_CONTEXT_FILE"

cat > "$PROJECT_CONTEXT_FILE" << EOF
# GEMINI CLI PROJECT CONTEXT & STYLE GUIDE

## Project Identity
- **Project Name:** Nimcp Project (Please update this)
- **Primary Goal:** Building a large-scale, terminal-first application.
- **Initial Context:** This code was originally generated using Claude Code, and its architecture uses modular design principles. We are migrating to Gemini for improved agentic reasoning and large context handling.

## Coding Standards & Style
- **Primary Language:** [e.g., Python, TypeScript, Rust] (Please specify)
- **Style Guide:** Follow PEP 8 (if Python) / Airbnb (if JavaScript). Prioritize clarity and minimal dependencies.
- **Refactoring Rule:** All refactors must maintain the current function signatures and pass existing tests.

## Available Tools (MCP)
- The 'mcp-memory' server is available for long-term state and knowledge retrieval.
- **Instruction to Gemini:** Use the \`mcp-memory\` tool whenever recalling complex project history, design decisions, or multi-step plan elements.
EOF

# 5. Final Instructions
echo "---------------------------------------------------------"
echo "Configuration Complete."
echo ""
echo "To begin working with Gemini on this project:"
echo "1. Change directory to your repo root: cd $REPO_ROOT"
echo "2. Launch Gemini CLI: gemini"
echo "3. Verify your MCP server is ready: /mcp"
echo "4. Edit $PROJECT_CONTEXT_FILE to include specific project details."
echo ""
echo "Gemini will now automatically use the large context window, enabled preview features, and the '$MCP_SERVER_NAME' server for advanced agent tasks."
echo "---------------------------------------------------------"

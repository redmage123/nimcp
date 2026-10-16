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
- **Instruction to Gemini:** Use the `mcp-memory` tool whenever recalling complex project history, design decisions, or multi-step plan elements.

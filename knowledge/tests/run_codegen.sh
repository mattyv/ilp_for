#!/bin/bash
# Wrapper script for code generation evaluation
# Handles virtual environment and working directory setup

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KNOWLEDGE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check for API key
if [ -z "$ANTHROPIC_API_KEY" ]; then
    echo "Error: ANTHROPIC_API_KEY environment variable not set"
    echo "Set it with: export ANTHROPIC_API_KEY=your_key_here"
    exit 1
fi

# Activate virtual environment
if [ -f "$KNOWLEDGE_DIR/.venv/bin/activate" ]; then
    source "$KNOWLEDGE_DIR/.venv/bin/activate"
else
    echo "Error: Virtual environment not found at $KNOWLEDGE_DIR/.venv"
    echo "Run setup.sh first"
    exit 1
fi

# Change to knowledge directory for proper imports
cd "$KNOWLEDGE_DIR"

# Run the evaluation script
python tests/evaluate_codegen.py "$@"

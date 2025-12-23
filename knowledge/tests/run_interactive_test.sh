#!/bin/bash
# Wrapper script for interactive code generation test
# Handles virtual environment and working directory setup

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KNOWLEDGE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

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

# Run the interactive test script
python tests/interactive_codegen_test.py "$@"

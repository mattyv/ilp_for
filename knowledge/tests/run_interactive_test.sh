#!/bin/bash
# Wrapper script for interactive code generation test
# Handles virtual environment and working directory setup

set -e

show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Interactive code generation test for RAG knowledge base.
Generates prompts with RAG context that can be tested manually or via API.

Options:
  -h, --help         Show this help message
  --test-id ID       Run specific test by ID (e.g., basic_001)
  --auto             Auto-call Claude API instead of copy/paste mode
  -v, --verbose      Show full prompt and response

Examples:
  $(basename "$0")                    # Run all tests interactively
  $(basename "$0") --test-id basic_001
  $(basename "$0") --auto --verbose   # Auto-run with full output
EOF
    exit 0
}

# Check for help flag first
case "$1" in
    -h|--help)
        show_help
        ;;
esac

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

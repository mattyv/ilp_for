#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "Setting up ilp_for knowledge base..."

# Create venv if it doesn't exist
if [ ! -d .venv ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv .venv
fi

# Activate venv
source .venv/bin/activate

# Install dependencies
echo "Installing dependencies..."
pip install -q --upgrade pip
pip install -q -r requirements.txt

# Build database
echo "Building knowledge database..."
python scripts/import_knowledge.py

# Setup Claude Code RAG hook
echo "Setting up Claude Code RAG hook..."
CLAUDE_HOOKS_DIR="../.claude/hooks"
mkdir -p "$CLAUDE_HOOKS_DIR"

# Remove old symlink/file if it exists
if [ -e "$CLAUDE_HOOKS_DIR/rag_query_hook.py" ]; then
    rm "$CLAUDE_HOOKS_DIR/rag_query_hook.py"
fi

# Create symlink to golden source
ln -s "../../knowledge/hooks/rag_query_hook.py" "$CLAUDE_HOOKS_DIR/rag_query_hook.py"
echo "Created symlink: .claude/hooks/rag_query_hook.py -> ../../knowledge/hooks/rag_query_hook.py"

echo ""
echo "Setup complete! Knowledge base ready at knowledge/db/"
echo "To query: cd knowledge/scripts && source ../.venv/bin/activate && python rag_query.py 'your question'"

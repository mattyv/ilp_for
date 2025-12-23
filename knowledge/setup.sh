#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "Setting up RAG knowledge base..."

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
CLAUDE_DIR="../.claude"
CLAUDE_HOOKS_DIR="$CLAUDE_DIR/hooks"
SETTINGS_FILE="$CLAUDE_DIR/settings.local.json"

mkdir -p "$CLAUDE_HOOKS_DIR"

# Remove old symlink/file if it exists
if [ -e "$CLAUDE_HOOKS_DIR/rag_query_hook.py" ]; then
    rm "$CLAUDE_HOOKS_DIR/rag_query_hook.py"
fi

# Create symlink to golden source
ln -s "../../knowledge/hooks/rag_query_hook.py" "$CLAUDE_HOOKS_DIR/rag_query_hook.py"
echo "Created symlink: .claude/hooks/rag_query_hook.py -> ../../knowledge/hooks/rag_query_hook.py"

# Register hook in settings.local.json
echo "Registering hook in Claude Code settings..."
python3 << 'PYTHON_SCRIPT'
import json
from pathlib import Path

settings_path = Path("../.claude/settings.local.json")

# Load existing settings or create default
if settings_path.exists():
    with open(settings_path) as f:
        settings = json.load(f)
else:
    settings = {"permissions": {"allow": [], "deny": [], "ask": []}}

# Add MCP tool permissions
if "permissions" not in settings:
    settings["permissions"] = {"allow": [], "deny": [], "ask": []}
if "allow" not in settings["permissions"]:
    settings["permissions"]["allow"] = []

mcp_perm = "mcp__ilp-knowledge__*"
if mcp_perm not in settings["permissions"]["allow"]:
    settings["permissions"]["allow"].append(mcp_perm)
    print(f"  Added MCP permission: {mcp_perm}")

# Add hooks configuration
settings["hooks"] = {
    "UserPromptSubmit": [
        {
            "matcher": "",
            "hooks": [
                {
                    "type": "command",
                    "command": "python3 .claude/hooks/rag_query_hook.py"
                }
            ]
        }
    ]
}

# Write back
with open(settings_path, "w") as f:
    json.dump(settings, f, indent=2)
    f.write("\n")

print("  Hook registered in settings.local.json")
PYTHON_SCRIPT

# Generate .mcp.json with absolute paths for this machine
MCP_CONFIG="../.mcp.json"
KNOWLEDGE_DIR="$(pwd)"
cat > "$MCP_CONFIG" << EOF
{
  "mcpServers": {
    "ilp-knowledge": {
      "command": "$KNOWLEDGE_DIR/.venv/bin/python",
      "args": ["$KNOWLEDGE_DIR/mcp_server.py"],
      "description": "ILP_FOR knowledge base - query documentation, examples, gotchas, performance info"
    }
  }
}
EOF
echo "Generated .mcp.json with paths for this machine"

echo ""
echo "Setup complete! Knowledge base ready at knowledge/db/"
echo "RAG hook will auto-query on relevant prompts."
echo "To query manually: cd knowledge/scripts && source ../.venv/bin/activate && python rag_query.py 'your question'"

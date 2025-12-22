#!/usr/bin/env python3
"""Hook to automatically query the RAG knowledge base when user asks about ilp_for."""
import json
import sys
import os
import re

# Read input from stdin
try:
    input_data = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(0)  # Silent exit on invalid input

prompt = input_data.get("prompt", "").lower()

# Keywords that suggest the user is asking about ilp_for
ILP_KEYWORDS = [
    r"\bilp_for\b",
    r"\bilp_",
    r"\bilp::",
    r"\bloop.*unroll",
    r"\bunroll.*loop",
    r"\bearly.?exit.*loop",
    r"\bloop.*break",
    r"\bloop.*return",
    r"\bloop.*continue",
]

# Check if this is an ilp_for related question
is_ilp_question = any(re.search(pattern, prompt) for pattern in ILP_KEYWORDS)

if not is_ilp_question:
    sys.exit(0)  # Not relevant, continue normally

# Query the RAG knowledge base
project_dir = os.environ.get("CLAUDE_PROJECT_DIR", "")
if not project_dir:
    sys.exit(0)

knowledge_dir = os.path.join(project_dir, "knowledge")
venv_python = os.path.join(knowledge_dir, ".venv", "bin", "python3")
query_script = os.path.join(knowledge_dir, "scripts", "rag_query.py")

if not os.path.exists(venv_python) or not os.path.exists(query_script):
    sys.exit(0)  # RAG not set up

import subprocess

try:
    result = subprocess.run(
        [venv_python, query_script, prompt, "--limit", "3", "--json"],
        capture_output=True,
        text=True,
        timeout=30,
        cwd=os.path.join(knowledge_dir, "scripts"),
    )

    if result.returncode == 0 and result.stdout.strip():
        results = json.loads(result.stdout)
        if results:
            # Format results as context for Claude
            context_lines = ["[RAG Knowledge Base Results for ilp_for]"]
            for r in results:
                content = r.get("content", "")
                category = r.get("_category", "unknown")
                symbols = r.get("related_symbols", [])
                context_lines.append(f"- [{category}] {content}")
                if symbols:
                    context_lines.append(f"  Symbols: {', '.join(symbols)}")
            context_lines.append("[End RAG Results]")

            # Output context that will be injected
            output = {
                "additionalContext": "\n".join(context_lines)
            }
            print(json.dumps(output))
except Exception:
    pass  # Silent failure - don't block the user

sys.exit(0)

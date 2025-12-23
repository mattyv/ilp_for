#!/usr/bin/env python3
"""Hook to automatically query the RAG knowledge base for library-related questions."""
import json
import sys
import os
import re

# Read input from stdin
try:
    input_data = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(0)  # Silent exit on invalid input

prompt = input_data.get("prompt", "")

# Get project directory to locate config
project_dir = os.environ.get("CLAUDE_PROJECT_DIR", "")
if not project_dir:
    sys.exit(0)

# Add scripts directory to path for config import
scripts_dir = os.path.join(project_dir, "knowledge", "scripts")
if scripts_dir not in sys.path:
    sys.path.insert(0, scripts_dir)

try:
    from config import HOOK_KEYWORDS, LIBRARY_DISPLAY_NAME
except ImportError:
    # Fallback if config can't be imported
    sys.exit(0)

# Pre-compile patterns for efficiency (case-insensitive)
COMPILED_PATTERNS = [re.compile(p, re.IGNORECASE) for p in HOOK_KEYWORDS]

# Check if this is a library-related question
is_relevant = any(p.search(prompt) for p in COMPILED_PATTERNS)

if not is_relevant:
    sys.exit(0)  # Not relevant, continue normally

# Query the RAG knowledge base
knowledge_dir = os.path.join(project_dir, "knowledge")
venv_python = os.path.join(knowledge_dir, ".venv", "bin", "python3")
query_script = os.path.join(knowledge_dir, "scripts", "rag_query.py")

if not os.path.exists(venv_python) or not os.path.exists(query_script):
    sys.exit(0)  # RAG not set up

import subprocess

try:
    result = subprocess.run(
        [venv_python, query_script, prompt, "--limit", "3", "--json", "--source", "hook"],
        capture_output=True,
        text=True,
        timeout=30,
        cwd=os.path.join(knowledge_dir, "scripts"),
    )

    if result.returncode == 0 and result.stdout.strip():
        results = json.loads(result.stdout)
        if results:
            # Compute relevance indicator
            top_score = results[0].get("_hybrid_score", 0) if results else 0
            result_count = len(results)
            if top_score > 0.015 or result_count >= 3:
                relevance = "HIGH"
            elif top_score > 0.010 or result_count >= 2:
                relevance = "MEDIUM"
            else:
                relevance = "LOW"

            # Format results as context
            context_lines = [
                f"[RAG Knowledge Base - AUTHORITATIVE SOURCE for {LIBRARY_DISPLAY_NAME}]",
                "Trust these results. Only explore codebase if results are empty or you need to modify code.",
                f"Results: {result_count} | Relevance: {relevance}",
                ""
            ]
            for r in results:
                content = r.get("content", "")
                category = r.get("_category", "unknown")
                symbols = r.get("related_symbols", [])
                context_lines.append(f"- [{category}] {content}")
                if symbols:
                    context_lines.append(f"  Symbols: {', '.join(symbols)}")
            context_lines.append("[End RAG Results]")

            # Output context that will be injected
            # Must use hookSpecificOutput wrapper for Claude Code
            output = {
                "hookSpecificOutput": {
                    "hookEventName": "UserPromptSubmit",
                    "additionalContext": "\n".join(context_lines)
                }
            }
            print(json.dumps(output))
except Exception:
    pass  # Silent failure - don't block the user

sys.exit(0)

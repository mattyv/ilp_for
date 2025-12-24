#!/usr/bin/env python3
"""
Manual code generation test - Prints prompts for Claude Code to respond to.

This script queries the RAG and formats prompts that you can paste into
Claude Code to manually test if the RAG provides sufficient context for
code generation.
"""

import json
import sys
from pathlib import Path

# Add parent directory (knowledge/) and lucidity/scripts to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))
sys.path.insert(0, str(Path(__file__).parent.parent / "lucidity" / "scripts"))
from config import DB_PATH, EMBEDDING_MODEL, CATEGORIES
from rag_query import query


def query_rag(task_description: str, top_k: int = 5) -> str:
    """Query RAG system and format results as context string."""
    results = query(task_description, category="all", limit=top_k)

    if not results:
        return "No relevant context found."

    context_parts = []
    for i, result in enumerate(results, 1):
        context_parts.append(f"[Context {i}]")
        context_parts.append(f"Category: {result.get('_category', 'unknown')}")
        if result.get('related_symbols'):
            symbols = result['related_symbols']
            if isinstance(symbols, list):
                context_parts.append(f"Symbols: {', '.join(symbols)}")
            else:
                context_parts.append(f"Symbols: {symbols}")
        context_parts.append(f"Content: {result['content']}")
        context_parts.append("")

    return "\n".join(context_parts)


def call_claude_cli(prompt: str) -> str:
    """Call Claude CLI with the prompt and return the response."""
    import subprocess

    try:
        # Call claude CLI tool in print mode with no tools to avoid project context
        # Pass prompt via stdin
        result = subprocess.run(
            ['claude', '--print', '--tools', ''],
            input=prompt,
            capture_output=True,
            text=True,
            timeout=60
        )

        # Debug: print both stdout and stderr
        if result.returncode != 0:
            error_msg = result.stderr if result.stderr else f"Command failed with exit code {result.returncode}"
            return f"Error calling claude CLI: {error_msg}\nStdout: {result.stdout[:200]}"

        # If stdout is empty, that's also an error
        if not result.stdout.strip():
            return f"Error: Claude CLI returned empty response. Exit code: {result.returncode}, Stderr: {result.stderr}"

        return result.stdout
    except FileNotFoundError:
        return "Error: 'claude' command not found. Make sure Claude CLI is installed."
    except subprocess.TimeoutExpired:
        return "Error: Claude CLI timed out after 60 seconds"
    except Exception as e:
        return f"Error calling claude CLI: {e}"


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate manual test prompts")
    parser.add_argument("--test-id", help="Specific test ID to run")
    parser.add_argument("--auto", action="store_true", help="Automatically call Claude API instead of copy/paste")
    parser.add_argument("--verbose", "-v", action="store_true", help="Show full prompt and response")
    args = parser.parse_args()

    # Load test cases
    test_file = Path(__file__).parent / "code_generation_evaluation.json"
    with open(test_file) as f:
        data = json.load(f)

    test_cases = data["test_cases"]

    # Filter by test ID if specified
    if args.test_id:
        test_cases = [tc for tc in test_cases if tc["id"] == args.test_id]
        if not test_cases:
            print(f"Error: Test ID '{args.test_id}' not found")
            sys.exit(1)

    for test_case in test_cases:
        print("\n" + "=" * 80)
        print(f"TEST: {test_case['id']} - {test_case['difficulty'].upper()}")
        print("=" * 80)

        # Query RAG
        print("\n[1/4] Querying RAG for context...")
        context = query_rag(test_case["task_description"])

        # Build the prompt
        prompt = f"""You are a C++ developer who MUST use ONLY the provided context below.
Do NOT use any prior knowledge about ILP_FOR. Follow the context exactly.
If the context doesn't provide enough information, say so - do not invent features.

CONTEXT:
{context}

TASK:
{test_case['task_description']}

Function signature:
{test_case['function_signature']}

Requirements:
- Write ONLY the function implementation
- Include necessary #include statements
- Use the exact function signature provided
- Follow the patterns shown in the context
- Return ONLY valid C++ code, no explanations

Generate the complete function implementation now:"""

        print("\n[2/4] RAG Context Retrieved:")
        print("-" * 80)
        print(context)
        print("-" * 80)

        if args.auto:
            # Automatically call Claude CLI
            if args.verbose:
                print("\n[3/4] Full Prompt Sent to Claude:")
                print("╔" + "=" * 78 + "╗")
                print(prompt)
                print("╚" + "=" * 78 + "╝")
                print("\nCalling Claude CLI...")
            else:
                print("\n[3/4] Calling Claude CLI...")

            response = call_claude_cli(prompt)

            if args.verbose:
                print("\n[4/4] Full Claude Response:")
            else:
                print("\n[4/4] Claude's Response:")
            print("╔" + "=" * 78 + "╗")
            print(response)
            print("╚" + "=" * 78 + "╝")

            # Check quality
            checks = test_case["evaluation_checks"]
            print("\n  Quality checks:")
            if "must_contain" in checks:
                for pattern in checks["must_contain"]:
                    status = "✓" if pattern in response else "✗"
                    print(f"    {status} Must contain '{pattern}': {'PASS' if pattern in response else 'FAIL'}")
            if "must_not_contain" in checks:
                for pattern in checks["must_not_contain"]:
                    status = "✓" if pattern not in response else "✗"
                    print(f"    {status} Must NOT contain '{pattern}': {'PASS' if pattern not in response else 'FAIL'}")
        else:
            # Manual copy/paste mode
            print("\n[3/4] COPY THIS PROMPT TO CLAUDE CODE:")
            print("╔" + "=" * 78 + "╗")
            print(prompt)
            print("╚" + "=" * 78 + "╝")

            print("\n[4/4] Expected Test Results:")
            print("-" * 80)
            for i, tc in enumerate(test_case["test_cases"], 1):
                print(f"  Test {i}: {tc}")

            checks = test_case["evaluation_checks"]
            print("\n  Quality checks:")
            if "must_contain" in checks:
                print(f"    ✓ Must contain: {checks['must_contain']}")
            if "must_not_contain" in checks:
                print(f"    ✗ Must NOT contain: {checks['must_not_contain']}")

        print("-" * 80)

        # Add spacing between tests
        if len(test_cases) > 1:
            print("\n\n")


if __name__ == "__main__":
    main()

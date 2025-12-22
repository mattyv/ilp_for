#!/usr/bin/env python3
"""Parse library source and extract API signatures as ground truth.

This script is customizable via ingest_config.py for different libraries.
"""
import re
from pathlib import Path

from config import REPO_ROOT


def load_ingest_config():
    """Load library-specific ingestion configuration."""
    try:
        from ingest_config import INGEST_CONFIG
        return INGEST_CONFIG
    except ImportError:
        print("Warning: No ingest_config.py found, using defaults")
        return {
            "files": [],
            "macro_patterns": [],
            "function_patterns": [],
        }


def extract_by_pattern(content: str, patterns: list[dict]) -> list[dict]:
    """Extract matches using configured patterns."""
    results = []
    for pat_config in patterns:
        pattern = pat_config["pattern"]
        name_group = pat_config.get("name_group", 1)
        sig_group = pat_config.get("signature_group", 0)
        item_type = pat_config.get("type", "unknown")
        prefix_filter = pat_config.get("prefix_filter", None)

        for match in re.finditer(pattern, content):
            name = match.group(name_group)
            if prefix_filter and not name.startswith(prefix_filter):
                continue

            signature = match.group(sig_group) if sig_group else match.group(0)
            results.append({
                "name": name,
                "signature": signature.strip(),
                "type": item_type
            })
    return results


def ingest():
    config = load_ingest_config()

    if not config.get("files"):
        print("No files configured for ingestion in ingest_config.py")
        return

    all_macros = []
    all_functions = []

    for file_config in config["files"]:
        file_path = REPO_ROOT / file_config["path"]
        if not file_path.exists():
            print(f"Warning: {file_path} not found, skipping")
            continue

        content = file_path.read_text()

        # Extract macros
        if config.get("macro_patterns"):
            macros = extract_by_pattern(content, config["macro_patterns"])
            all_macros.extend(macros)

        # Extract functions
        if config.get("function_patterns"):
            functions = extract_by_pattern(content, config["function_patterns"])
            all_functions.extend(functions)

    print(f"Found {len(all_macros)} macros:")
    for m in all_macros:
        print(f"  {m['signature']}")

    print(f"\nFound {len(all_functions)} functions:")
    for f in all_functions:
        sig = f['signature']
        if len(sig) > 80:
            sig = sig[:80] + "..."
        print(f"  {sig}")

    # Add to database
    from rag_add import add_insight

    for m in all_macros:
        add_insight(
            content=f"Macro: {m['signature']}",
            category="api_signatures",
            symbols=[m['name']],
            tags=["macro", "api"],
            source="code",
            validated_by="parsed",
            confidence=1.0
        )

    for f in all_functions:
        add_insight(
            content=f"Function: {f['signature']}",
            category="api_signatures",
            symbols=[f['name']],
            tags=["function", "api"],
            source="code",
            validated_by="parsed",
            confidence=1.0
        )

    print(f"\nIngested {len(all_macros) + len(all_functions)} API signatures")


if __name__ == "__main__":
    ingest()

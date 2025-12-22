#!/usr/bin/env python3
"""Configuration for the RAG knowledge system.

Edit this file to customize for your library.
"""
import os
from pathlib import Path

# Database path (relative to this script)
DB_PATH = os.path.join(os.path.dirname(__file__), "..", "db")

# Repository root (two levels up from scripts/)
REPO_ROOT = Path(__file__).parent.parent.parent

# Embedding model - good balance of speed and quality for code
EMBEDDING_MODEL = 'BAAI/bge-small-en-v1.5'

# Knowledge categories - customize as needed for your library
CATEGORIES = [
    "api_signatures",   # Exact function/macro signatures
    "usage_patterns",   # Correct usage examples
    "antipatterns",     # What NOT to do
    "gotchas",          # Subtle issues/edge cases
    "performance",      # When to use, benchmarks
    "compiler",         # Compiler-specific behavior
    "semantics",        # What things actually do/expand to
]

# Valid sources for insights
VALID_SOURCES = ["docs", "code", "conversation", "test"]

# Valid validators
VALID_VALIDATORS = ["human", "compiles", "test_passes", "parsed", "system"]

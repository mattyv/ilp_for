#!/usr/bin/env python3
"""Configuration for the RAG knowledge system.

Edit this file to customize for your library.
"""
import os
from pathlib import Path

# =============================================================================
# Library Identity (customize for your project)
# =============================================================================
LIBRARY_NAME = "ilp_for"
LIBRARY_DISPLAY_NAME = "ILP_FOR"

# =============================================================================
# Paths
# =============================================================================
# Database path (relative to this script)
DB_PATH = os.path.join(os.path.dirname(__file__), "..", "db")

# Repository root (two levels up from scripts/)
REPO_ROOT = Path(__file__).parent.parent.parent

# =============================================================================
# Models
# =============================================================================
# Embedding model - good balance of speed and quality for code
EMBEDDING_MODEL = 'BAAI/bge-small-en-v1.5'
EMBEDDING_MODEL_REVISION = None  # Set to commit hash for reproducibility

# Cross-encoder reranking model
RERANK_MODEL = "cross-encoder/ms-marco-MiniLM-L-6-v2"

# Claude API model for code generation evaluation
CODEGEN_EVAL_MODEL = "claude-sonnet-4-5-20250929"

# =============================================================================
# Hook Configuration
# =============================================================================
# Regex patterns that trigger RAG queries (case-insensitive)
HOOK_KEYWORDS = [
    r"\bilp\b",
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

# =============================================================================
# Knowledge Categories
# =============================================================================
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
VALID_SOURCES = ["docs", "code", "conversation", "test", "implementation"]

# Valid validators
VALID_VALIDATORS = ["human", "compiles", "test_passes", "parsed", "system"]

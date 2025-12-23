#!/usr/bin/env python3
"""MCP server for ILP_FOR knowledge base queries."""
import sys
from pathlib import Path

# Add scripts to path for imports
sys.path.insert(0, str(Path(__file__).parent / "scripts"))

from mcp.server.fastmcp import FastMCP
from scripts.rag_query import query as rag_query

mcp = FastMCP("ilp-knowledge")


@mcp.tool()
def query_ilp_knowledge(
    query: str,
    category: str = "all",
    limit: int = 5,
    rerank: bool = True
) -> str:
    """
    Query the ILP_FOR knowledge base for documentation, examples, gotchas, and performance info.

    Use this to look up:
    - How to use ILP_FOR macros (syntax, examples)
    - Common mistakes and gotchas
    - Performance characteristics and benchmarks
    - LoopType selection guidance
    - Compiler flags and options
    - Migration patterns from standard loops

    Args:
        query: Natural language search query (e.g., "how to use ILP_BREAK", "return type too large")
        category: Filter by category - "all", "semantics", "gotchas", "performance",
                  "usage_patterns", "compiler", "api_signatures", "antipatterns"
        limit: Number of results to return (default 5, max 15)
        rerank: Use cross-encoder reranking for better precision (default True)

    Returns:
        Formatted knowledge base results with content, category, and confidence scores
    """
    limit = min(limit, 15)  # Cap at 15

    results = rag_query(
        text=query,
        category=category,
        limit=limit,
        use_rerank=rerank,
        use_hybrid=True,
        source="mcp"
    )

    if not results:
        return f"No results found for query: {query}"

    output_parts = []
    for i, r in enumerate(results, 1):
        category_name = r.get('_category', 'unknown')
        confidence = r.get('confidence', 'N/A')
        symbols = r.get('related_symbols', [])
        content = r.get('content', '')
        tags = r.get('tags', [])

        entry = f"[{i}] [{category_name}] (confidence: {confidence})\n"
        if symbols:
            entry += f"    Symbols: {', '.join(symbols)}\n"
        if tags:
            entry += f"    Tags: {', '.join(tags)}\n"
        entry += f"    {content}"
        output_parts.append(entry)

    return "\n\n".join(output_parts)


@mcp.tool()
def list_ilp_categories() -> str:
    """
    List all available knowledge categories in the ILP_FOR knowledge base.

    Returns:
        List of category names with descriptions
    """
    categories = {
        "semantics": "How ILP_FOR works internally, iteration order, variable capture, lambda mechanics",
        "gotchas": "Common mistakes, pitfalls, and things that trip people up",
        "performance": "Benchmarks, speedup data, assembly analysis, optimization tips",
        "usage_patterns": "Examples, patterns, how-to guides, migration from standard loops",
        "compiler": "Compiler flags, error messages, debug modes, CPU targeting",
        "api_signatures": "Complete list of macros, LoopType enum values, function signatures",
        "antipatterns": "When NOT to use ILP_FOR, misuse cases"
    }

    output = "ILP_FOR Knowledge Base Categories:\n\n"
    for name, desc in categories.items():
        output += f"  {name}: {desc}\n"

    return output


if __name__ == "__main__":
    mcp.run()

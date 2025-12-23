#!/usr/bin/env python3
"""CLI for adding validated insights to the knowledge base."""
import lancedb
from sentence_transformers import SentenceTransformer
import argparse
from datetime import datetime

from config import DB_PATH, EMBEDDING_MODEL, CATEGORIES, VALID_SOURCES

# Lazy-loaded model to avoid reloading on repeated calls
_model = None


def get_model():
    """Lazy load embedding model."""
    global _model
    if _model is None:
        _model = SentenceTransformer(EMBEDDING_MODEL)
    return _model


def add_insight(content: str, category: str, symbols: list[str],
                tags: list[str], source: str = "conversation",
                validated_by: str = "human", confidence: float = 0.9):
    """Add an insight to the knowledge base.

    Args:
        content: The insight text
        category: Category for the insight (must be in CATEGORIES)
        symbols: Related API symbols
        tags: Tags for categorization
        source: Source of the insight
        validated_by: How the insight was validated
        confidence: Confidence score 0.0-1.0
    """
    db = lancedb.connect(DB_PATH)
    model = get_model()

    table = db.open_table(category)
    table.add([{
        "content": content,
        "vector": model.encode(content).tolist(),
        "source": source,
        "validated_by": validated_by,
        "confidence": confidence,
        "related_symbols": symbols,
        "tags": tags,
        "created_at": datetime.now().isoformat()
    }])

    # Rebuild FTS index to include new entry
    try:
        table.create_fts_index("content", replace=True)
    except Exception:
        pass  # FTS update is best-effort

    print(f"Added insight to '{category}'")
    print(f"  Symbols: {symbols}")
    content_preview = content[:100] + "..." if len(content) > 100 else content
    print(f"  Content: {content_preview}")


def main():
    parser = argparse.ArgumentParser(description='Add validated insight')
    parser.add_argument('--category', '-c', required=True,
                        choices=CATEGORIES,
                        help='Category for the insight')
    parser.add_argument('--content', required=True, help='The insight content')
    parser.add_argument('--symbols', '-s', nargs='+', default=[],
                        help='Related symbols (e.g., API names, macros)')
    parser.add_argument('--tags', '-t', nargs='+', default=[],
                        help='Tags for categorization')
    parser.add_argument('--source', default='conversation',
                        choices=VALID_SOURCES,
                        help='Source of the insight')
    parser.add_argument('--confidence', type=float, default=0.9,
                        help='Confidence score 0.0-1.0')
    args = parser.parse_args()

    # Validate confidence range
    if not 0.0 <= args.confidence <= 1.0:
        parser.error("Confidence must be between 0.0 and 1.0")

    add_insight(args.content, args.category, args.symbols, args.tags,
                args.source, "human", args.confidence)


if __name__ == "__main__":
    main()

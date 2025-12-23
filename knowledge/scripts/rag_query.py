#!/usr/bin/env python3
"""CLI for querying the RAG knowledge base."""
import lancedb
from sentence_transformers import SentenceTransformer
import argparse
import json
import sys
from pathlib import Path

# Add scripts directory to path if not already there
if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).parent))

from config import DB_PATH, EMBEDDING_MODEL


def query(text: str, category: str = "all", limit: int = 5):
    db = lancedb.connect(DB_PATH)
    model = SentenceTransformer(EMBEDDING_MODEL)
    embedding = model.encode(text).tolist()

    results = []
    if category != "all":
        tables = [category]
    else:
        tables_response = db.list_tables()
        tables = tables_response.tables if hasattr(tables_response, 'tables') else tables_response

    for table_name in tables:
        try:
            table = db.open_table(table_name)
            hits = table.search(embedding).limit(limit).to_list()
            for hit in hits:
                hit['_category'] = table_name
                results.append(hit)
        except Exception:
            continue

    # Sort by distance
    results.sort(key=lambda x: x.get('_distance', float('inf')))
    return results[:limit]


def main():
    parser = argparse.ArgumentParser(description='Query knowledge base')
    parser.add_argument('query', type=str, help='Search query')
    parser.add_argument('--category', '-c', default='all',
                        help='Category to search (default: all)')
    parser.add_argument('--limit', '-n', type=int, default=5,
                        help='Number of results (default: 5)')
    parser.add_argument('--json', '-j', action='store_true',
                        help='Output as JSON')
    args = parser.parse_args()

    results = query(args.query, args.category, args.limit)

    if args.json:
        print(json.dumps(results, indent=2, default=str))
    else:
        for i, r in enumerate(results, 1):
            print(f"\n{'='*60}")
            print(f"[{i}] Category: {r['_category']} | Confidence: {r.get('confidence', 'N/A')}")
            print(f"    Symbols: {r.get('related_symbols', [])}")
            content = r['content']
            if len(content) > 500:
                content = content[:500] + "..."
            print(f"    Content: {content}")


if __name__ == "__main__":
    main()

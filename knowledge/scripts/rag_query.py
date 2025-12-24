#!/usr/bin/env python3
"""CLI for querying the RAG knowledge base with hybrid search and reranking."""
import lancedb
from sentence_transformers import SentenceTransformer, CrossEncoder
import argparse
import json
import sys
import threading
from pathlib import Path
from datetime import datetime

# Add scripts directory to path if not already there
if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).parent))

from config import DB_PATH, EMBEDDING_MODEL, RERANK_MODEL

# Paths
LOG_PATH = Path(__file__).parent.parent / "logs" / "queries.jsonl"

# Lazy-loaded models with thread safety
_embedding_model = None
_rerank_model = None
_model_lock = threading.Lock()

# Log directory creation flag
_log_dir_created = False

# RRF fallback rank for results not found in a search type
RRF_FALLBACK_RANK = 1000


def get_embedding_model():
    """Lazy load embedding model (thread-safe)."""
    global _embedding_model
    if _embedding_model is None:
        with _model_lock:
            if _embedding_model is None:  # Double-check after acquiring lock
                _embedding_model = SentenceTransformer(EMBEDDING_MODEL)
    return _embedding_model


def get_rerank_model():
    """Lazy load cross-encoder reranking model (thread-safe)."""
    global _rerank_model
    if _rerank_model is None:
        with _model_lock:
            if _rerank_model is None:  # Double-check after acquiring lock
                _rerank_model = CrossEncoder(RERANK_MODEL)
    return _rerank_model


def log_query(query_text: str, results: list, source: str = "cli"):
    """Log query to JSONL file for feedback loop."""
    global _log_dir_created
    try:
        if not _log_dir_created:
            LOG_PATH.parent.mkdir(exist_ok=True)
            _log_dir_created = True
        entry = {
            "timestamp": datetime.now().isoformat(),
            "query": query_text,
            "source": source,
            "result_count": len(results),
            "top_categories": [r.get('_category', 'unknown') for r in results[:3]],
            "top_distances": [round(r.get('_distance', 0), 4) for r in results[:3]]
        }
        with open(LOG_PATH, "a") as f:
            f.write(json.dumps(entry) + "\n")
    except Exception:
        pass  # Silent fail - logging shouldn't break queries


def rerank(query_text: str, results: list, top_k: int = 5) -> list:
    """Rerank results using cross-encoder for better precision."""
    if not results:
        return results

    reranker = get_rerank_model()
    # Defensive access in case content is missing
    pairs = [(query_text, r.get('content', '')) for r in results]
    scores = reranker.predict(pairs)

    for r, score in zip(results, scores):
        r['_rerank_score'] = float(score)

    return sorted(results, key=lambda x: x['_rerank_score'], reverse=True)[:top_k]


def hybrid_search(table, query_text: str, embedding: list, limit: int, hybrid_weight: float = 0.7) -> list:
    """
    Perform hybrid search combining vector similarity and full-text search.

    Args:
        table: LanceDB table
        query_text: Raw query string for FTS
        embedding: Query embedding vector
        limit: Number of results to return
        hybrid_weight: Weight for vector search (0-1). 1.0 = pure vector, 0.0 = pure FTS

    Returns:
        List of results with blended scores
    """
    results_map = {}

    # Vector search
    try:
        vector_hits = table.search(embedding).limit(limit * 2).to_list()
        for i, hit in enumerate(vector_hits):
            content = hit.get('content', '')
            if content not in results_map:
                results_map[content] = hit.copy()
                results_map[content]['_vector_rank'] = i
            else:
                # Keep best (lowest) rank if duplicate
                results_map[content]['_vector_rank'] = min(
                    i, results_map[content].get('_vector_rank', i)
                )
    except Exception:
        pass

    # Full-text search on searchable_content (if FTS index exists)
    try:
        fts_hits = table.search(query_text, query_type="fts").limit(limit * 2).to_list()
        for i, hit in enumerate(fts_hits):
            content = hit.get('content', '')
            if content not in results_map:
                results_map[content] = hit.copy()
                results_map[content]['_fts_rank'] = i
            else:
                # Keep best (lowest) rank if duplicate
                results_map[content]['_fts_rank'] = min(
                    i, results_map[content].get('_fts_rank', i)
                )
    except Exception:
        # FTS index may not exist - fall back to vector-only
        pass

    # Compute hybrid scores using reciprocal rank fusion
    results = []
    for content, hit in results_map.items():
        # Use constant fallback rank for results not found in a search type
        vector_rank = hit.get('_vector_rank', RRF_FALLBACK_RANK)
        fts_rank = hit.get('_fts_rank', RRF_FALLBACK_RANK)

        # Reciprocal Rank Fusion: score = w1/(k+rank1) + w2/(k+rank2)
        k = 60  # Standard RRF constant
        vector_score = hybrid_weight / (k + vector_rank)
        fts_score = (1 - hybrid_weight) / (k + fts_rank)
        hit['_hybrid_score'] = vector_score + fts_score

        results.append(hit)

    # Sort by hybrid score (higher is better)
    results.sort(key=lambda x: x['_hybrid_score'], reverse=True)
    return results[:limit]


def query(
    text: str,
    category: str = "all",
    limit: int = 5,
    use_rerank: bool = False,
    use_hybrid: bool = True,
    hybrid_weight: float = 0.7,
    source: str = "cli"
):
    """
    Query the knowledge base.

    Args:
        text: Search query
        category: Category to search ("all" for all categories)
        limit: Number of results to return
        use_rerank: Whether to use cross-encoder reranking
        use_hybrid: Whether to use hybrid (vector + FTS) search
        hybrid_weight: Weight for vector search in hybrid mode (0-1)
        source: Source of query for logging ("cli", "hook", etc.)

    Returns:
        List of result dictionaries
    """
    db = lancedb.connect(DB_PATH)
    model = get_embedding_model()
    embedding = model.encode(text).tolist()

    results = []
    if category != "all":
        tables = [category]
    else:
        tables_response = db.list_tables()
        tables = tables_response.tables if hasattr(tables_response, 'tables') else tables_response

    # Fetch more results if reranking (reranker will trim to limit)
    fetch_limit = limit * 3 if use_rerank else limit

    for table_name in tables:
        try:
            table = db.open_table(table_name)

            if use_hybrid:
                hits = hybrid_search(table, text, embedding, fetch_limit, hybrid_weight)
            else:
                hits = table.search(embedding).limit(fetch_limit).to_list()

            for hit in hits:
                hit['_category'] = table_name
                results.append(hit)
        except Exception:
            continue

    # Sort by distance or hybrid score
    if use_hybrid:
        results.sort(key=lambda x: x.get('_hybrid_score', 0), reverse=True)
    else:
        results.sort(key=lambda x: x.get('_distance', float('inf')))

    results = results[:fetch_limit]

    # Rerank if requested
    if use_rerank and results:
        results = rerank(text, results, limit)
    else:
        results = results[:limit]

    # Log query
    log_query(text, results, source)

    return results


def main():
    parser = argparse.ArgumentParser(description='Query knowledge base')
    parser.add_argument('query', type=str, help='Search query')
    parser.add_argument('--category', '-c', default='all',
                        help='Category to search (default: all)')
    parser.add_argument('--limit', '-n', type=int, default=5,
                        help='Number of results (default: 5)')
    parser.add_argument('--json', '-j', action='store_true',
                        help='Output as JSON')
    parser.add_argument('--rerank', '-r', action='store_true',
                        help='Use cross-encoder reranking for better precision')
    parser.add_argument('--no-hybrid', action='store_true',
                        help='Disable hybrid search (use vector-only)')
    parser.add_argument('--hybrid-weight', type=float, default=0.7,
                        help='Weight for vector search in hybrid mode (0-1, default: 0.7)')
    parser.add_argument('--source', '-s', default='cli',
                        help='Source of query for logging (default: cli)')
    args = parser.parse_args()

    results = query(
        args.query,
        args.category,
        args.limit,
        use_rerank=args.rerank,
        use_hybrid=not args.no_hybrid,
        hybrid_weight=args.hybrid_weight,
        source=args.source
    )

    if args.json:
        print(json.dumps(results, indent=2, default=str))
    else:
        for i, r in enumerate(results, 1):
            print(f"\n{'='*60}")
            score_info = []
            if '_distance' in r:
                score_info.append(f"Dist: {r['_distance']:.4f}")
            if '_hybrid_score' in r:
                score_info.append(f"Hybrid: {r['_hybrid_score']:.4f}")
            if '_rerank_score' in r:
                score_info.append(f"Rerank: {r['_rerank_score']:.4f}")
            score_str = " | ".join(score_info) if score_info else "N/A"

            print(f"[{i}] Category: {r['_category']} | {score_str}")
            print(f"    Confidence: {r.get('confidence', 'N/A')} | Symbols: {r.get('related_symbols', [])}")
            content = r.get('content', '')
            if len(content) > 500:
                content = content[:500] + "..."
            print(f"    Content: {content}")


if __name__ == "__main__":
    main()

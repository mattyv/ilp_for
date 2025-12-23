#!/usr/bin/env python3
"""Custom RAG evaluation - tests retrieval quality without LLM inference."""
import sys
import json
from pathlib import Path
from datetime import datetime

# Add scripts to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'scripts'))

import lancedb
from sentence_transformers import SentenceTransformer
from config import CATEGORIES, EMBEDDING_MODEL

def load_test_cases():
    """Load test cases from rag_evaluation.json."""
    test_file = Path(__file__).parent / 'rag_evaluation.json'
    with open(test_file) as f:
        data = json.load(f)
    return data['test_cases']

def query_rag(question, db, model, top_k=5):
    """Query RAG and return results from all categories."""
    vector = model.encode(question).tolist()

    results = []
    for category in CATEGORIES:
        try:
            table = db.open_table(category)
            matches = table.search(vector).limit(top_k).to_list()
            for match in matches:
                results.append({
                    'category': category,
                    'content': match['content'],
                    'distance': match['_distance'],
                    'symbols': match.get('related_symbols', []),
                    'confidence': match.get('confidence', 0.0)
                })
        except Exception:
            # Table might be empty
            continue

    # Sort by distance (lower is better)
    results.sort(key=lambda x: x['distance'])
    return results[:top_k]

def evaluate_context_retrieval(retrieved, expected_categories, expected_symbols):
    """
    Evaluate if RAG retrieved relevant context.

    Returns:
        score: 0.0 (miss), 0.5 (partial), 1.0 (perfect)
        details: dict with diagnostic info
    """
    retrieved_categories = set(r['category'] for r in retrieved)
    retrieved_symbols = set()
    for r in retrieved:
        retrieved_symbols.update(r['symbols'])

    expected_cat_set = set(expected_categories)
    expected_sym_set = set(expected_symbols)

    # Category matching
    cat_overlap = len(retrieved_categories & expected_cat_set)
    cat_total = len(expected_cat_set)
    cat_score = cat_overlap / cat_total if cat_total > 0 else 1.0

    # Symbol matching
    sym_overlap = len(retrieved_symbols & expected_sym_set)
    sym_total = len(expected_sym_set)
    sym_score = sym_overlap / sym_total if sym_total > 0 else 1.0

    # Combined score (preserve granularity instead of quantizing)
    overall_score = (cat_score + sym_score) / 2

    # Classify for human readability
    if overall_score >= 0.8:
        rating = "perfect"
    elif overall_score >= 0.3:
        rating = "partial"
    else:
        rating = "miss"

    return overall_score, {
        'rating': rating,
        'overall_score': overall_score,
        'category_score': cat_score,
        'symbol_score': sym_score,
        'retrieved_categories': list(retrieved_categories),
        'expected_categories': expected_categories,
        'retrieved_symbols': list(retrieved_symbols),
        'expected_symbols': expected_symbols,
        'category_overlap': cat_overlap,
        'symbol_overlap': sym_overlap
    }

def run_evaluation(verbose=False):
    """Run evaluation on all test cases."""
    test_cases = load_test_cases()

    # Connect to database
    db_path = Path(__file__).parent.parent / 'db'
    if not db_path.exists():
        print("Error: Database not found. Run setup.sh first.")
        return 1

    db = lancedb.connect(str(db_path))

    # Load embedding model
    print("Loading embedding model...")
    model = SentenceTransformer(EMBEDDING_MODEL)

    print(f"\nRunning evaluation on {len(test_cases)} test cases...\n")

    results = []
    total_score = 0.0

    for i, test in enumerate(test_cases, 1):
        if verbose:
            print(f"[{i}/{len(test_cases)}] {test['id']}: {test['question']}")

        # Query RAG
        retrieved = query_rag(test['question'], db, model, top_k=5)

        # Evaluate retrieval
        score, details = evaluate_context_retrieval(
            retrieved,
            test['expected_rag_categories'],
            test['expected_symbols']
        )

        total_score += score

        result = {
            'id': test['id'],
            'category': test['category'],
            'difficulty': test['difficulty'],
            'question': test['question'],
            'score': score,
            'details': details,
            'top_retrieved': [
                {
                    'category': r['category'],
                    'content': r['content'][:100] + '...' if len(r['content']) > 100 else r['content'],
                    'distance': r['distance']
                }
                for r in retrieved[:3]
            ]
        }
        results.append(result)

        if verbose:
            print(f"  Score: {score:.1f} ({details['rating']})")
            print(f"  Retrieved categories: {details['retrieved_categories']}")
            print(f"  Expected categories: {details['expected_categories']}")
            if score < 1.0:
                print(f"  ⚠️  Missing: categories={set(details['expected_categories']) - set(details['retrieved_categories'])}, "
                      f"symbols={set(details['expected_symbols']) - set(details['retrieved_symbols'])}")
            print()

    # Summary
    avg_score = total_score / len(test_cases)
    pass_rate = sum(1 for r in results if r['score'] >= 0.5) / len(test_cases)
    perfect_rate = sum(1 for r in results if r['score'] == 1.0) / len(test_cases)

    print("=" * 60)
    print("EVALUATION SUMMARY")
    print("=" * 60)
    print(f"Total test cases: {len(test_cases)}")
    print(f"Average score: {avg_score:.2f} / 1.0")
    print(f"Pass rate (≥0.5): {pass_rate:.1%}")
    print(f"Perfect rate (1.0): {perfect_rate:.1%}")
    print()

    # Breakdown by difficulty
    for difficulty in ['easy', 'medium', 'hard']:
        difficulty_results = [r for r in results if r['difficulty'] == difficulty]
        if difficulty_results:
            difficulty_avg = sum(r['score'] for r in difficulty_results) / len(difficulty_results)
            print(f"{difficulty.capitalize()}: {difficulty_avg:.2f} ({len(difficulty_results)} tests)")

    print()

    # Failed tests
    failed = [r for r in results if r['score'] < 1.0]
    if failed:
        print(f"⚠️  {len(failed)} tests did not achieve perfect retrieval:")
        for r in failed:
            print(f"  - {r['id']}: {r['details']['rating']} (score={r['score']:.1f})")
    else:
        print("✓ All tests achieved perfect retrieval!")

    # Save results
    results_dir = Path(__file__).parent / 'results'
    results_dir.mkdir(exist_ok=True)

    timestamp = datetime.now().isoformat()
    output_file = results_dir / f"custom_eval_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"

    output = {
        'timestamp': timestamp,
        'test_count': len(test_cases),
        'average_score': avg_score,
        'pass_rate': pass_rate,
        'perfect_rate': perfect_rate,
        'results': results
    }

    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"\nResults saved to {output_file}")

    return 0 if avg_score >= 0.8 else 1

if __name__ == '__main__':
    verbose = '--verbose' in sys.argv or '-v' in sys.argv
    exit(run_evaluation(verbose=verbose))

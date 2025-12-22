#!/usr/bin/env python3
"""RAGAS evaluation - industry-standard RAG metrics using LLM inference.

Requires:
  pip install ragas anthropic

Set ANTHROPIC_API_KEY environment variable before running.
"""
import sys
import json
import os
from pathlib import Path
from datetime import datetime

try:
    from ragas import evaluate
    from ragas.metrics import faithfulness, answer_relevancy, context_precision, context_recall
    from datasets import Dataset
except ImportError:
    print("Error: RAGAS not installed. Run: pip install ragas datasets")
    sys.exit(1)

try:
    import anthropic
except ImportError:
    print("Error: Anthropic SDK not installed. Run: pip install anthropic")
    sys.exit(1)

# Add scripts to path
sys.path.insert(0, str(Path(__file__).parent.parent / 'scripts'))

import lancedb
from sentence_transformers import SentenceTransformer
from config import CATEGORIES

def load_test_cases():
    """Load test cases from rag_evaluation.json."""
    test_file = Path(__file__).parent / 'rag_evaluation.json'
    with open(test_file) as f:
        data = json.load(f)
    return data['test_cases']

def query_rag(question, db, model, top_k=5):
    """Query RAG and return contexts."""
    vector = model.encode([question])[0].tolist()

    results = []
    for category in CATEGORIES:
        try:
            table = db.open_table(category)
            matches = table.search(vector).limit(top_k).to_list()
            for match in matches:
                results.append({
                    'content': match['content'],
                    'distance': match['_distance'],
                    'category': category
                })
        except Exception:
            continue

    # Sort by distance and take top_k
    results.sort(key=lambda x: x['distance'])
    return [r['content'] for r in results[:top_k]]

def generate_answer(question, contexts, api_key):
    """Generate answer using Claude API with retrieved contexts."""
    client = anthropic.Anthropic(api_key=api_key)

    context_text = "\n\n".join(f"[Context {i+1}]\n{ctx}" for i, ctx in enumerate(contexts))

    prompt = f"""You are an expert on the ilp_for C++ library. Answer the following question using ONLY the provided context. Do not add information not present in the context.

Context:
{context_text}

Question: {question}

Answer:"""

    message = client.messages.create(
        model="claude-3-5-sonnet-20241022",
        max_tokens=1024,
        messages=[{"role": "user", "content": prompt}]
    )

    return message.content[0].text

def run_ragas_evaluation(verbose=False):
    """Run RAGAS evaluation on all test cases."""
    # Check API key
    api_key = os.environ.get('ANTHROPIC_API_KEY')
    if not api_key:
        print("Error: ANTHROPIC_API_KEY environment variable not set")
        return 1

    test_cases = load_test_cases()

    # Connect to database
    db_path = Path(__file__).parent.parent / 'db'
    if not db_path.exists():
        print("Error: Database not found. Run setup.sh first.")
        return 1

    db = lancedb.connect(str(db_path))

    # Load embedding model
    print("Loading embedding model...")
    model = SentenceTransformer('BAAI/bge-small-en-v1.5')

    print(f"\nGenerating answers for {len(test_cases)} test cases...")
    print("⚠️  This will make API calls to Claude. Cost estimate: ~$0.05-0.10\n")

    # Prepare data for RAGAS
    questions = []
    answers = []
    contexts = []
    ground_truths = []

    for i, test in enumerate(test_cases, 1):
        if verbose:
            print(f"[{i}/{len(test_cases)}] {test['id']}: Generating answer...")

        # Query RAG
        retrieved_contexts = query_rag(test['question'], db, model, top_k=5)

        # Generate answer
        answer = generate_answer(test['question'], retrieved_contexts, api_key)

        questions.append(test['question'])
        answers.append(answer)
        contexts.append(retrieved_contexts)
        ground_truths.append(test['expected_answer'])

        if verbose:
            print(f"  Retrieved {len(retrieved_contexts)} contexts")
            print(f"  Answer: {answer[:100]}...")
            print()

    # Create RAGAS dataset
    dataset = Dataset.from_dict({
        'question': questions,
        'answer': answers,
        'contexts': contexts,
        'ground_truth': ground_truths
    })

    # Run RAGAS evaluation
    print("\nRunning RAGAS evaluation...")
    print("⚠️  This will make additional API calls for metric computation\n")

    metrics = [
        faithfulness,        # Is answer grounded in context?
        answer_relevancy,    # Does answer address the question?
        context_precision,   # Are relevant contexts ranked higher?
        context_recall       # Did we retrieve all necessary context?
    ]

    ragas_result = evaluate(dataset, metrics=metrics)

    # Print results
    print("=" * 60)
    print("RAGAS EVALUATION RESULTS")
    print("=" * 60)
    print(f"Test cases: {len(test_cases)}\n")

    for metric_name, score in ragas_result.items():
        if metric_name != 'ragas_score':  # Skip aggregate
            print(f"{metric_name}: {score:.3f}")

    print(f"\nOverall RAGAS Score: {ragas_result.get('ragas_score', 'N/A'):.3f}")

    # Save detailed results
    results_dir = Path(__file__).parent / 'results'
    results_dir.mkdir(exist_ok=True)

    timestamp = datetime.now().isoformat()
    output_file = results_dir / f"ragas_eval_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"

    # Per-question breakdown
    per_question_results = []
    for i, test in enumerate(test_cases):
        per_question_results.append({
            'id': test['id'],
            'category': test['category'],
            'difficulty': test['difficulty'],
            'question': questions[i],
            'answer': answers[i],
            'expected_answer': ground_truths[i],
            'contexts_retrieved': len(contexts[i])
        })

    output = {
        'timestamp': timestamp,
        'test_count': len(test_cases),
        'metrics': {k: v for k, v in ragas_result.items()},
        'per_question_results': per_question_results,
        'model_used': 'claude-3-5-sonnet-20241022'
    }

    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"\nDetailed results saved to {output_file}")

    # Success if all metrics >= 0.7
    min_score = min(v for k, v in ragas_result.items() if k != 'ragas_score')
    return 0 if min_score >= 0.7 else 1

if __name__ == '__main__':
    verbose = '--verbose' in sys.argv or '-v' in sys.argv
    exit(run_ragas_evaluation(verbose=verbose))

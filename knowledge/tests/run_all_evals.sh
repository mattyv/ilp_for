#!/bin/bash
# Unified evaluation runner for ilp_for RAG knowledge base
# Runs all three types of evaluations: custom, RAGAS, and code generation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================================================"
echo "ILP_FOR RAG Knowledge Base - Comprehensive Evaluation Suite"
echo "======================================================================"
echo ""

# Check for required environment variables
if [ -z "$ANTHROPIC_API_KEY" ]; then
    echo "Warning: ANTHROPIC_API_KEY not set. RAGAS and CodeGen evaluations will be skipped."
    SKIP_API_TESTS=true
else
    SKIP_API_TESTS=false
fi

# Activate virtual environment
if [ -f "../.venv/bin/activate" ]; then
    source ../.venv/bin/activate
else
    echo "Error: Virtual environment not found at ../.venv"
    echo "Run setup.sh first"
    exit 1
fi

# Track overall success
ALL_PASSED=true

# ============================================================================
# 1. Custom Evaluation (Retrieval Quality - No API needed)
# ============================================================================
echo "======================================================================"
echo "1. CUSTOM EVALUATION - Retrieval Quality"
echo "======================================================================"
echo ""

if python evaluate_custom.py; then
    echo "✓ Custom evaluation PASSED"
else
    echo "✗ Custom evaluation FAILED"
    ALL_PASSED=false
fi

echo ""

# ============================================================================
# 2. RAGAS Evaluation (Answer Quality - Requires API)
# ============================================================================
if [ "$SKIP_API_TESTS" = false ]; then
    echo "======================================================================"
    echo "2. RAGAS EVALUATION - Answer Quality"
    echo "======================================================================"
    echo ""

    if python evaluate_ragas.py; then
        echo "✓ RAGAS evaluation PASSED"
    else
        echo "✗ RAGAS evaluation FAILED"
        ALL_PASSED=false
    fi

    echo ""
else
    echo "======================================================================"
    echo "2. RAGAS EVALUATION - SKIPPED (No API key)"
    echo "======================================================================"
    echo ""
fi

# ============================================================================
# 3. Code Generation Evaluation (Synthesis Quality - Requires API)
# ============================================================================
if [ "$SKIP_API_TESTS" = false ]; then
    echo "======================================================================"
    echo "3. CODE GENERATION EVALUATION - Synthesis Quality"
    echo "======================================================================"
    echo ""

    if python evaluate_codegen.py; then
        echo "✓ Code generation evaluation PASSED"
    else
        echo "✗ Code generation evaluation FAILED"
        ALL_PASSED=false
    fi

    echo ""
else
    echo "======================================================================"
    echo "3. CODE GENERATION EVALUATION - SKIPPED (No API key)"
    echo "======================================================================"
    echo ""
fi

# ============================================================================
# Summary
# ============================================================================
echo "======================================================================"
echo "EVALUATION SUMMARY"
echo "======================================================================"
echo ""

if [ "$ALL_PASSED" = true ]; then
    echo "✓ ALL EVALUATIONS PASSED"
    echo ""
    echo "The RAG knowledge base meets all quality criteria:"
    echo "  - Retrieval: Correct knowledge chunks retrieved"
    echo "  - Answers: Faithful and relevant responses"
    if [ "$SKIP_API_TESTS" = false ]; then
        echo "  - Synthesis: Can generate correct, compilable code"
    fi
    exit 0
else
    echo "✗ SOME EVALUATIONS FAILED"
    echo ""
    echo "Please review the detailed output above and the result files in:"
    echo "  $SCRIPT_DIR/results/"
    echo ""
    echo "Failed evaluations indicate gaps in the knowledge base that need addressing."
    exit 1
fi

#!/usr/bin/env python3
"""
Code Generation Evaluation for RAG Knowledge Base

Tests whether an LLM using ONLY RAG context can generate correct, compilable code.
"""

import anthropic
import json
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Tuple

# Add parent directory (knowledge/) and lucidity/scripts to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))
sys.path.insert(0, str(Path(__file__).parent.parent / "lucidity" / "scripts"))
from config import DB_PATH, EMBEDDING_MODEL, CATEGORIES, CODEGEN_EVAL_MODEL
from rag_query import query

def query_rag(task_description: str, top_k: int = 5) -> str:
    """Query RAG system and format results as context string."""
    results = query(task_description, category="all", limit=top_k)

    if not results:
        return "No relevant context found."

    context_parts = []
    for i, result in enumerate(results, 1):
        context_parts.append(f"[Context {i}]")
        context_parts.append(f"Category: {result.get('_category', 'unknown')}")
        if result.get('related_symbols'):
            symbols = result['related_symbols']
            if isinstance(symbols, list):
                context_parts.append(f"Symbols: {', '.join(symbols)}")
            else:
                context_parts.append(f"Symbols: {symbols}")
        context_parts.append(f"Content: {result['content']}")
        context_parts.append("")

    return "\n".join(context_parts)


def generate_code(context: str, task_description: str, function_signature: str, api_key: str) -> str:
    """Generate code using Claude API with strict RAG-only prompting."""
    client = anthropic.Anthropic(api_key=api_key)

    prompt = f"""You are a C++ developer who MUST use ONLY the provided context below.
Do NOT use any prior knowledge about ILP_FOR. Follow the context exactly.
If the context doesn't provide enough information, say so - do not invent features.

CONTEXT:
{context}

TASK:
{task_description}

Function signature:
{function_signature}

Requirements:
- Write ONLY the function implementation
- Include necessary #include statements
- Use the exact function signature provided
- Follow the patterns shown in the context
- Return ONLY valid C++ code, no explanations

Generate the complete function implementation now:"""

    message = client.messages.create(
        model=CODEGEN_EVAL_MODEL,
        max_tokens=2000,
        messages=[{"role": "user", "content": prompt}]
    )

    return message.content[0].text


def extract_code(response: str) -> str:
    """Extract C++ code from LLM response (handles markdown code blocks)."""
    # Try to find code in ```cpp or ``` blocks
    cpp_pattern = r'```(?:cpp|c\+\+)?\n(.*?)```'
    matches = re.findall(cpp_pattern, response, re.DOTALL)

    if matches:
        return matches[0].strip()

    # If no code blocks, return the whole response (might be plain code)
    return response.strip()


def compile_code(code: str, flags: List[str], test_id: str) -> Tuple[bool, str, str]:
    """
    Compile C++ code with g++.

    Returns:
        (success, binary_path, error_message)
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        source_file = Path(tmpdir) / f"{test_id}.cpp"
        binary_file = Path(tmpdir) / f"{test_id}.out"

        # Write source code
        source_file.write_text(code)

        # Compile
        compile_cmd = ["g++"] + flags + ["-o", str(binary_file), str(source_file)]

        try:
            result = subprocess.run(
                compile_cmd,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                # Copy binary to persistent location
                output_dir = Path(__file__).parent / "results" / "codegen_binaries"
                output_dir.mkdir(parents=True, exist_ok=True)
                persistent_binary = output_dir / f"{test_id}.out"

                # Read and write binary
                with open(binary_file, 'rb') as src:
                    with open(persistent_binary, 'wb') as dst:
                        dst.write(src.read())

                # Make executable
                os.chmod(persistent_binary, 0o755)

                return True, str(persistent_binary), ""
            else:
                return False, "", result.stderr

        except subprocess.TimeoutExpired:
            return False, "", "Compilation timeout (30s)"
        except Exception as e:
            return False, "", f"Compilation error: {str(e)}"


def create_test_harness(function_code: str, function_signature: str, test_cases: List[Dict]) -> str:
    """Create a C++ test harness that calls the generated function."""
    # Extract function name from signature
    func_name_match = re.search(r'\w+\s+(\w+)\s*\(', function_signature)
    if not func_name_match:
        raise ValueError(f"Cannot extract function name from: {function_signature}")

    func_name = func_name_match.group(1)

    # Determine return type
    return_type_match = re.search(r'^(\w+)\s+', function_signature)
    if not return_type_match:
        return_type = "int"  # default
    else:
        return_type = return_type_match.group(1)

    harness = f"""#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "../../../ilp_for.hpp"

{function_code}

int main(int argc, char** argv) {{
    if (argc < 2) {{
        std::cerr << "Usage: " << argv[0] << " <test_case_index>" << std::endl;
        return 1;
    }}

    int test_idx = std::stoi(argv[1]);

"""

    # Add test cases
    for i, test_case in enumerate(test_cases):
        input_data = test_case["input"]
        expected = test_case["expected"]

        harness += f"    if (test_idx == {i}) {{\n"

        # Build input
        if "data" in input_data:
            data_values = input_data["data"]
            if isinstance(expected, list):  # string vector test
                harness += f"        std::vector<std::string> data = {{"
                harness += ", ".join([f'"{s}"' for s in data_values])
                harness += "};\n"
                harness += f"        {func_name}(data);\n"
                harness += f"        std::vector<std::string> expected = {{"
                harness += ", ".join([f'"{s}"' for s in expected])
                harness += "};\n"
                harness += "        if (data == expected) {\n"
                harness += "            std::cout << \"PASS\" << std::endl;\n"
                harness += "            return 0;\n"
                harness += "        } else {\n"
                harness += "            std::cout << \"FAIL\" << std::endl;\n"
                harness += "            return 1;\n"
                harness += "        }\n"
            else:
                # Numeric test
                if isinstance(data_values[0] if data_values else 0, float):
                    harness += f"        std::vector<double> data = {{"
                else:
                    harness += f"        std::vector<int> data = {{"
                harness += ", ".join([str(v) for v in data_values])
                harness += "};\n"

                # Handle additional inputs (like target, threshold, weights, etc.)
                for key, value in input_data.items():
                    if key != "data":
                        if isinstance(value, list):
                            if isinstance(value[0] if value else 0, float):
                                harness += f"        std::vector<double> {key} = {{"
                            else:
                                harness += f"        std::vector<int> {key} = {{"
                            harness += ", ".join([str(v) for v in value])
                            harness += "};\n"
                        elif isinstance(value, float):
                            harness += f"        double {key} = {value};\n"
                        else:
                            harness += f"        int {key} = {value};\n"

                # Call function
                args = ", ".join(["data"] + [k for k in input_data.keys() if k != "data"])
                harness += f"        auto result = {func_name}({args});\n"

                if isinstance(expected, float):
                    harness += f"        double expected = {expected};\n"
                    harness += "        if (std::abs(result - expected) < 0.0001) {\n"
                else:
                    harness += f"        {return_type} expected = {expected};\n"
                    harness += "        if (result == expected) {\n"

                harness += "            std::cout << \"PASS\" << std::endl;\n"
                harness += "            return 0;\n"
                harness += "        } else {\n"
                harness += "            std::cout << \"FAIL: expected \" << expected << \" got \" << result << std::endl;\n"
                harness += "            return 1;\n"
                harness += "        }\n"

        harness += "    }\n\n"

    harness += """    std::cerr << "Invalid test index" << std::endl;
    return 1;
}
"""

    return harness


def run_tests(binary_path: str, test_cases: List[Dict]) -> Tuple[int, int, List[str]]:
    """
    Run compiled binary against test cases.

    Returns:
        (passed_count, total_count, failure_details)
    """
    passed = 0
    failures = []

    for i, test_case in enumerate(test_cases):
        try:
            result = subprocess.run(
                [binary_path, str(i)],
                capture_output=True,
                text=True,
                timeout=5
            )

            if result.returncode == 0 and "PASS" in result.stdout:
                passed += 1
            else:
                failures.append(f"Test {i}: {result.stdout.strip()} {result.stderr.strip()}")
        except subprocess.TimeoutExpired:
            failures.append(f"Test {i}: Timeout (5s)")
        except Exception as e:
            failures.append(f"Test {i}: {str(e)}")

    return passed, len(test_cases), failures


def check_quality(code: str, checks: Dict[str, Any]) -> Tuple[bool, List[str]]:
    """
    Check code quality against requirements.

    Returns:
        (passed, violations)
    """
    violations = []

    # Check must_contain patterns
    if "must_contain" in checks:
        for pattern in checks["must_contain"]:
            if pattern not in code:
                violations.append(f"Missing required pattern: {pattern}")

    # Check must_not_contain patterns
    if "must_not_contain" in checks:
        for pattern in checks["must_not_contain"]:
            if pattern in code:
                violations.append(f"Contains forbidden pattern: {pattern}")

    # Check must_contain_count
    if "must_contain_count" in checks:
        for pattern, expected_count in checks["must_contain_count"].items():
            actual_count = code.count(pattern)
            if actual_count != expected_count:
                violations.append(f"Pattern '{pattern}' appears {actual_count} times, expected {expected_count}")

    # Check comment requirements
    if "comment_must_mention" in checks:
        for keyword in checks["comment_must_mention"]:
            if keyword.lower() not in code.lower():
                violations.append(f"Comment should mention: {keyword}")

    return len(violations) == 0, violations


def evaluate_test_case(test_case: Dict, api_key: str, verbose: bool = False) -> Dict[str, Any]:
    """Evaluate a single code generation test case."""
    test_id = test_case["id"]

    if verbose:
        print(f"\n{'='*60}")
        print(f"Test: {test_id}")
        print(f"Category: {test_case['category']}")
        print(f"Difficulty: {test_case['difficulty']}")
        print(f"{'='*60}\n")

    result = {
        "id": test_id,
        "category": test_case["category"],
        "difficulty": test_case["difficulty"],
        "compilation": {"success": False, "error": ""},
        "correctness": {"passed": 0, "total": 0, "failures": []},
        "quality": {"passed": False, "violations": []},
        "score": 0.0,
        "generated_code": "",
        "rag_context": ""
    }

    try:
        # Step 1: Query RAG
        if verbose:
            print("Step 1: Querying RAG...")
        context = query_rag(test_case["task_description"])
        result["rag_context"] = context

        if verbose:
            print(f"Retrieved {context.count('[Context')} context entries\n")

        # Step 2: Generate code
        if verbose:
            print("Step 2: Generating code with Claude...")
        generated_code = generate_code(
            context,
            test_case["task_description"],
            test_case["function_signature"],
            api_key
        )
        extracted_code = extract_code(generated_code)
        result["generated_code"] = extracted_code

        if verbose:
            print("Generated code:")
            print(extracted_code)
            print()

        # Step 3: Create test harness
        if verbose:
            print("Step 3: Creating test harness...")
        test_harness = create_test_harness(
            extracted_code,
            test_case["function_signature"],
            test_case["test_cases"]
        )

        # Step 4: Compile
        if verbose:
            print("Step 4: Compiling...")
        compile_success, binary_path, compile_error = compile_code(
            test_harness,
            test_case["evaluation_checks"]["compilation_flags"],
            test_id
        )

        result["compilation"]["success"] = compile_success
        result["compilation"]["error"] = compile_error

        if not compile_success:
            if verbose:
                print(f"❌ Compilation failed:")
                print(compile_error)
            return result

        if verbose:
            print("✓ Compilation successful\n")

        # Step 5: Run tests
        if verbose:
            print("Step 5: Running tests...")
        passed, total, failures = run_tests(binary_path, test_case["test_cases"])
        result["correctness"]["passed"] = passed
        result["correctness"]["total"] = total
        result["correctness"]["failures"] = failures

        if verbose:
            print(f"Tests: {passed}/{total} passed")
            if failures:
                for failure in failures:
                    print(f"  - {failure}")
            print()

        # Step 6: Quality checks
        if verbose:
            print("Step 6: Quality checks...")
        quality_passed, violations = check_quality(
            extracted_code,
            test_case["evaluation_checks"]
        )
        result["quality"]["passed"] = quality_passed
        result["quality"]["violations"] = violations

        if verbose:
            if quality_passed:
                print("✓ Quality checks passed")
            else:
                print("❌ Quality violations:")
                for violation in violations:
                    print(f"  - {violation}")
            print()

        # Calculate score
        compilation_score = 0.3 if compile_success else 0.0
        correctness_score = 0.4 * (passed / total) if total > 0 else 0.0
        quality_score = 0.3 if quality_passed else 0.0

        result["score"] = compilation_score + correctness_score + quality_score

        if verbose:
            print(f"Score: {result['score']:.2f} (compile: {compilation_score:.2f}, correctness: {correctness_score:.2f}, quality: {quality_score:.2f})")

    except Exception as e:
        result["compilation"]["error"] = str(e)
        if verbose:
            print(f"❌ Error: {e}")

    return result


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Evaluate code generation with RAG context")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--test-id", help="Run only specific test ID")
    args = parser.parse_args()

    # Check for API key
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print("Error: ANTHROPIC_API_KEY environment variable not set")
        sys.exit(1)

    # Load test cases
    test_file = Path(__file__).parent / "code_generation_evaluation.json"
    with open(test_file) as f:
        data = json.load(f)

    test_cases = data["test_cases"]

    # Filter by test ID if specified
    if args.test_id:
        test_cases = [tc for tc in test_cases if tc["id"] == args.test_id]
        if not test_cases:
            print(f"Error: Test ID '{args.test_id}' not found")
            sys.exit(1)

    # Run evaluation
    print(f"Running code generation evaluation with {len(test_cases)} test(s)...")
    print(f"{'='*60}\n")

    results = []
    for test_case in test_cases:
        result = evaluate_test_case(test_case, api_key, args.verbose)
        results.append(result)

    # Calculate aggregate statistics
    total_tests = len(results)
    compiled = sum(1 for r in results if r["compilation"]["success"])
    all_tests_passed = sum(1 for r in results if r["correctness"]["passed"] == r["correctness"]["total"])
    avg_score = sum(r["score"] for r in results) / total_tests if total_tests > 0 else 0.0

    compile_rate = compiled / total_tests if total_tests > 0 else 0.0
    correctness_rate = all_tests_passed / total_tests if total_tests > 0 else 0.0

    # Print summary
    print(f"\n{'='*60}")
    print("EVALUATION SUMMARY")
    print(f"{'='*60}\n")
    print(f"Total Tests: {total_tests}")
    print(f"Compilation Rate: {compile_rate:.1%} ({compiled}/{total_tests})")
    print(f"Correctness Rate: {correctness_rate:.1%} ({all_tests_passed}/{total_tests})")
    print(f"Average Score: {avg_score:.3f}")
    print()

    # Success criteria
    print("Success Criteria:")
    print(f"  Compilation Rate >= 90%: {'✓ PASS' if compile_rate >= 0.9 else '✗ FAIL'}")
    print(f"  Correctness Rate >= 80%: {'✓ PASS' if correctness_rate >= 0.8 else '✗ FAIL'}")
    print(f"  Average Score >= 0.8: {'✓ PASS' if avg_score >= 0.8 else '✗ FAIL'}")
    print()

    # Save results
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    results_dir = Path(__file__).parent / "results"
    results_dir.mkdir(exist_ok=True)

    output_file = results_dir / f"codegen_eval_{timestamp}.json"

    output_data = {
        "version": data["version"],
        "timestamp": timestamp,
        "summary": {
            "total_tests": total_tests,
            "compilation_rate": compile_rate,
            "correctness_rate": correctness_rate,
            "average_score": avg_score,
            "success_criteria": {
                "compilation_rate_90": compile_rate >= 0.9,
                "correctness_rate_80": correctness_rate >= 0.8,
                "average_score_80": avg_score >= 0.8
            }
        },
        "results": results
    }

    with open(output_file, "w") as f:
        json.dump(output_data, f, indent=2)

    print(f"Results saved to: {output_file}")
    print()

    # Exit code based on success criteria
    if compile_rate >= 0.9 and correctness_rate >= 0.8 and avg_score >= 0.8:
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()

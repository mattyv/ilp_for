#!/usr/bin/env python3
"""
Generate Godbolt Compiler Explorer links for ILP_FOR examples.

This script creates comparison views showing ILP vs hand-rolled vs simple implementations.
"""

import requests
import json
from pathlib import Path
from typing import Dict, List

# Compiler configurations - using stable IDs
COMPILERS = {
    'x86-64 Clang': {
        'compiler': 'clang_trunk',  # Clang trunk (latest)
        'options': '-std=c++2b -O3 -march=skylake',
        'lang': 'c++'
    },
    'x86-64 GCC': {
        'compiler': 'g141',  # GCC 14.1
        'options': '-std=c++2b -O3 -march=skylake',
        'lang': 'c++'
    },
    'ARM64': {
        'compiler': 'arm64g1220',  # ARM64 GCC 12.2
        'options': '-std=c++2b -O3 -mcpu=cortex-a72',
        'lang': 'c++'
    }
}

EXAMPLES = [
    # ILP_FOR macro examples (primary)
    {
        'file': 'loop_with_break.cpp',
        'title': 'Loop with Break',
        'description': 'ILP_FOR with ILP_BREAK showing early exit from unrolled loop'
    },
    {
        'file': 'loop_with_return.cpp',
        'title': 'Loop with Return',
        'description': 'ILP_FOR with ILP_RETURN to exit enclosing function from loop'
    },
    # ilp::find and ilp::reduce function examples (auxiliary)
    {
        'file': 'find_first_match.cpp',
        'title': 'Find First Match',
        'description': 'ilp::find for early-exit search (std::find alternative)'
    },
    {
        'file': 'parallel_min.cpp',
        'title': 'Parallel Minimum',
        'description': 'ilp::reduce breaking dependency chains (std::min_element alternative)'
    },
    {
        'file': 'sum_with_break.cpp',
        'title': 'Sum with Early Exit',
        'description': 'ilp::reduce with early termination (std::accumulate alternative)'
    }
]

import re

def extract_template_params(content: str, start_pos: int) -> str:
    """Extract template<...> declaration with proper angle bracket matching."""
    if not content[start_pos:].startswith('template'):
        return ''

    # Find the opening <
    i = start_pos + len('template')
    while i < len(content) and content[i].isspace():
        i += 1

    if i >= len(content) or content[i] != '<':
        return ''

    # Match balanced angle brackets
    depth = 1
    i += 1
    start = start_pos

    while i < len(content) and depth > 0:
        if content[i] == '<':
            depth += 1
        elif content[i] == '>':
            depth -= 1
        i += 1

    if depth == 0:
        return content[start:i]
    return ''

def extract_function_or_struct(content: str, name: str) -> str:
    """Extract a function, struct, or class definition by name from C++ code."""
    # Special case for is_optional - need both base template and specialization
    if name == 'is_optional':
        pattern = r'(template<typename T>\s+struct is_optional.*?;\s*\n\s*template<typename T>\s+struct is_optional<std::optional<T>>.*?;)'
        match = re.search(pattern, content, re.DOTALL)
        if match:
            return match.group(1) + '\n'

    # First find where the function/struct name appears
    escaped_name = re.escape(name)
    # Look for the name preceded by keywords or types
    # Also handle variable templates like: constexpr bool name = ...
    name_pattern = rf'(?:struct|class|auto|void|inline|constexpr|\w+)\s+{escaped_name}\s*[(<{{=]'

    for match in re.finditer(name_pattern, content):
        # Search backwards from the match to find if there's a template declaration
        pos = match.start()

        # Look backwards for template keyword (can be several lines before)
        search_start = max(0, pos - 1000)  # Look back up to 1000 chars
        prefix = content[search_start:pos]

        # Search for template keyword anywhere in the prefix
        # Template can be followed by newlines, requires clause, etc before the function
        template_matches = list(re.finditer(r'template\s*<', prefix))

        if template_matches:
            # Take the last template match (closest to the function)
            last_template = template_matches[-1]
            template_start = search_start + last_template.start()

            # Verify this template belongs to our function by checking there's no
            # other function/struct/class declaration between template and our match
            between = content[template_start:pos]
            # Check if there's another complete declaration in between
            other_decl = re.search(r'\n(?:template|struct|class|auto|void|inline|constexpr|\w+)\s+(?!\w*\s*requires)\w+\s*[(<{{].*?[;}]', between, re.DOTALL)

            if not other_decl:
                # This template belongs to our function
                body_start = template_start
            else:
                body_start = pos
        else:
            body_start = pos

        # Check what the match ended with
        next_char_pos = match.end() - 1  # Back up to the last char we matched

        if content[next_char_pos] == '=':
            # Variable template or declaration - extract until semicolon
            semicolon = content.find(';', next_char_pos)
            if semicolon != -1:
                return content[body_start:semicolon + 1].rstrip() + '\n'

        # If the match ended with '{', that's our opening brace
        if content[next_char_pos] == '{':
            opening_brace = next_char_pos
        else:
            # Find the opening brace of the function/struct body
            opening_brace = content.find('{', match.end())

        if opening_brace == -1:
            # No body (declaration only), extract until semicolon or newline
            end_pattern = r'(?=\n(?:template|struct|class|namespace|#define|$))'
            end_match = re.search(end_pattern, content[body_start:], re.MULTILINE)
            if end_match:
                body_end = body_start + end_match.start()
            else:
                body_end = len(content)
            return content[body_start:body_end].rstrip() + '\n'

        # Count braces to find the matching closing brace
        brace_count = 1
        i = opening_brace + 1
        while i < len(content) and brace_count > 0:
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
            i += 1

        if brace_count == 0:
            # Found the matching closing brace
            body_end = i
            # For structs/classes, include trailing semicolon if present
            if body_end < len(content) and content[body_end] == ';':
                body_end += 1
            return content[body_start:body_end].rstrip() + '\n'

        # Fallback: couldn't find matching brace
        return content[body_start:opening_brace + 100].rstrip() + '\n'

    return ''

def extract_macro(content: str, name: str) -> str:
    """Extract a #define macro by name, including multi-line macros."""
    pattern = rf'(#define\s+{re.escape(name)}\b[^\n]*(?:\\\n[^\n]*)*)'
    match = re.search(pattern, content)
    if match:
        return match.group(1) + '\n'
    return ''

def get_needed_items(example_name: str) -> Dict[str, List[str]]:
    """Return dict of {file_path: [list of items to extract]} for each example."""

    needs = {
        'transform_simple.cpp': {
            'detail/loops_common.hpp': ['warn_large_unroll_factor', 'validate_unroll_factor', 'ForBody', 'ForCtrlBody'],
            'detail/loops_ilp.hpp': ['for_loop_impl'],
            'ilp_for.hpp': ['For_Context_USE_ILP_END', 'for_loop', 'ILP_FOR', 'ILP_END']
        },
        'parallel_min.cpp': {
            'detail/loops_common.hpp': ['warn_large_unroll_factor', 'validate_unroll_factor', 'ReduceBody'],
            'detail/loops_ilp.hpp': ['reduce_impl'],
            'ilp_for.hpp': ['Reduce_Context_USE_ILP_END_REDUCE', 'reduce', 'ILP_REDUCE', 'ILP_END_REDUCE']
        },
        'sum_with_break.cpp': {
            'detail/loops_common.hpp': ['warn_large_unroll_factor', 'validate_unroll_factor'],
            'detail/ctrl.hpp': ['LoopCtrl'],
            'detail/loops_ilp.hpp': ['reduce_impl'],
            'ilp_for.hpp': ['Reduce_Context_USE_ILP_END_REDUCE', 'reduce', 'ILP_REDUCE', 'ILP_BREAK_RET', 'ILP_END_REDUCE']
        },
        'find_first_match.cpp': {
            'detail/loops_common.hpp': ['is_optional', 'is_optional_v', 'warn_large_unroll_factor', 'validate_unroll_factor'],
            'detail/loops_ilp.hpp': ['find_impl'],
            'ilp_for.hpp': ['For_Context_USE_ILP_END', 'find', 'ILP_FIND', 'ILP_END']
        }
    }

    return needs.get(example_name, {})

def extract_needed_code(example_name: str) -> str:
    """Extract only the macros and functions needed for this specific example."""
    repo_root = Path(__file__).parent.parent
    needed_items = get_needed_items(example_name)

    if not needed_items:
        return ""

    # Common includes
    code = """#include <cstddef>
#include <array>
#include <functional>
#include <utility>
#include <type_traits>
#include <concepts>
#include <optional>

namespace ilp {
namespace detail {

"""

    # Extract from library files - detail namespace items
    detail_code = ""
    public_code = ""

    for file_path, items in needed_items.items():
        full_path = repo_root / file_path
        if not full_path.exists():
            print(f"Warning: {full_path} not found")
            continue

        content = full_path.read_text(encoding='utf-8')

        for item in items:
            if item.startswith('ILP_'):  # It's a macro
                extracted = extract_macro(content, item)
                if extracted:
                    # Macros go outside namespace, add them after
                    continue
            else:  # It's a function or struct
                extracted = extract_function_or_struct(content, item)
                if extracted:
                    # Items from detail/ go in detail namespace
                    # Items from ilp_for.hpp might be public or detail
                    if 'detail/' in file_path:
                        detail_code += extracted + '\n'
                    elif file_path == 'ilp_for.hpp':
                        # Context structs and check functions are in detail
                        if '_Context_' in item or 'check_' in item:
                            detail_code += extracted + '\n'
                        else:
                            # Public wrapper functions go in ilp namespace
                            public_code += extracted + '\n'
                    else:
                        public_code += extracted + '\n'

    code += detail_code
    code += "} // namespace detail\n\n"
    code += public_code
    code += "} // namespace ilp\n\n"

    # Now add macros
    for file_path, items in needed_items.items():
        full_path = repo_root / file_path
        if not full_path.exists():
            continue

        content = full_path.read_text(encoding='utf-8')

        for item in items:
            if item.startswith('ILP_'):  # It's a macro
                extracted = extract_macro(content, item)
                if extracted:
                    code += extracted + '\n'

    return code


def create_godbolt_config(source_code: str, arch: str, example_name: str) -> Dict:
    """Create Godbolt API configuration."""
    compiler_config = COMPILERS[arch]

    # Use the example source code directly (it now contains all needed code)
    return {
        'sessions': [{
            'id': 1,
            'language': compiler_config['lang'],
            'source': source_code,
            'compilers': [{
                'id': compiler_config['compiler'],
                'options': compiler_config['options']
            }]
        }]
    }

def create_shortlink(config: Dict, debug: bool = False) -> str:
    """POST to Godbolt API and return short URL."""
    try:
        if debug:
            print(f"  Config size: {len(json.dumps(config))} bytes")
            print(f"  Files count: {len(config['sessions'][0].get('files', []))}")

        response = requests.post(
            'https://godbolt.org/api/shortener',
            json=config,
            headers={'Content-Type': 'application/json'},
            timeout=10
        )
        response.raise_for_status()
        result = response.json()
        url = result.get('url', '')

        # API returns full URL already
        if url.startswith('http'):
            return url
        # Otherwise construct it
        return f"https://godbolt.org/z/{url}" if url else None
    except Exception as e:
        print(f'Error creating shortlink: {e}')
        print(f'Response: {response.text if "response" in locals() else "N/A"}')
        return None

def generate_links() -> Dict[str, Dict[str, str]]:
    """Generate Godbolt links for all examples and architectures."""
    godbolt_dir = Path(__file__).parent.parent / 'godbolt_examples'
    links = {}

    print("Generating minimal self-contained examples...\n")

    for example in EXAMPLES:
        file_path = godbolt_dir / example['file']
        if not file_path.exists():
            print(f"Warning: {file_path} not found")
            continue

        source_code = file_path.read_text(encoding='utf-8')
        example_links = {}

        for arch_idx, arch in enumerate(COMPILERS):
            print(f"Generating {example['title']} - {arch}...")
            config = create_godbolt_config(source_code, arch, example['file'])
            # Debug first link only
            debug = (arch_idx == 0 and example == EXAMPLES[0])
            link = create_shortlink(config, debug=debug)

            if link:
                example_links[arch] = link
                print(f"  -> {link}")
            else:
                print(f"  -> Failed")

        links[example['file']] = example_links

    return links

def generate_markdown(links: Dict[str, Dict[str, str]]) -> str:
    """Generate EXAMPLES.md content with links."""
    md = """# Assembly Examples

View side-by-side comparisons on Compiler Explorer (Godbolt).

Each example shows three versions:
- **ILP**: Multi-accumulator pattern with parallel operations
- **Hand-rolled**: Manual 4x unroll for comparison
- **Simple**: Baseline single-iteration loop

---

# ILP_FOR Macro

The primary API for loops with `break`, `continue`, or `return`.

"""

    # ILP_FOR examples (first two)
    for example in EXAMPLES[:2]:
        file_name = example['file']
        if file_name not in links or not links[file_name]:
            continue

        md += f"## {example['title']}\n\n"
        md += f"{example['description']}\n\n"

        arch_links = links[file_name]
        link_parts = []
        for arch, link in arch_links.items():
            link_parts.append(f"[{arch}]({link})")

        md += f"**View on Godbolt:** {' | '.join(link_parts)}\n\n"
        md += f"[Source code](../godbolt_examples/{file_name})\n\n"
        md += "---\n\n"

    md += """# Function API

Alternative `std::`-style functions with early exit support.

"""

    # Function API examples (remaining)
    for example in EXAMPLES[2:]:
        file_name = example['file']
        if file_name not in links or not links[file_name]:
            continue

        md += f"## {example['title']}\n\n"
        md += f"{example['description']}\n\n"

        arch_links = links[file_name]
        link_parts = []
        for arch, link in arch_links.items():
            link_parts.append(f"[{arch}]({link})")

        md += f"**View on Godbolt:** {' | '.join(link_parts)}\n\n"
        md += f"[Source code](../godbolt_examples/{file_name})\n\n"
        md += "---\n\n"

    md += """
## How to Use

1. Click a Godbolt link for your target architecture
2. The code will load with optimizations enabled
3. Compare the generated assembly between implementations
4. Look for:
   - Parallel comparisons in ILP version vs sequential in hand-rolled
   - Register usage and instruction scheduling differences
   - Branch prediction patterns

## Compiler Settings

- **x86-64 Clang**: Clang 18, `-std=c++2b -O3 -march=skylake`
- **x86-64 GCC**: GCC 14.1, `-std=c++2b -O3 -march=skylake`
- **ARM64**: ARM Clang 18, `-std=c++2b -O3 -mcpu=apple-m1`
"""

    return md

def main():
    """Generate Godbolt links and create EXAMPLES.md."""
    print("Generating Godbolt Compiler Explorer links...")
    print("This will make API requests and may take a few moments.\n")

    links = generate_links()

    # Generate markdown
    markdown = generate_markdown(links)

    # Write to docs/EXAMPLES.md
    docs_dir = Path(__file__).parent.parent / 'docs'
    docs_dir.mkdir(exist_ok=True)

    output_file = docs_dir / 'EXAMPLES.md'
    output_file.write_text(markdown, encoding='utf-8')

    print(f"\n[OK] Generated {output_file}")
    print(f"  Total examples: {len([e for e in EXAMPLES if e['file'] in links])}")

    # Also save links as JSON for reference
    json_file = docs_dir / 'godbolt_links.json'
    json_file.write_text(json.dumps(links, indent=2), encoding='utf-8')
    print(f"[OK] Saved links to {json_file}")

if __name__ == '__main__':
    main()

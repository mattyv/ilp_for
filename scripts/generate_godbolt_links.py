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
    'x86-64 Clang (MCA)': {
        'compiler': 'clang_trunk',  # Clang trunk (latest)
        'options': '-std=c++2b -O3 -march=skylake',
        'lang': 'c++',
        'tools': [{
            'id': 'llvm-mcatrunk',
            'args': '-timeline -bottleneck-analysis'
        }]
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
    # ILP_FOR macro examples
    {
        'file': 'loop_with_break.cpp',
        'title': 'Loop with Break',
        'description': 'ILP_FOR with ILP_BREAK showing early exit from unrolled loop'
    },
    {
        'file': 'pragma_vs_ilp.cpp',
        'title': 'Pragma Unroll vs ILP_FOR',
        'description': 'Why #pragma unroll doesn\'t help for early-exit loops - look for per-iteration bounds checks'
    },
    {
        'file': 'loop_with_return.cpp',
        'title': 'Loop with Return',
        'description': 'ILP_FOR with ILP_RETURN to exit enclosing function from loop'
    },
    {
        'file': 'loop_with_return_typed.cpp',
        'title': 'Loop with Large Return Type',
        'description': 'ILP_FOR_T for return types > 8 bytes (structs, large objects)'
    }
]

def create_godbolt_config(source_code: str, arch: str, example_name: str) -> Dict:
    """Create Godbolt API configuration."""
    compiler_config = COMPILERS[arch]

    # Build compiler entry
    compiler_entry = {
        'id': compiler_config['compiler'],
        'options': compiler_config['options']
    }

    # Add tools if configured (e.g., llvm-mca for Clang)
    if 'tools' in compiler_config:
        compiler_entry['tools'] = compiler_config['tools']

    # Use the example source code directly (it now contains all needed code)
    return {
        'sessions': [{
            'id': 1,
            'language': compiler_config['lang'],
            'source': source_code,
            'compilers': [compiler_entry]
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

"""

    for example in EXAMPLES:
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

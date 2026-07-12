#!/usr/bin/env python3
# Regenerate the Compiler Explorer (godbolt.org) share links in docs/EXAMPLES.md
# from the current godbolt_examples/*.cpp sources.
#
# Each example gets three links, matching the three views EXAMPLES.md documents:
#   - x86-64 Clang (MCA): clang + an llvm-mca tool pane (throughput analysis)
#   - x86-64 GCC:         gcc assembly view
#   - ARM64:              armv8-a clang assembly view (Apple-M1-tuned)
#
# The examples are C++20-clean (verified per godbolt_examples/INSTRUCTIONS.md),
# so every view compiles with -std=c++20. Uploads the source to godbolt's
# shortener API and rewrites the "View on Godbolt" lines in docs/EXAMPLES.md
# in place.
#
# Usage:
#   python3 godbolt_examples/make_godbolt_links.py            # regenerate + rewrite EXAMPLES.md
#   python3 godbolt_examples/make_godbolt_links.py --dry-run  # print links, don't touch EXAMPLES.md
#
# Requires only the Python standard library and outbound HTTPS to godbolt.org.

import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path

GODBOLT = "https://godbolt.org"
SHORTENER = f"{GODBOLT}/api/shortener"

REPO_ROOT = Path(__file__).resolve().parent.parent
EXAMPLES_MD = REPO_ROOT / "docs" / "EXAMPLES.md"
SRC_DIR = REPO_ROOT / "godbolt_examples"

# Compiler ids come from GET https://godbolt.org/api/compilers/c++?fields=id,name
# (bump these when the pinned toolchains in EXAMPLES.md's "Compiler Settings"
# change). Each view is (link-label, compiler-id, options, optional mca cpu).
X86_MARCH = "-march=skylake"
ARM_MCPU = "-mcpu=apple-m1"
STD = "-std=c++20"

# The three columns, in the exact order they appear in EXAMPLES.md's
# "View on Godbolt:" lines. label is what renders as the link text.
VIEWS = [
    {
        "label": "x86-64 Clang (MCA)",
        "compiler": "clang1810",
        "options": f"{STD} -O3 {X86_MARCH}",
        "mca_cpu": "skylake",  # attach an llvm-mca tool pane tuned for this cpu
    },
    {
        "label": "x86-64 GCC",
        "compiler": "g141",
        "options": f"{STD} -O3 {X86_MARCH}",
        "mca_cpu": None,
    },
    {
        "label": "ARM64",
        "compiler": "armv8-clang1810",
        "options": f"{STD} -O3 {ARM_MCPU}",
        "mca_cpu": None,
    },
]

# EXAMPLES.md section heading -> source file. The rewrite locates each
# "View on Godbolt:" line by the source-code link that follows its section.
SECTIONS = [
    ("loop_with_break.cpp", "Loop with Break"),
    ("pragma_vs_ilp.cpp", "Pragma Unroll vs ILP_FOR"),
    ("loop_with_return.cpp", "Loop with Return"),
    ("loop_with_return_typed.cpp", "Loop with Large Return Type"),
    ("find_if.cpp", "`ilp::find_if` — Vectorizable First-Match Search"),
]


def make_client_state(source: str, view: dict) -> dict:
    compiler = {"id": view["compiler"], "options": view["options"]}
    if view["mca_cpu"]:
        compiler["tools"] = [{"id": "llvm-mcatrunk", "args": f"-mcpu={view['mca_cpu']}"}]
    return {
        "sessions": [
            {"id": 1, "language": "c++", "source": source, "compilers": [compiler]}
        ]
    }


def shorten(state: dict) -> str:
    body = json.dumps(state).encode()
    req = urllib.request.Request(
        SHORTENER,
        data=body,
        headers={"Content-Type": "application/json", "Accept": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        if resp.status != 200:
            raise RuntimeError(f"shortener returned HTTP {resp.status}")
        return json.loads(resp.read().decode())["url"]


def generate_links() -> dict:
    """Returns {source_filename: [(label, url), ...]}."""
    out = {}
    for src_name, _heading in SECTIONS:
        src_path = SRC_DIR / src_name
        source = src_path.read_text()
        links = []
        for view in VIEWS:
            url = shorten(make_client_state(source, view))
            print(f"  {src_name:32} {view['label']:20} -> {url}", file=sys.stderr)
            links.append((view["label"], url))
        out[src_name] = links
    return out


def rewrite_examples_md(links: dict) -> bool:
    """Rewrite the 'View on Godbolt:' line in each section. Returns True if changed."""
    text = EXAMPLES_MD.read_text()
    original = text

    for src_name, section_links in links.items():
        # Anchor on the "[Source code](../godbolt_examples/<name>)" line, which
        # uniquely identifies each section, then rewrite the nearest preceding
        # "**View on Godbolt:**" line.
        rendered = " | ".join(f"[{label}]({url})" for label, url in section_links)
        new_line = f"**View on Godbolt:** {rendered}"

        src_link_pat = re.escape(f"[Source code](../godbolt_examples/{src_name})")
        # match a "View on Godbolt" line followed (after blank lines) by the source link
        pat = re.compile(
            r"\*\*View on Godbolt:\*\*[^\n]*(\n\s*\n\[Source code\]\(\.\./godbolt_examples/"
            + re.escape(src_name)
            + r"\))"
        )
        m = pat.search(text)
        if not m:
            print(
                f"WARNING: could not find View-on-Godbolt line for {src_name}; "
                f"skipping (check EXAMPLES.md structure)",
                file=sys.stderr,
            )
            continue
        text = text[: m.start()] + new_line + m.group(1) + text[m.end():]

    if text != original:
        EXAMPLES_MD.write_text(text)
        return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="print the generated links but do not modify docs/EXAMPLES.md",
    )
    args = ap.parse_args()

    print("Generating godbolt share links...", file=sys.stderr)
    links = generate_links()

    if args.dry_run:
        print("\n--- dry run, EXAMPLES.md not modified ---")
        for src_name, section_links in links.items():
            print(f"\n{src_name}:")
            for label, url in section_links:
                print(f"  {label}: {url}")
        return 0

    changed = rewrite_examples_md(links)
    print(
        f"\ndocs/EXAMPLES.md {'updated' if changed else 'unchanged (links identical)'}.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

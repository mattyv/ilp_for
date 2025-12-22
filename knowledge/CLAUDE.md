# Library Knowledge Base - Claude Instructions

This knowledge base contains validated insights about the library to reduce hallucinations.

## Setup (First Time)

### Prerequisites

On Debian/Ubuntu, you may need:
```bash
sudo apt install python3-venv python3-pip
```

### Installation

```bash
cd knowledge
python3 -m venv .venv
source .venv/bin/activate  # or .venv\Scripts\activate on Windows
pip install -r requirements.txt
python scripts/rag_setup.py
python scripts/rag_ingest.py  # Parse library source
```

### Claude Code Hook (Auto-Query)

To automatically query the knowledge base when asking about ilp_for:

```bash
# Create .claude directory and symlink the hook
mkdir -p .claude/hooks
ln -sf ../knowledge/hooks/rag_query_hook.py .claude/hooks/

# Create settings.json
cat > .claude/settings.json << 'EOF'
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/rag_query_hook.py",
            "timeout": 30
          }
        ]
      }
    ]
  }
}
EOF
```

## Before Using This Library

**ALWAYS query the knowledge base first:**
```bash
cd knowledge && source .venv/bin/activate
python scripts/rag_query.py "your question here"
```

## Querying Knowledge

```bash
# General query
python scripts/rag_query.py "how to use early exit"

# Category-specific
python scripts/rag_query.py "performance" --category performance

# JSON output for parsing
python scripts/rag_query.py "reduce" --json

# Limit results
python scripts/rag_query.py "macros" --limit 10
```

## Adding Knowledge

When you discover something important, save it:
```bash
python scripts/rag_add.py \
    --category gotchas \
    --content "Description of the issue" \
    --symbols SYMBOL1 SYMBOL2 \
    --tags "tag1" "tag2"
```

### Categories
- `api_signatures` - Exact function/macro signatures
- `usage_patterns` - Correct usage examples
- `antipatterns` - What NOT to do
- `gotchas` - Subtle issues/edge cases
- `performance` - When to use, benchmarks
- `compiler` - Compiler-specific behavior
- `semantics` - What things actually do/expand to

### Sources
- `code` - Parsed from source
- `docs` - From documentation
- `conversation` - Discovered in discussion
- `test` - Verified by testing

---

# ilp_for Specific Notes

## What is ilp_for?

A C++23 header-only library for compile-time loop unrolling optimized for early exit loops (break/continue/return). It avoids the per-iteration bounds checks that `#pragma unroll` generates.

## Key Things LLMs Get Wrong

1. **ILP_END vs ILP_END_RETURN** - Use ILP_END_RETURN when ILP_RETURN is used
2. **Macro hygiene** - These are macros, not functions. No semicolons after ILP_END
3. **LoopType selection** - Search for early exit, Sum for accumulation
4. **Return type limits** - Default SBO is 8 bytes; use ILP_FOR_T for larger types
5. **auto&& in range loops** - Use forwarding reference to avoid copies

## Quick Reference

```cpp
// Basic loop with break
ILP_FOR(auto i, 0, n, 4) {
    if (condition) ILP_BREAK;
    process(i);
} ILP_END;  // No semicolon needed after ILP_END

// Loop with return (note: ILP_END_RETURN, not ILP_END)
int find(int* data, int n, int target) {
    ILP_FOR(auto i, 0, n, 4) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RETURN;  // Must use this with ILP_RETURN
    return -1;
}

// Auto-selected unroll factor
ILP_FOR_AUTO(auto i, 0, n, Search) {
    // ...
} ILP_END;
```

---

## Adapting for Other Libraries

To use this RAG system with a different library:

1. Edit `scripts/config.py` - Update categories if needed
2. Edit `scripts/ingest_config.py` - Configure parsing patterns for your library
3. Update this file with library-specific notes
4. Run setup and ingest scripts

The core scripts (`rag_setup.py`, `rag_query.py`, `rag_add.py`) are library-agnostic.

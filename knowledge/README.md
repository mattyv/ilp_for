# RAG Knowledge System

A vector-based knowledge retrieval system for reducing LLM hallucinations when working with the ilp_for library. Automatically provides relevant context to Claude Code when users ask about the library.

## Architecture

```
knowledge/
├── db/                     # LanceDB vector database (gitignored)
├── hooks/
│   └── rag_query_hook.py   # Claude Code hook (committable)
├── scripts/
│   ├── config.py           # Centralized configuration
│   ├── ingest_config.py    # Library-specific parsing patterns
│   ├── rag_setup.py        # Database initialization
│   ├── rag_ingest.py       # Parse source code for API signatures
│   ├── rag_add.py          # Add individual insights
│   ├── rag_query.py        # Query the knowledge base
│   └── rag_delete.py       # Remove incorrect entries
├── CLAUDE.md               # Instructions for Claude (committed)
└── README.md               # This file
```

## How It Works

### 1. Vector Storage (LanceDB)

Knowledge is stored as vector embeddings in a LanceDB database. Each entry contains:

| Field | Type | Description |
|-------|------|-------------|
| `content` | string | The actual knowledge text |
| `vector` | float32[384] | Embedding from sentence-transformers |
| `source` | string | Origin: `docs`, `code`, `conversation`, `test` |
| `validated_by` | string | Who verified: `parsed`, `user`, `README` |
| `confidence` | float | 0.0-1.0 confidence score |
| `related_symbols` | string[] | API symbols this relates to |
| `tags` | string[] | Additional categorization |
| `created_at` | string | ISO timestamp |

### 2. Embedding Model

Uses `BAAI/bge-small-en-v1.5` via sentence-transformers:
- 384-dimensional embeddings
- Fast inference (~100ms per query)
- Good semantic similarity for technical content

### 3. Categories

Entries are organized into semantic tables:

| Category | Purpose |
|----------|---------|
| `api_signatures` | Macro/function signatures from source |
| `usage_patterns` | How to use features correctly |
| `antipatterns` | What NOT to do |
| `gotchas` | Common mistakes and surprises |
| `performance` | Assembly analysis, optimization tips |
| `compiler` | Compiler flags, CPU targeting |
| `semantics` | How the library behaves internally |

### 4. Query Flow

```
User prompt → Hook detects keywords → Query vector DB → Return top matches
                                                              ↓
                              Claude sees additionalContext ←─┘
```

The hook ([hooks/rag_query_hook.py](hooks/rag_query_hook.py)) triggers on keywords:
- `ilp_for`, `ilp_`, `ilp::`
- `loop.*unroll`, `unroll.*loop`
- `early.?exit.*loop`
- `loop.*break`, `loop.*return`, `loop.*continue`

## Setup

### Initial Setup

```bash
cd knowledge
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Initialize database
python scripts/rag_setup.py

# Parse source code for API signatures
python scripts/rag_ingest.py
```

### Claude Code Hook

The hook auto-queries when users ask about ilp_for:

```bash
# Create .claude directory and symlink
mkdir -p ../.claude/hooks
ln -sf ../../knowledge/hooks/rag_query_hook.py ../.claude/hooks/

# Create settings.json
cat > ../.claude/settings.json << 'EOF'
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

Note: `.claude/` is gitignored, but `knowledge/hooks/` is committed. The symlink approach lets collaborators easily set up the hook.

## Usage

### Query the Knowledge Base

```bash
cd knowledge/scripts
source ../.venv/bin/activate

# Search all categories
python rag_query.py "how do I use ILP_BREAK"

# Search specific category
python rag_query.py "break semantics" --category semantics

# JSON output for scripting
python rag_query.py "ILP_RETURN" --json --limit 3
```

### Add Knowledge

```bash
python rag_add.py \
  --category gotchas \
  --content "ILP_END_RETURN must be used when ILP_RETURN appears in the loop body" \
  --symbols ILP_RETURN ILP_END_RETURN \
  --confidence 0.9 \
  --source docs
```

### Delete Incorrect Entries

```bash
python rag_delete.py \
  --category gotchas \
  --contains "incorrect substring"
```

### Re-ingest Source Code

```bash
# After updating ilp_for.hpp
python rag_ingest.py
```

## Porting to Another Library

This system is designed to be library-agnostic. To use with a different library:

### 1. Update Configuration

Edit `scripts/config.py`:

```python
# Change categories if needed
CATEGORIES = [
    "api_signatures",
    "usage_patterns",
    # ... add/remove as needed
]
```

### 2. Configure Source Parsing

Edit `scripts/ingest_config.py`:

```python
INGEST_CONFIG = {
    "source_files": [
        "path/to/your_library.hpp",
    ],

    "macro_patterns": [
        {
            "pattern": r'#define\s+(YOUR_PREFIX_\w+)\s*(\([^)]*\))?',
            "name_group": 1,
            "signature_group": 0,
            "type": "macro",
            "prefix_filter": "YOUR_PREFIX_",
        },
    ],

    "function_patterns": [
        # Add patterns for your library's functions
    ],
}
```

### 3. Update Hook Keywords

Edit `hooks/rag_query_hook.py`:

```python
ILP_KEYWORDS = [
    r"\byour_library\b",
    r"\byour_prefix_",
    # ... add relevant patterns
]
```

### 4. Update CLAUDE.md

Replace ilp_for-specific content with your library's notes.

### 5. Re-run Setup

```bash
python scripts/rag_setup.py    # Creates new tables
python scripts/rag_ingest.py   # Parses your source
```

## Technical Details

### Why LanceDB?

- Embedded (no server required)
- Fast vector similarity search
- Simple Python API
- Supports filtering and metadata

### Why bge-small-en-v1.5?

- Good balance of quality vs speed
- 384 dimensions (compact)
- Strong performance on technical content
- MIT licensed

### Schema Design

Uses explicit PyArrow schema to handle empty lists:

```python
schema = pa.schema([
    pa.field("content", pa.string()),
    pa.field("vector", pa.list_(pa.float32(), 384)),
    pa.field("related_symbols", pa.list_(pa.string())),
    # ...
])
```

### Hook Integration

The hook outputs JSON with `additionalContext`:

```json
{
  "additionalContext": "[RAG Knowledge Base Results]\n- [category] content\n..."
}
```

Claude Code injects this into the conversation context before processing.

## Maintenance

### Adding Knowledge Sources

1. **From documentation**: Read docs, add key insights with `rag_add.py`
2. **From tests**: Extract edge cases and verified behaviors
3. **From assembly**: Analyze Godbolt output for performance insights
4. **From conversations**: When users discover issues, add corrections

### Verifying Entries

Always verify content before adding:
- Check against source code
- Check against README/docs
- Test the behavior if possible
- Set appropriate confidence level

### Removing Bad Entries

If an entry is wrong:
```bash
python rag_delete.py --category <cat> --contains "unique substring"
```

Then add the corrected version.

## Current Stats

- **60 entries** across 7 categories
- Sources: code parsing, documentation, unit tests, assembly analysis
- Covers: API signatures, usage patterns, gotchas, performance, semantics

# RAG Knowledge System

This is a vector-based knowledge retrieval system I built to reduce LLM hallucinations when working with the ilp_for library. It automatically provides relevant context to Claude Code when you ask about the library.

## Architecture

```
knowledge/
├── db/                               # LanceDB vector database (gitignored)
├── hooks/
│   └── rag_query_hook.py             # Claude Code hook (committable)
├── scripts/
│   ├── config.py                     # Centralized configuration
│   ├── ingest_config.py              # Library-specific parsing patterns
│   ├── import_knowledge.py           # Build database from seed + source
│   ├── rag_ingest.py                 # Parse source code for API signatures
│   ├── rag_add.py                    # Add individual insights
│   ├── rag_query.py                  # Query the knowledge base
│   └── rag_delete.py                 # Remove incorrect entries
├── tests/
│   ├── rag_evaluation.json           # Test cases (retrieval quality)
│   ├── code_generation_evaluation.json  # Test cases (code synthesis)
│   ├── evaluate_custom.py            # Custom evaluation (free, fast)
│   ├── evaluate_ragas.py             # RAGAS evaluation (answer quality)
│   ├── evaluate_codegen.py           # Code generation evaluation (synthesis)
│   ├── run_all_evals.sh              # Unified test runner
│   └── results/                      # Evaluation results with timestamps
├── knowledge_seed.json               # Curated knowledge (committed)
├── setup.sh                          # One-command setup script
├── CLAUDE.md                         # Instructions for Claude (committed)
└── README.md                         # This file
```

## How It Works

### 1. Vector Storage (LanceDB)

Knowledge is stored as vector embeddings in a LanceDB database. Each entry has:

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

I'm using `BAAI/bge-small-en-v1.5` via sentence-transformers:
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

The hook ([hooks/rag_query_hook.py](hooks/rag_query_hook.py)) triggers on keywords like:
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

# Build database from seed file + source code
python scripts/import_knowledge.py
```

This imports the curated insights from [knowledge_seed.json](knowledge_seed.json) and parses API signatures from [ilp_for.hpp](../ilp_for.hpp).

### Claude Code Hook

The hook auto-queries when you ask about ilp_for:

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

### Evaluate RAG Quality

I've set up three types of evaluations you can run:

```bash
cd knowledge/tests

# Run all evaluations at once
./run_all_evals.sh

# Or run individually:

# 1. Custom evaluation (free, fast - retrieval quality)
python evaluate_custom.py --verbose

# 2. RAGAS evaluation (LLM-based - answer quality)
export ANTHROPIC_API_KEY=your_key_here
python evaluate_ragas.py --verbose

# 3. Code generation evaluation (LLM-based - synthesis quality)
python evaluate_codegen.py --verbose

# Run specific test only
python evaluate_codegen.py --test-id codegen_003
```

#### 1. Custom Evaluation (Retrieval Quality)

This tests whether the RAG retrieves the right knowledge chunks:
- Queries RAG for each test question
- Checks if expected categories and symbols were retrieved
- Scores: 0.0 (miss), 0.5 (partial), 1.0 (perfect)
- Fast (~30 seconds for 12 tests)
- No API costs
- **Use for**: Regular testing, CI/CD

**Test Cases**: [tests/rag_evaluation.json](tests/rag_evaluation.json)
- 12 questions: basic, intermediate, and advanced
- Covers: API syntax, control flow, debugging, performance, edge cases

#### 2. RAGAS Evaluation (Answer Quality)

This tests whether answers are faithful and relevant:
- Generates answers using Claude API with RAG context
- Measures 4 industry-standard metrics:
  - **Faithfulness** (0-1): Is the answer grounded in retrieved context?
  - **Answer Relevancy** (0-1): Does the answer address the question?
  - **Context Precision** (0-1): Are relevant contexts ranked higher?
  - **Context Recall** (0-1): Did we retrieve all necessary context?
- Requires LLM API calls (~$0.05-0.10 per run)
- **Use for**: Pre-release validation, major knowledge base changes

#### 3. Code Generation Evaluation (Synthesis Quality)

This tests whether the LLM can write correct code using ONLY RAG context:
- Queries RAG with task description
- Generates code via Claude API with strict prompting
- Compiles with g++ and runs test cases
- Checks code quality (patterns, anti-patterns)
- Scoring:
  - **Compilation** (30%): Compiles without errors
  - **Correctness** (40%): Passes all test cases
  - **Quality** (30%): Follows best practices, avoids pitfalls
- Success criteria:
  - Compilation rate ≥ 90%
  - Correctness rate ≥ 80%
  - Average score ≥ 0.8
- **Use for**: End-to-end validation that RAG provides sufficient knowledge

**Test Cases**: [tests/code_generation_evaluation.json](tests/code_generation_evaluation.json)
- 8 scenarios: easy (2), medium (4), hard (2)
- Tests: ILP_BREAK, ILP_RETURN, nested loops, range loops, LoopType selection
- Each test has 3-5 input/output test cases
- Validates compilation, correctness, and quality

**Results** are saved to `tests/results/` with timestamps so you can track improvement over time.

## Porting to Another Library

I designed this system to be library-agnostic. If you want to use it with a different library:

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

Replace the ilp_for-specific content with your library's notes.

### 5. Re-run Setup

```bash
python scripts/rag_setup.py    # Creates new tables
python scripts/rag_ingest.py   # Parses your source
```

## Technical Details

### Why LanceDB?

I went with LanceDB because:
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

I use an explicit PyArrow schema to handle empty lists:

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

## MCP Server

There's also an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server if you want to use this with other AI tools beyond Claude Code.

### Setup

Just run the setup script - it creates `.mcp.json` in the project root:

```bash
cd knowledge && ./setup.sh
```

The MCP server uses the same Python environment as the rest of the knowledge system.

### Available Tools

| Tool | Description |
|------|-------------|
| `query_ilp_knowledge` | Search the knowledge base with natural language queries |
| `list_ilp_categories` | List all available knowledge categories |

### query_ilp_knowledge

Searches the knowledge base for documentation, examples, gotchas, and performance info.

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `query` | string | required | Natural language search query (e.g., "how to use ILP_BREAK") |
| `category` | string | "all" | Filter by category: `all`, `semantics`, `gotchas`, `performance`, `usage_patterns`, `compiler`, `api_signatures`, `antipatterns` |
| `limit` | int | 5 | Number of results to return (max 15) |
| `rerank` | bool | true | Use cross-encoder reranking for better precision |

**Example usage:**
```
# In Claude Code or any MCP-compatible tool:
Use the query_ilp_knowledge tool to search for "ILP_RETURN semantics"
```

**Response format:**
```
[1] [gotchas] (confidence: 0.9)
    Symbols: ILP_RETURN, ILP_END_RETURN
    ILP_END_RETURN must be used instead of ILP_END when ILP_RETURN appears in the loop body...

[2] [semantics] (confidence: 0.9)
    Symbols: ILP_BREAK, ILP_RETURN
    ILP_BREAK exits the loop, ILP_RETURN(x) exits the enclosing function and returns x...
```

### list_ilp_categories

Returns all available knowledge categories with descriptions:

```
ILP_FOR Knowledge Base Categories:

  semantics: How ILP_FOR works internally, iteration order, variable capture, lambda mechanics
  gotchas: Common mistakes, pitfalls, and things that trip people up
  performance: Benchmarks, speedup data, assembly analysis, optimization tips
  usage_patterns: Examples, patterns, how-to guides, migration from standard loops
  compiler: Compiler flags, error messages, debug modes, CPU targeting
  api_signatures: Complete list of macros, LoopType enum values, function signatures
  antipatterns: When NOT to use ILP_FOR, misuse cases
```

### Configuration

The `.mcp.json` file is auto-generated by `setup.sh` with machine-specific paths:

```json
{
  "mcpServers": {
    "ilp-knowledge": {
      "command": "/path/to/knowledge/.venv/bin/python",
      "args": ["/path/to/knowledge/mcp_server.py"],
      "description": "ILP_FOR knowledge base - query documentation, examples, gotchas, performance info"
    }
  }
}
```

---

## Current Stats

- **90+ entries** across 7 categories (77 curated + 13 API signatures)
- **12 test cases** for evaluation (easy/medium/hard difficulty)
- **88% average score** on custom evaluation (100% pass rate)
- Sources: code parsing, documentation, unit tests, assembly analysis
- Covers: API signatures, usage patterns, gotchas, performance, semantics, clang-tidy tooling
- Evaluation: Custom (free/fast) + RAGAS (industry-standard)

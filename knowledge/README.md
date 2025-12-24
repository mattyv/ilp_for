# RAG Knowledge System

A vector-based knowledge retrieval system to reduce LLM hallucinations when working with the ilp_for library. It automatically provides relevant context to Claude Code when you ask about the library.

## Setup

```bash
cd knowledge && ./setup.sh
```

This creates the venv, builds the database from [knowledge_seed.json](knowledge_seed.json) + [ilp_for.hpp](../ilp_for.hpp), sets up the Claude Code hook, and configures the MCP server.

## Usage

```bash
cd knowledge/scripts && source ../.venv/bin/activate

python rag_query.py "how do I use ILP_BREAK"           # Search all categories
python rag_query.py "break semantics" --category semantics  # Filter by category
python rag_add.py --category gotchas --content "..." --symbols ILP_RETURN  # Add knowledge
python rag_delete.py --category gotchas --contains "substring"  # Remove entries
```

## Evaluation

```bash
cd knowledge/tests && ./run_all_evals.sh
```

Three eval types: custom (free/fast retrieval checks), RAGAS (LLM-based answer quality), and codegen (compiles + runs generated code). Current scores: **88% average, 100% pass rate**.

## MCP Server

For use with AI tools beyond Claude Code. Setup creates `.mcp.json` in project root.

| Tool | Description |
|------|-------------|
| `query_ilp_knowledge` | Search with natural language (query, category, limit, rerank) |
| `list_ilp_categories` | List available categories |

## Porting to Another Library

1. Edit `scripts/config.py` (categories)
2. Edit `scripts/ingest_config.py` (source parsing patterns)
3. Edit `hooks/rag_query_hook.py` (trigger keywords)
4. Run `./setup.sh`

## Architecture

```
knowledge/
├── db/                    # LanceDB vector database (gitignored)
├── hooks/                 # Claude Code hook
├── scripts/               # Query, add, delete, ingest scripts
├── tests/                 # Evaluation suite
├── knowledge_seed.json    # Curated knowledge (committed)
├── mcp_server.py          # MCP server
└── setup.sh               # One-command setup
```

Uses LanceDB for vector storage, `BAAI/bge-small-en-v1.5` for embeddings (384-dim). Categories: `api_signatures`, `usage_patterns`, `antipatterns`, `gotchas`, `performance`, `compiler`, `semantics`.

---

**90+ entries** | 7 categories | 12 test cases | Sources: code parsing, docs, tests, assembly analysis

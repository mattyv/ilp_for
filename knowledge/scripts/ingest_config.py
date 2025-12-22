"""Library-specific ingestion configuration for ilp_for.

Customize this file when adapting the RAG system for a different library.
"""

INGEST_CONFIG = {
    # Files to parse for API extraction
    "files": [
        {"path": "ilp_for.hpp"},
    ],

    # Patterns for extracting macros
    # Each pattern needs: pattern (regex), name_group, type
    # Optional: signature_group (default 0 = full match), prefix_filter
    "macro_patterns": [
        {
            "pattern": r'#define\s+(ILP_\w+)\s*(\([^)]*\))?',
            "name_group": 1,
            "signature_group": 0,
            "type": "macro",
            "prefix_filter": "ILP_",
        },
    ],

    # For ilp_for, the main API is macros. Function extraction is disabled
    # to avoid false positives from comments and complex template syntax.
    "function_patterns": [],
}

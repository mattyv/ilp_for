#!/usr/bin/env python3
"""CLI for deleting insights from the knowledge base."""
import lancedb
import argparse

from config import DB_PATH, CATEGORIES


def escape_sql_string(s: str) -> str:
    """Escape a string for use in SQL-like filter expressions.

    Escapes single quotes, backslashes, and other special characters.
    """
    # Escape backslashes first (order matters)
    s = s.replace("\\", "\\\\")
    # Escape single quotes
    s = s.replace("'", "''")
    return s


def delete_by_content(category: str, content_substring: str):
    """Delete entries containing the substring in their content."""
    db = lancedb.connect(DB_PATH)
    table = db.open_table(category)

    # Find matching entries
    arrow_table = table.to_arrow()
    to_delete = []
    for i in range(arrow_table.num_rows):
        content = arrow_table['content'][i].as_py()
        if content_substring in content:
            to_delete.append(content)

    if not to_delete:
        print(f"No entries found containing '{content_substring}'")
        return

    print(f"Found {len(to_delete)} entries to delete:")
    for c in to_delete:
        # Only add "..." if content is actually truncated
        preview = c[:80] + "..." if len(c) > 80 else c
        print(f"  - {preview}")

    # Delete using SQL-like filter
    for content in to_delete:
        escaped = escape_sql_string(content)
        table.delete(f"content = '{escaped}'")

    # Rebuild FTS index after deletion
    try:
        table.create_fts_index("content", replace=True)
    except Exception:
        pass  # FTS update is best-effort

    print(f"Deleted {len(to_delete)} entries from '{category}'")


def main():
    parser = argparse.ArgumentParser(description='Delete insights from knowledge base')
    parser.add_argument('--category', '-c', required=True,
                        choices=CATEGORIES,
                        help='Category to delete from')
    parser.add_argument('--contains', required=True,
                        help='Substring to match in content')
    args = parser.parse_args()

    delete_by_content(args.category, args.contains)


if __name__ == "__main__":
    main()

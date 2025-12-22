#!/usr/bin/env python3
"""One-time setup for the RAG knowledge database."""
import lancedb
import pyarrow as pa
from sentence_transformers import SentenceTransformer

from config import DB_PATH, EMBEDDING_MODEL, CATEGORIES


def setup():
    db = lancedb.connect(DB_PATH)
    model = SentenceTransformer(EMBEDDING_MODEL)

    # Get embedding dimension
    sample_embedding = model.encode("sample")
    embedding_dim = len(sample_embedding)

    # Define schema with proper types
    schema = pa.schema([
        pa.field("content", pa.string()),
        pa.field("vector", pa.list_(pa.float32(), embedding_dim)),
        pa.field("source", pa.string()),
        pa.field("validated_by", pa.string()),
        pa.field("confidence", pa.float64()),
        pa.field("related_symbols", pa.list_(pa.string())),
        pa.field("tags", pa.list_(pa.string())),
        pa.field("created_at", pa.string()),
    ])

    for cat in CATEGORIES:
        if cat not in db.list_tables():
            # Create empty table with explicit schema
            db.create_table(cat, schema=schema)

    print(f"Database initialized at {DB_PATH}")
    print(f"  Tables: {db.list_tables()}")


if __name__ == "__main__":
    setup()

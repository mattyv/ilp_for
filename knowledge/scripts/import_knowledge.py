#!/usr/bin/env python3
"""Import knowledge from JSON seed file into LanceDB, then ingest code signatures."""
import lancedb
import json
import pyarrow as pa
from pathlib import Path
from sentence_transformers import SentenceTransformer
import sys

def main():
    script_dir = Path(__file__).parent
    seed_file = script_dir.parent / 'knowledge_seed.json'
    db_path = script_dir.parent / 'db'

    if not seed_file.exists():
        print(f"Error: Seed file not found: {seed_file}")
        return 1

    # Load seed data
    with open(seed_file) as f:
        entries = json.load(f)

    print(f"Loading {len(entries)} curated entries from {seed_file}")

    # Initialize embedding model
    print("Loading embedding model...")
    model = SentenceTransformer('BAAI/bge-small-en-v1.5')
    embedding_dim = 384

    # Connect to database
    db = lancedb.connect(str(db_path))

    # Define schema
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

    # Group by category
    by_category = {}
    for entry in entries:
        cat = entry.pop('category')
        by_category.setdefault(cat, []).append(entry)

    # Import each category
    for category, cat_entries in sorted(by_category.items()):
        print(f"Importing {len(cat_entries)} entries into '{category}'...")

        # Generate embeddings
        contents = [e['content'] for e in cat_entries]
        vectors = model.encode(contents, show_progress_bar=False)

        # Add vectors to entries
        for entry, vector in zip(cat_entries, vectors):
            entry['vector'] = vector.tolist()

        # Create table
        db.create_table(category, cat_entries, schema=schema, mode='overwrite')

    print(f"\nSuccessfully imported {len(entries)} curated entries into {len(by_category)} tables")

    # Now ingest code signatures
    print("\nIngesting API signatures from source code...")
    sys.path.insert(0, str(script_dir))
    from rag_ingest import ingest_source_code

    code_count = ingest_source_code()
    print(f"Added {code_count} API signatures from source code")

    print(f"\nTotal knowledge base ready: {len(entries) + code_count} entries")

if __name__ == '__main__':
    exit(main() or 0)

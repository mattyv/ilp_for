#!/usr/bin/env python3
"""Import knowledge from JSON seed file into LanceDB, then ingest code signatures."""
import lancedb
import json
import pyarrow as pa
from pathlib import Path
from sentence_transformers import SentenceTransformer
import sys

from config import EMBEDDING_MODEL, CATEGORIES

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
    model = SentenceTransformer(EMBEDDING_MODEL)
    embedding_dim = model.get_sentence_embedding_dimension()

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

    # Group by category (without mutating original entries)
    by_category = {}
    skipped = 0
    for entry in entries:
        cat = entry.get('category')
        if cat not in CATEGORIES:
            print(f"  Warning: Unknown category '{cat}' in seed file, skipping entry")
            skipped += 1
            continue
        # Copy entry without 'category' field
        entry_copy = {k: v for k, v in entry.items() if k != 'category'}
        by_category.setdefault(cat, []).append(entry_copy)

    if skipped > 0:
        print(f"  Skipped {skipped} entries with unknown categories")

    # Ensure all categories exist (even if empty in seed)
    for cat in CATEGORIES:
        by_category.setdefault(cat, [])

    # Import each category
    for category, cat_entries in sorted(by_category.items()):
        if cat_entries:
            print(f"Importing {len(cat_entries)} entries into '{category}'...")

            # Generate embeddings
            contents = [e['content'] for e in cat_entries]
            vectors = model.encode(contents, show_progress_bar=False)

            # Add vectors to entries
            for entry, vector in zip(cat_entries, vectors):
                entry['vector'] = vector.tolist()

            # Create table with data
            table = db.create_table(category, cat_entries, schema=schema, mode='overwrite')

            # Create FTS index for hybrid search
            try:
                table.create_fts_index("content", replace=True)
            except Exception as e:
                print(f"  Warning: Could not create FTS index for '{category}': {e}")
        else:
            # Create empty table
            print(f"Creating empty table '{category}'...")
            db.create_table(category, [], schema=schema, mode='overwrite')

    print(f"\nSuccessfully imported {len(entries)} curated entries into {len(by_category)} tables")
    print("FTS indexes created for hybrid search")

    # Now ingest code signatures
    print("\nIngesting API signatures from source code...")
    sys.path.insert(0, str(script_dir))
    from rag_ingest import ingest

    ingest()
    print(f"\nKnowledge base ready with curated insights + API signatures")

if __name__ == '__main__':
    exit(main() or 0)

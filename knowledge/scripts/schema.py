"""Schema definitions for the RAG knowledge system."""
from pydantic import BaseModel
from enum import Enum
from datetime import datetime


class InsightCategory(str, Enum):
    API_SIGNATURE = "api_signatures"      # Exact macro/function signatures
    USAGE_PATTERN = "usage_patterns"      # Correct usage examples
    ANTIPATTERN = "antipatterns"          # What NOT to do
    GOTCHA = "gotchas"                    # Subtle issues/edge cases
    PERFORMANCE = "performance"           # When to use, benchmarks
    COMPILER = "compiler"                 # GCC vs Clang differences
    SEMANTIC = "semantics"                # What the macros actually expand to


class Insight(BaseModel):
    content: str
    category: InsightCategory
    source: str  # "docs", "code", "conversation", "test"
    validated_by: str  # "human", "compiles", "test_passes"
    confidence: float  # 0.0-1.0
    related_symbols: list[str]  # ["ILP_FOR", "ILP_BREAK", etc.]
    tags: list[str]
    created_at: datetime = datetime.now()

    class Config:
        use_enum_values = True

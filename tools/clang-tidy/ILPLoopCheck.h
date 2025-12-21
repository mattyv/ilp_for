// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#ifndef ILP_LOOP_CHECK_H
#define ILP_LOOP_CHECK_H

#include "clang-tidy/ClangTidyCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <optional>
#include <string>
#include <utility>

namespace clang::tidy::ilp {

    /// Detected loop type patterns matching ilp::LoopType enum
    enum class DetectedLoopType {
        Sum,        // acc += val
        DotProduct, // acc += a * b (FMA pattern)
        Search,     // early exit (if...break/return)
        Copy,       // dst[i] = src[i]
        Transform,  // dst[i] = f(src[i])
        Multiply,   // acc *= val
        Divide,     // x / y
        Sqrt,       // sqrt(x)
        MinMax,     // min(a,b) / max(a,b)
        Bitwise,    // acc &= x, |=, ^=
        Shift,      // x << n, x >> n
        Unknown     // Could not determine
    };

    /// Parsed macro arguments for fix generation
    struct MacroArgs {
        std::string macroName;      // "ILP_FOR" or "ILP_FOR_RANGE"
        std::string varDecl;        // "auto i"
        std::string start;          // "0uz" or range for RANGE variant
        std::string end;            // "n"
        std::string lastArg;        // "4" (N) or "DotProduct" (LoopType)
        CharSourceRange macroRange; // Full macro invocation range
    };

    /// Analysis result for a loop body
    struct LoopAnalysis {
        DetectedLoopType detectedType = DetectedLoopType::Unknown;

        // Evidence flags for each pattern
        bool hasCompoundAdd = false; // +=
        bool hasCompoundMul = false; // *=
        bool hasMulInAdd = false;    // acc += a * b
        bool hasEarlyExit = false;   // if(...) break/return
        bool hasCopy = false;        // dst = src (different arrays)
        bool hasTransform = false;   // dst = f(src)
        bool hasDivision = false;    // a / b
        bool hasSqrt = false;        // sqrt(x)
        bool hasMinMax = false;      // std::min/max
        bool hasBitwise = false;     // &=, |=, ^=
        bool hasShift = false;       // <<, >>

        // Type information for N computation
        QualType accumulatorType;
        unsigned typeSize = 0; // 1, 2, 4, 8 bytes
        bool isFloatingPoint = false;

        // Determine the primary loop type from evidence
        void computeLoopType();
    };

    /// clang-tidy check that analyzes ILP_FOR loop bodies
    class ILPLoopCheck : public ClangTidyCheck {
      public:
        ILPLoopCheck(StringRef Name, ClangTidyContext* Context);

        void registerMatchers(ast_matchers::MatchFinder* Finder) override;
        void check(const ast_matchers::MatchFinder::MatchResult& Result) override;
        void storeOptions(ClangTidyOptions::OptionMap& Opts) override;

      private:
        // Configuration
        std::string TargetCPU;
        bool PreferPortableFix;

        // Analysis helpers
        LoopAnalysis analyzeLoopBody(const Stmt* Body, ASTContext& Context);
        void analyzeStatement(const Stmt* S, LoopAnalysis& Analysis, ASTContext& Context, unsigned Depth = 0);
        void analyzeBinaryOperator(const BinaryOperator* BO, LoopAnalysis& Analysis);
        void analyzeCompoundAssign(const CompoundAssignOperator* CAO, LoopAnalysis& Analysis);
        void analyzeCallExpr(const CallExpr* CE, LoopAnalysis& Analysis);

        // N value lookup
        int lookupOptimalN(DetectedLoopType Type, const LoopAnalysis& Analysis);

        // Find the pattern needing the highest N - that's the bottleneck
        std::pair<DetectedLoopType, int> computeOptimalN(const LoopAnalysis& Analysis);

        // Diagnostic helpers
        std::string getLoopTypeName(DetectedLoopType Type);

        // Fix generation helpers
        std::optional<MacroArgs> extractMacroArgs(const CallExpr* LoopCall, const SourceManager& SM,
                                                  const LangOptions& LO);
        std::string buildPortableFix(const MacroArgs& Args, DetectedLoopType Type, const LoopAnalysis& Analysis);
    };

} // namespace clang::tidy::ilp

#endif // ILP_LOOP_CHECK_H

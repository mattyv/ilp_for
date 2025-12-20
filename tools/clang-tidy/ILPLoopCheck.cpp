// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#include "ILPLoopCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include <vector>

using namespace clang::ast_matchers;

namespace clang::tidy::ilp {

    // CPU profile N values - replicates ilp_for/cpu_profiles/ilp_cpu_skylake.hpp
    // TODO: Make configurable per target CPU
    namespace cpu_profiles {

        struct CPUProfile {
            // Sum
            int N_SUM_1, N_SUM_2, N_SUM_4I, N_SUM_8I, N_SUM_4F, N_SUM_8F;
            // DotProduct
            int N_DOTPRODUCT_4, N_DOTPRODUCT_8;
            // Search
            int N_SEARCH_1, N_SEARCH_2, N_SEARCH_4, N_SEARCH_8;
            // Copy
            int N_COPY_1, N_COPY_2, N_COPY_4, N_COPY_8;
            // Transform
            int N_TRANSFORM_1, N_TRANSFORM_2, N_TRANSFORM_4, N_TRANSFORM_8;
            // Multiply
            int N_MULTIPLY_4F, N_MULTIPLY_8F, N_MULTIPLY_4I, N_MULTIPLY_8I;
            // Divide
            int N_DIVIDE_4F, N_DIVIDE_8F;
            // Sqrt
            int N_SQRT_4F, N_SQRT_8F;
            // MinMax
            int N_MINMAX_1, N_MINMAX_2, N_MINMAX_4I, N_MINMAX_8I, N_MINMAX_4F, N_MINMAX_8F;
            // Bitwise
            int N_BITWISE_1, N_BITWISE_2, N_BITWISE_4, N_BITWISE_8;
            // Shift
            int N_SHIFT_1, N_SHIFT_2, N_SHIFT_4, N_SHIFT_8;
        };

        // Skylake profile (default)
        constexpr CPUProfile Skylake = {
            // Sum - Integer: L=1, TPC=3 → 3
            .N_SUM_1 = 3,
            .N_SUM_2 = 3,
            .N_SUM_4I = 3,
            .N_SUM_8I = 3,
            // Sum - FP: L=4, TPC=2 → 8
            .N_SUM_4F = 8,
            .N_SUM_8F = 8,
            // DotProduct - FMA: L=4, TPC=2 → 8
            .N_DOTPRODUCT_4 = 8,
            .N_DOTPRODUCT_8 = 8,
            // Search - compare + branch
            .N_SEARCH_1 = 4,
            .N_SEARCH_2 = 4,
            .N_SEARCH_4 = 4,
            .N_SEARCH_8 = 4,
            // Copy - memory bandwidth limited
            .N_COPY_1 = 8,
            .N_COPY_2 = 4,
            .N_COPY_4 = 4,
            .N_COPY_8 = 4,
            // Transform - memory + compute balanced
            .N_TRANSFORM_1 = 4,
            .N_TRANSFORM_2 = 4,
            .N_TRANSFORM_4 = 4,
            .N_TRANSFORM_8 = 4,
            // Multiply
            .N_MULTIPLY_4F = 8,
            .N_MULTIPLY_8F = 8,
            .N_MULTIPLY_4I = 10,
            .N_MULTIPLY_8I = 4,
            // Divide - high latency
            .N_DIVIDE_4F = 2,
            .N_DIVIDE_8F = 2,
            // Sqrt - high latency
            .N_SQRT_4F = 2,
            .N_SQRT_8F = 2,
            // MinMax
            .N_MINMAX_1 = 2,
            .N_MINMAX_2 = 2,
            .N_MINMAX_4I = 2,
            .N_MINMAX_8I = 2,
            .N_MINMAX_4F = 8,
            .N_MINMAX_8F = 8,
            // Bitwise - L=1, TPC=3 → 3
            .N_BITWISE_1 = 3,
            .N_BITWISE_2 = 3,
            .N_BITWISE_4 = 3,
            .N_BITWISE_8 = 3,
            // Shift - L=1, TPC=2 → 2
            .N_SHIFT_1 = 2,
            .N_SHIFT_2 = 2,
            .N_SHIFT_4 = 2,
            .N_SHIFT_8 = 2,
        };

        // Apple M1 profile
        constexpr CPUProfile AppleM1 = {
            // Sum - Integer: L=1, TPC=4 → 4
            .N_SUM_1 = 4,
            .N_SUM_2 = 4,
            .N_SUM_4I = 4,
            .N_SUM_8I = 4,
            // Sum - FP: L=3, TPC=4 → 12 (capped to 8)
            .N_SUM_4F = 8,
            .N_SUM_8F = 8,
            // DotProduct - FMA: L=4, TPC=4 → 16 (capped to 8)
            .N_DOTPRODUCT_4 = 8,
            .N_DOTPRODUCT_8 = 8,
            // Search
            .N_SEARCH_1 = 4,
            .N_SEARCH_2 = 4,
            .N_SEARCH_4 = 4,
            .N_SEARCH_8 = 4,
            // Copy
            .N_COPY_1 = 8,
            .N_COPY_2 = 4,
            .N_COPY_4 = 4,
            .N_COPY_8 = 4,
            // Transform
            .N_TRANSFORM_1 = 4,
            .N_TRANSFORM_2 = 4,
            .N_TRANSFORM_4 = 4,
            .N_TRANSFORM_8 = 4,
            // Multiply
            .N_MULTIPLY_4F = 8,
            .N_MULTIPLY_8F = 8,
            .N_MULTIPLY_4I = 4,
            .N_MULTIPLY_8I = 4,
            // Divide
            .N_DIVIDE_4F = 2,
            .N_DIVIDE_8F = 2,
            // Sqrt
            .N_SQRT_4F = 2,
            .N_SQRT_8F = 2,
            // MinMax
            .N_MINMAX_1 = 4,
            .N_MINMAX_2 = 4,
            .N_MINMAX_4I = 4,
            .N_MINMAX_8I = 4,
            .N_MINMAX_4F = 8,
            .N_MINMAX_8F = 8,
            // Bitwise
            .N_BITWISE_1 = 4,
            .N_BITWISE_2 = 4,
            .N_BITWISE_4 = 4,
            .N_BITWISE_8 = 4,
            // Shift
            .N_SHIFT_1 = 4,
            .N_SHIFT_2 = 4,
            .N_SHIFT_4 = 4,
            .N_SHIFT_8 = 4,
        };

        const CPUProfile& getProfile(StringRef CPU) {
            if (CPU == "apple_m1" || CPU == "m1")
                return AppleM1;
            // Default to Skylake
            return Skylake;
        }

    } // namespace cpu_profiles

    void LoopAnalysis::computeLoopType() {
        // Priority order (from ilp_for README decision tree):
        // 1. Early exit → Search
        // 2. Sqrt → Sqrt
        // 3. Division → Divide
        // 4. FMA pattern (acc += a * b) → DotProduct
        // 5. Compound multiply → Multiply
        // 6. Min/Max → MinMax
        // 7. Bitwise ops → Bitwise
        // 8. Shift ops → Shift
        // 9. Transform → Transform
        // 10. Copy → Copy
        // 11. Compound add → Sum

        if (hasEarlyExit) {
            detectedType = DetectedLoopType::Search;
        } else if (hasSqrt) {
            detectedType = DetectedLoopType::Sqrt;
        } else if (hasDivision) {
            detectedType = DetectedLoopType::Divide;
        } else if (hasMulInAdd) {
            detectedType = DetectedLoopType::DotProduct;
        } else if (hasCompoundMul) {
            detectedType = DetectedLoopType::Multiply;
        } else if (hasMinMax) {
            detectedType = DetectedLoopType::MinMax;
        } else if (hasBitwise) {
            detectedType = DetectedLoopType::Bitwise;
        } else if (hasShift) {
            detectedType = DetectedLoopType::Shift;
        } else if (hasTransform) {
            detectedType = DetectedLoopType::Transform;
        } else if (hasCopy) {
            detectedType = DetectedLoopType::Copy;
        } else if (hasCompoundAdd) {
            detectedType = DetectedLoopType::Sum;
        } else {
            detectedType = DetectedLoopType::Unknown;
        }
    }

    ILPLoopCheck::ILPLoopCheck(StringRef Name, ClangTidyContext* Context)
        : ClangTidyCheck(Name, Context), TargetCPU(Options.get("TargetCPU", "skylake")),
          PreferPortableFix(Options.get("PreferPortableFix", true)) {}

    void ILPLoopCheck::storeOptions(ClangTidyOptions::OptionMap& Opts) {
        Options.store(Opts, "TargetCPU", TargetCPU);
        Options.store(Opts, "PreferPortableFix", PreferPortableFix);
    }

    void ILPLoopCheck::registerMatchers(MatchFinder* Finder) {
        // Match lambda expressions that are passed to functions starting with "for_loop"
        // This catches the expanded form of ILP_FOR and ILP_FOR_AUTO macros
        Finder->addMatcher(
            callExpr(callee(functionDecl(matchesName("for_loop.*"))), hasArgument(2, lambdaExpr().bind("loopBody")))
                .bind("forLoopCall"),
            this);

        // Also match direct calls to ilp::for_loop
        Finder->addMatcher(
            callExpr(callee(functionDecl(hasName("ilp::for_loop"))), hasArgument(2, lambdaExpr().bind("loopBody")))
                .bind("forLoopCall"),
            this);
    }

    void ILPLoopCheck::check(const MatchFinder::MatchResult& Result) {
        const auto* LoopCall = Result.Nodes.getNodeAs<CallExpr>("forLoopCall");
        const auto* LoopBody = Result.Nodes.getNodeAs<LambdaExpr>("loopBody");

        if (!LoopCall || !LoopBody)
            return;

        // Get the lambda body
        const Stmt* Body = LoopBody->getBody();
        if (!Body)
            return;

        // Analyze the loop body
        LoopAnalysis Analysis = analyzeLoopBody(Body, *Result.Context);

        // Skip if we couldn't determine the type
        if (Analysis.detectedType == DetectedLoopType::Unknown)
            return;

        // Get optimal N for this pattern
        int OptimalN = lookupOptimalN(Analysis.detectedType, Analysis);

        // Get source locations
        SourceLocation Loc = LoopCall->getBeginLoc();
        const SourceManager& SM = *Result.SourceManager;

        // Build diagnostic message
        std::string LoopTypeName = getLoopTypeName(Analysis.detectedType);

        // Try to extract macro arguments for auto-fix
        auto MaybeArgs = extractMacroArgs(LoopCall, SM, getLangOpts());

        // Check if already using ILP_FOR_AUTO - skip if so
        if (MaybeArgs && (MaybeArgs->macroName == "ILP_FOR_AUTO" || MaybeArgs->macroName == "ILP_FOR_RANGE_AUTO")) {
            return; // Already using auto variant, nothing to fix
        }

        // Emit diagnostic with fix if we can generate one
        if (MaybeArgs) {
            std::string FixText = buildPortableFix(*MaybeArgs, Analysis.detectedType);
            if (!FixText.empty()) {
                diag(Loc, "Loop body contains %0 pattern")
                    << LoopTypeName << FixItHint::CreateReplacement(MaybeArgs->macroRange, FixText);
            } else {
                diag(Loc, "Loop body contains %0 pattern") << LoopTypeName;
            }
        } else {
            // No macro args available, just emit diagnostic without fix
            diag(Loc, "Loop body contains %0 pattern") << LoopTypeName;
        }

        // Add note with fix options
        diag(Loc, "Portable fix: use ILP_FOR_AUTO with LoopType::%0", DiagnosticIDs::Note) << LoopTypeName;

        diag(Loc, "Architecture-specific fix for %0: use ILP_FOR with N=%1", DiagnosticIDs::Note)
            << TargetCPU << OptimalN;
    }

    LoopAnalysis ILPLoopCheck::analyzeLoopBody(const Stmt* Body, ASTContext& Context) {
        LoopAnalysis Analysis;

        // Recursively analyze all statements in the body
        analyzeStatement(Body, Analysis, Context);

        // Determine the loop type from evidence
        Analysis.computeLoopType();

        return Analysis;
    }

    void ILPLoopCheck::analyzeStatement(const Stmt* S, LoopAnalysis& Analysis, ASTContext& Context) {
        if (!S)
            return;

        // Check for early exit patterns (Search)
        if (isa<BreakStmt>(S) || isa<ReturnStmt>(S)) {
            Analysis.hasEarlyExit = true;
        }

        // Check binary operators
        if (const auto* BO = dyn_cast<BinaryOperator>(S)) {
            analyzeBinaryOperator(BO, Analysis);
        }

        // Check compound assignment operators
        if (const auto* CAO = dyn_cast<CompoundAssignOperator>(S)) {
            analyzeCompoundAssign(CAO, Analysis);
        }

        // Check call expressions
        if (const auto* CE = dyn_cast<CallExpr>(S)) {
            analyzeCallExpr(CE, Analysis);
        }

        // Check for copy/transform pattern: assignment to array element
        if (const auto* BO = dyn_cast<BinaryOperator>(S)) {
            if (BO->isAssignmentOp() && !BO->isCompoundAssignmentOp()) {
                const Expr* LHS = BO->getLHS()->IgnoreParenImpCasts();
                const Expr* RHS = BO->getRHS()->IgnoreParenImpCasts();

                // Check if LHS is an array subscript (dst[i] = ...)
                if (isa<ArraySubscriptExpr>(LHS)) {
                    // Check if RHS is a function call - that's Transform
                    if (isa<CallExpr>(RHS)) {
                        Analysis.hasTransform = true;
                    }
                    // Check if RHS is also array subscript - that's Copy
                    else if (isa<ArraySubscriptExpr>(RHS)) {
                        Analysis.hasCopy = true;
                    }
                }
            }
        }

        // Recurse into child statements
        for (const Stmt* Child : S->children()) {
            if (Child)
                analyzeStatement(Child, Analysis, Context);
        }
    }

    void ILPLoopCheck::analyzeBinaryOperator(const BinaryOperator* BO, LoopAnalysis& Analysis) {
        BinaryOperator::Opcode Op = BO->getOpcode();

        // Division
        if (Op == BO_Div) {
            Analysis.hasDivision = true;
            QualType Ty = BO->getType();
            Analysis.accumulatorType = Ty;
            Analysis.typeSize = Ty->isFloatingType() ? (Ty->isFloat128Type() ? 16 : (Ty->isDoubleType() ? 8 : 4)) : 4;
            Analysis.isFloatingPoint = Ty->isFloatingType();
        }

        // Shift operations
        if (Op == BO_Shl || Op == BO_Shr) {
            Analysis.hasShift = true;
            QualType Ty = BO->getLHS()->getType();
            Analysis.typeSize = static_cast<unsigned>(
                BO->getLHS()->getType().getTypePtr()->isIntegerType() ? sizeof(int) : 4); // Approximate
        }
    }

    void ILPLoopCheck::analyzeCompoundAssign(const CompoundAssignOperator* CAO, LoopAnalysis& Analysis) {
        BinaryOperator::Opcode Op = CAO->getOpcode();
        QualType Ty = CAO->getLHS()->getType();

        // Track type info
        Analysis.accumulatorType = Ty;
        if (Ty->isBuiltinType()) {
            const auto* BT = Ty->getAs<BuiltinType>();
            if (BT) {
                switch (BT->getKind()) {
                case BuiltinType::Char_S:
                case BuiltinType::Char_U:
                case BuiltinType::SChar:
                case BuiltinType::UChar:
                    Analysis.typeSize = 1;
                    break;
                case BuiltinType::Short:
                case BuiltinType::UShort:
                    Analysis.typeSize = 2;
                    break;
                case BuiltinType::Int:
                case BuiltinType::UInt:
                case BuiltinType::Float:
                    Analysis.typeSize = 4;
                    break;
                case BuiltinType::Long:
                case BuiltinType::ULong:
                case BuiltinType::LongLong:
                case BuiltinType::ULongLong:
                case BuiltinType::Double:
                    Analysis.typeSize = 8;
                    break;
                default:
                    Analysis.typeSize = 4;
                }
            }
        }
        Analysis.isFloatingPoint = Ty->isFloatingType();

        switch (Op) {
        case BO_AddAssign:
        case BO_SubAssign:
            Analysis.hasCompoundAdd = true;
            // Check for FMA pattern: acc += a * b
            if (const auto* RHS = dyn_cast<BinaryOperator>(CAO->getRHS()->IgnoreParenImpCasts())) {
                if (RHS->getOpcode() == BO_Mul) {
                    Analysis.hasMulInAdd = true;
                }
            }
            break;

        case BO_MulAssign:
            Analysis.hasCompoundMul = true;
            break;

        case BO_AndAssign:
        case BO_OrAssign:
        case BO_XorAssign:
            Analysis.hasBitwise = true;
            break;

        case BO_ShlAssign:
        case BO_ShrAssign:
            Analysis.hasShift = true;
            break;

        default:
            break;
        }
    }

    void ILPLoopCheck::analyzeCallExpr(const CallExpr* CE, LoopAnalysis& Analysis) {
        const FunctionDecl* FD = CE->getDirectCallee();
        std::string Name;
        std::string QualifiedName;

        if (FD) {
            Name = FD->getNameAsString();
            QualifiedName = FD->getQualifiedNameAsString();
        } else {
            // For unresolved calls (e.g., templates), try to get name from callee expr
            if (const auto* DRE = dyn_cast<DeclRefExpr>(CE->getCallee()->IgnoreParenImpCasts())) {
                Name = DRE->getNameInfo().getAsString();
            }
        }

        // Check for sqrt (handles std::sqrt, ::sqrt, sqrtf, sqrtl)
        if (Name == "sqrt" || Name == "sqrtf" || Name == "sqrtl" || Name.find("sqrt") != std::string::npos ||
            QualifiedName.find("sqrt") != std::string::npos) {
            Analysis.hasSqrt = true;
            if (FD) {
                QualType RetTy = FD->getReturnType();
                Analysis.accumulatorType = RetTy;
                Analysis.typeSize = RetTy->isDoubleType() ? 8 : 4;
            } else {
                Analysis.typeSize = 4; // Default to float
            }
            Analysis.isFloatingPoint = true;
        }

        // Check for min/max (handles std::min, std::max, fmin, fmax, etc.)
        if (Name == "min" || Name == "max" || Name == "fmin" || Name == "fmax" || Name == "fminf" || Name == "fmaxf" ||
            QualifiedName.find("::min") != std::string::npos || QualifiedName.find("::max") != std::string::npos) {
            Analysis.hasMinMax = true;
            if (CE->getNumArgs() > 0) {
                QualType ArgTy = CE->getArg(0)->getType();
                Analysis.accumulatorType = ArgTy;
                Analysis.isFloatingPoint = ArgTy->isFloatingType();
                Analysis.typeSize = ArgTy->isDoubleType() ? 8 : 4;
            }
        }
    }

    int ILPLoopCheck::lookupOptimalN(DetectedLoopType Type, const LoopAnalysis& Analysis) {
        const auto& Profile = cpu_profiles::getProfile(TargetCPU);
        unsigned Size = Analysis.typeSize;
        bool FP = Analysis.isFloatingPoint;

        switch (Type) {
        case DetectedLoopType::Sum:
            if (Size == 1)
                return Profile.N_SUM_1;
            if (Size == 2)
                return Profile.N_SUM_2;
            if (Size == 4 && FP)
                return Profile.N_SUM_4F;
            if (Size == 4)
                return Profile.N_SUM_4I;
            if (Size == 8 && FP)
                return Profile.N_SUM_8F;
            if (Size == 8)
                return Profile.N_SUM_8I;
            return 4;

        case DetectedLoopType::DotProduct:
            if (Size == 4)
                return Profile.N_DOTPRODUCT_4;
            if (Size == 8)
                return Profile.N_DOTPRODUCT_8;
            return 8;

        case DetectedLoopType::Search:
            if (Size == 1)
                return Profile.N_SEARCH_1;
            if (Size == 2)
                return Profile.N_SEARCH_2;
            if (Size == 4)
                return Profile.N_SEARCH_4;
            if (Size == 8)
                return Profile.N_SEARCH_8;
            return 4;

        case DetectedLoopType::Copy:
            if (Size == 1)
                return Profile.N_COPY_1;
            if (Size == 2)
                return Profile.N_COPY_2;
            if (Size == 4)
                return Profile.N_COPY_4;
            if (Size == 8)
                return Profile.N_COPY_8;
            return 4;

        case DetectedLoopType::Transform:
            if (Size == 1)
                return Profile.N_TRANSFORM_1;
            if (Size == 2)
                return Profile.N_TRANSFORM_2;
            if (Size == 4)
                return Profile.N_TRANSFORM_4;
            if (Size == 8)
                return Profile.N_TRANSFORM_8;
            return 4;

        case DetectedLoopType::Multiply:
            if (Size == 4 && FP)
                return Profile.N_MULTIPLY_4F;
            if (Size == 4)
                return Profile.N_MULTIPLY_4I;
            if (Size == 8 && FP)
                return Profile.N_MULTIPLY_8F;
            if (Size == 8)
                return Profile.N_MULTIPLY_8I;
            return 4;

        case DetectedLoopType::Divide:
            if (Size == 4)
                return Profile.N_DIVIDE_4F;
            if (Size == 8)
                return Profile.N_DIVIDE_8F;
            return 2;

        case DetectedLoopType::Sqrt:
            if (Size == 4)
                return Profile.N_SQRT_4F;
            if (Size == 8)
                return Profile.N_SQRT_8F;
            return 2;

        case DetectedLoopType::MinMax:
            if (Size == 1)
                return Profile.N_MINMAX_1;
            if (Size == 2)
                return Profile.N_MINMAX_2;
            if (Size == 4 && FP)
                return Profile.N_MINMAX_4F;
            if (Size == 4)
                return Profile.N_MINMAX_4I;
            if (Size == 8 && FP)
                return Profile.N_MINMAX_8F;
            if (Size == 8)
                return Profile.N_MINMAX_8I;
            return 4;

        case DetectedLoopType::Bitwise:
            if (Size == 1)
                return Profile.N_BITWISE_1;
            if (Size == 2)
                return Profile.N_BITWISE_2;
            if (Size == 4)
                return Profile.N_BITWISE_4;
            if (Size == 8)
                return Profile.N_BITWISE_8;
            return 3;

        case DetectedLoopType::Shift:
            if (Size == 1)
                return Profile.N_SHIFT_1;
            if (Size == 2)
                return Profile.N_SHIFT_2;
            if (Size == 4)
                return Profile.N_SHIFT_4;
            if (Size == 8)
                return Profile.N_SHIFT_8;
            return 2;

        case DetectedLoopType::Unknown:
        default:
            return 4;
        }
    }

    std::string ILPLoopCheck::getLoopTypeName(DetectedLoopType Type) {
        switch (Type) {
        case DetectedLoopType::Sum:
            return "Sum";
        case DetectedLoopType::DotProduct:
            return "DotProduct";
        case DetectedLoopType::Search:
            return "Search";
        case DetectedLoopType::Copy:
            return "Copy";
        case DetectedLoopType::Transform:
            return "Transform";
        case DetectedLoopType::Multiply:
            return "Multiply";
        case DetectedLoopType::Divide:
            return "Divide";
        case DetectedLoopType::Sqrt:
            return "Sqrt";
        case DetectedLoopType::MinMax:
            return "MinMax";
        case DetectedLoopType::Bitwise:
            return "Bitwise";
        case DetectedLoopType::Shift:
            return "Shift";
        case DetectedLoopType::Unknown:
            return "Unknown";
        }
        return "Unknown";
    }

    std::optional<MacroArgs> ILPLoopCheck::extractMacroArgs(const CallExpr* LoopCall, const SourceManager& SM,
                                                            const LangOptions& LO) {
        SourceLocation Loc = LoopCall->getBeginLoc();

        // Check if this is from a macro expansion
        if (!Loc.isMacroID())
            return std::nullopt;

        // Get the immediate macro caller location (where the macro was invoked)
        SourceLocation MacroLoc = SM.getImmediateMacroCallerLoc(Loc);
        if (!MacroLoc.isValid())
            return std::nullopt;

        // Get the spelling location to read the actual source text
        SourceLocation SpellingLoc = SM.getSpellingLoc(MacroLoc);
        if (!SpellingLoc.isValid())
            return std::nullopt;

        // Get the file ID and offset
        FileID FID = SM.getFileID(SpellingLoc);
        if (FID.isInvalid())
            return std::nullopt;

        // Get the buffer content
        bool Invalid = false;
        StringRef Buffer = SM.getBufferData(FID, &Invalid);
        if (Invalid)
            return std::nullopt;

        unsigned Offset = SM.getFileOffset(SpellingLoc);
        StringRef TextFromLoc = Buffer.substr(Offset);

        // Find the macro name - should start with ILP_FOR
        if (!TextFromLoc.starts_with("ILP_FOR"))
            return std::nullopt;

        MacroArgs Args;

        // Determine macro variant
        if (TextFromLoc.starts_with("ILP_FOR_RANGE_AUTO")) {
            Args.macroName = "ILP_FOR_RANGE_AUTO";
        } else if (TextFromLoc.starts_with("ILP_FOR_RANGE")) {
            Args.macroName = "ILP_FOR_RANGE";
        } else if (TextFromLoc.starts_with("ILP_FOR_AUTO")) {
            Args.macroName = "ILP_FOR_AUTO";
        } else if (TextFromLoc.starts_with("ILP_FOR")) {
            Args.macroName = "ILP_FOR";
        } else {
            return std::nullopt;
        }

        // Find opening paren
        size_t OpenParen = TextFromLoc.find('(');
        if (OpenParen == StringRef::npos)
            return std::nullopt;

        // Find matching closing paren, handling nested parens
        size_t Pos = OpenParen + 1;
        int ParenDepth = 1;
        while (Pos < TextFromLoc.size() && ParenDepth > 0) {
            if (TextFromLoc[Pos] == '(')
                ParenDepth++;
            else if (TextFromLoc[Pos] == ')')
                ParenDepth--;
            Pos++;
        }

        if (ParenDepth != 0)
            return std::nullopt;

        size_t CloseParen = Pos - 1;

        // Extract the arguments string
        StringRef ArgsStr = TextFromLoc.substr(OpenParen + 1, CloseParen - OpenParen - 1);

        // Parse arguments (comma-separated, respecting nested parens)
        std::vector<std::string> ParsedArgs;
        std::string CurrentArg;
        ParenDepth = 0;

        for (char C : ArgsStr) {
            if (C == '(') {
                ParenDepth++;
                CurrentArg += C;
            } else if (C == ')') {
                ParenDepth--;
                CurrentArg += C;
            } else if (C == ',' && ParenDepth == 0) {
                // Trim whitespace
                StringRef Trimmed(CurrentArg);
                Trimmed = Trimmed.trim();
                ParsedArgs.push_back(Trimmed.str());
                CurrentArg.clear();
            } else {
                CurrentArg += C;
            }
        }
        // Don't forget the last argument
        StringRef Trimmed(CurrentArg);
        Trimmed = Trimmed.trim();
        ParsedArgs.push_back(Trimmed.str());

        // ILP_FOR has 4 args: varDecl, start, end, N
        // ILP_FOR_AUTO has 4 args: varDecl, start, end, LoopType
        // ILP_FOR_RANGE has 3 args: varDecl, range, N
        // ILP_FOR_RANGE_AUTO has 3 args: varDecl, range, LoopType
        if (Args.macroName == "ILP_FOR" || Args.macroName == "ILP_FOR_AUTO") {
            if (ParsedArgs.size() != 4)
                return std::nullopt;
            Args.varDecl = ParsedArgs[0];
            Args.start = ParsedArgs[1];
            Args.end = ParsedArgs[2];
            Args.lastArg = ParsedArgs[3];
        } else {
            // RANGE variants
            if (ParsedArgs.size() != 3)
                return std::nullopt;
            Args.varDecl = ParsedArgs[0];
            Args.start = ParsedArgs[1]; // This is actually the range
            Args.end = "";              // Not used for RANGE variants
            Args.lastArg = ParsedArgs[2];
        }

        // Calculate the source range for the macro invocation (just the macro call, not the body)
        SourceLocation MacroEnd = SpellingLoc.getLocWithOffset(CloseParen + 1);
        Args.macroRange = CharSourceRange::getCharRange(SpellingLoc, MacroEnd);

        return Args;
    }

    std::string ILPLoopCheck::buildPortableFix(const MacroArgs& Args, DetectedLoopType Type) {
        std::string LoopTypeName = getLoopTypeName(Type);

        if (Args.macroName == "ILP_FOR") {
            return "ILP_FOR_AUTO(" + Args.varDecl + ", " + Args.start + ", " + Args.end + ", " + LoopTypeName + ")";
        } else if (Args.macroName == "ILP_FOR_RANGE") {
            return "ILP_FOR_RANGE_AUTO(" + Args.varDecl + ", " + Args.start + ", " + LoopTypeName + ")";
        }
        // Already using AUTO variant, return unchanged
        return "";
    }

} // namespace clang::tidy::ilp

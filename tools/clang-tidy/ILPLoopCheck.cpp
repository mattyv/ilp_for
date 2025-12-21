// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#include "ILPLoopCheck.h"
#include "ilp_for/cpu_profiles/ilp_cpu_profiles.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include <vector>

using namespace clang::ast_matchers;

namespace clang::tidy::ilp {

    namespace {
        // Helper to check if a type is double (compatible across Clang versions)
        bool isDoubleType(const Type* Ty) {
            if (const auto* BT = Ty->getAs<BuiltinType>()) {
                return BT->getKind() == BuiltinType::Double;
            }
            return false;
        }

        // Don't blow the stack on deeply nested code
        constexpr unsigned MaxAnalysisDepth = 64;

        // Returns true if expression represents indexed memory access
        // Handles: arr[i], i[arr], *(arr + i), *(i + arr)
        bool isIndexedAccess(const Expr* E) {
            if (!E)
                return false;
            E = E->IgnoreParenImpCasts();

            // Direct array subscript: arr[i] or i[arr]
            if (isa<ArraySubscriptExpr>(E))
                return true;

            // Pointer dereference with addition: *(arr + i) or *(i + arr)
            if (const auto* UO = dyn_cast<UnaryOperator>(E)) {
                if (UO->getOpcode() == UO_Deref) {
                    const Expr* Sub = UO->getSubExpr()->IgnoreParenImpCasts();
                    if (const auto* BO = dyn_cast<BinaryOperator>(Sub)) {
                        if (BO->getOpcode() == BO_Add)
                            return true;
                    }
                }
            }

            return false;
        }
    } // namespace

    void LoopAnalysis::computeLoopType() {
        // DEPRECATED: Don't use this - it picks patterns by priority which is wrong.
        // Use computeOptimalN() instead - it finds the pattern needing the highest N.
        // Keeping this around but it's not used anymore.

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

    std::pair<DetectedLoopType, int> ILPLoopCheck::computeOptimalN(const LoopAnalysis& Analysis) {
        // Find the pattern requiring the highest N value (the bottleneck).
        // Per Agner Fog: "You may have multiple carried dependency chains in a loop:
        // the speed limit is set by the longest."
        //
        // N = Latency × Throughput-per-cycle
        // Higher N means more parallel chains needed to saturate execution units.

        int maxN = 0;
        DetectedLoopType dominantType = DetectedLoopType::Unknown;

        auto updateMax = [&](DetectedLoopType type) {
            int n = lookupOptimalN(type, Analysis);
            if (n > maxN) {
                maxN = n;
                dominantType = type;
            }
        };

        // Check each detected pattern and find the one needing highest N
        if (Analysis.hasEarlyExit)
            updateMax(DetectedLoopType::Search);
        if (Analysis.hasSqrt)
            updateMax(DetectedLoopType::Sqrt);
        if (Analysis.hasDivision)
            updateMax(DetectedLoopType::Divide);
        if (Analysis.hasMulInAdd)
            updateMax(DetectedLoopType::DotProduct);
        if (Analysis.hasCompoundMul)
            updateMax(DetectedLoopType::Multiply);
        if (Analysis.hasMinMax)
            updateMax(DetectedLoopType::MinMax);
        if (Analysis.hasBitwise)
            updateMax(DetectedLoopType::Bitwise);
        if (Analysis.hasShift)
            updateMax(DetectedLoopType::Shift);
        if (Analysis.hasTransform)
            updateMax(DetectedLoopType::Transform);
        if (Analysis.hasCopy)
            updateMax(DetectedLoopType::Copy);
        if (Analysis.hasCompoundAdd)
            updateMax(DetectedLoopType::Sum);

        return {dominantType, maxN};
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

        // Find the pattern requiring the highest N (the bottleneck)
        auto [DominantType, OptimalN] = computeOptimalN(Analysis);

        // Skip if we couldn't determine the type
        if (DominantType == DetectedLoopType::Unknown)
            return;

        // Get source locations
        SourceLocation Loc = LoopCall->getBeginLoc();
        const SourceManager& SM = *Result.SourceManager;

        // Build diagnostic message
        std::string LoopTypeName = getLoopTypeName(DominantType);

        // Try to extract macro arguments for auto-fix
        auto MaybeArgs = extractMacroArgs(LoopCall, SM, getLangOpts());

        // Check if already using ILP_FOR_AUTO - skip if so
        if (MaybeArgs && (MaybeArgs->macroName == "ILP_FOR_AUTO" || MaybeArgs->macroName == "ILP_FOR_RANGE_AUTO")) {
            return; // Already using auto variant, nothing to fix
        }

        // Emit diagnostic with fix if we can generate one
        if (MaybeArgs) {
            std::string FixText = buildPortableFix(*MaybeArgs, DominantType);
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

        // Note: We don't call computeLoopType() here anymore.
        // The caller should use computeOptimalN() to find the pattern
        // requiring the highest N (the bottleneck).

        return Analysis;
    }

    void ILPLoopCheck::analyzeStatement(const Stmt* S, LoopAnalysis& Analysis, ASTContext& Context, unsigned Depth) {
        if (!S || Depth > MaxAnalysisDepth)
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

        // Check for copy/transform pattern: assignment to indexed element
        // Handles: arr[i], i[arr], *(arr + i), *(i + arr)
        if (const auto* BO = dyn_cast<BinaryOperator>(S)) {
            if (BO->isAssignmentOp() && !BO->isCompoundAssignmentOp()) {
                const Expr* LHS = BO->getLHS();
                const Expr* RHS = BO->getRHS()->IgnoreParenImpCasts();

                // Check if LHS is an indexed access (dst[i] = ..., *(dst + i) = ...)
                if (isIndexedAccess(LHS)) {
                    // Check if RHS is a function call - that's Transform
                    if (isa<CallExpr>(RHS)) {
                        Analysis.hasTransform = true;
                    }
                    // Check if RHS is also indexed access - that's Copy
                    else if (isIndexedAccess(RHS)) {
                        Analysis.hasCopy = true;
                    }
                }
            }
        }

        // Recurse into child statements with depth limit
        for (const Stmt* Child : S->children()) {
            if (Child)
                analyzeStatement(Child, Analysis, Context, Depth + 1);
        }
    }

    void ILPLoopCheck::analyzeBinaryOperator(const BinaryOperator* BO, LoopAnalysis& Analysis) {
        BinaryOperator::Opcode Op = BO->getOpcode();

        // Division
        if (Op == BO_Div) {
            Analysis.hasDivision = true;
            QualType Ty = BO->getType();
            Analysis.accumulatorType = Ty;

            // Inside generic lambdas, types are dependent - can't know what they are
            if (Ty->isDependentType()) {
                // Assume FP since higher N is safer
                Analysis.typeSize = 4;
                Analysis.isFloatingPoint = true;
            } else {
                Analysis.typeSize =
                    Ty->isFloatingType() ? (Ty->isFloat128Type() ? 16 : (isDoubleType(Ty.getTypePtr()) ? 8 : 4)) : 4;
                Analysis.isFloatingPoint = Ty->isFloatingType();
            }
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

        // Inside generic lambdas types are dependent - just guess FP (higher N is safer)
        if (Ty->isDependentType()) {
            Analysis.typeSize = 4;
            Analysis.isFloatingPoint = true;
        } else if (Ty->isBuiltinType()) {
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
            Analysis.isFloatingPoint = Ty->isFloatingType();
        } else {
            Analysis.typeSize = 4;
            Analysis.isFloatingPoint = Ty->isFloatingType();
        }

        switch (Op) {
        case BO_AddAssign:
        case BO_SubAssign:
            Analysis.hasCompoundAdd = true;
            // Check for FMA pattern: acc += a * b
            // Only mark as FMA/DotProduct if BOTH operands are indexed expressions
            // This distinguishes dot product (a[i] * b[i]) from scaled sum (data[i] * 2.0)
            if (const auto* RHS = dyn_cast<BinaryOperator>(CAO->getRHS()->IgnoreParenImpCasts())) {
                if (RHS->getOpcode() == BO_Mul) {
                    bool lhsIndexed = isIndexedAccess(RHS->getLHS());
                    bool rhsIndexed = isIndexedAccess(RHS->getRHS());
                    if (lhsIndexed && rhsIndexed) {
                        Analysis.hasMulInAdd = true;
                    }
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
            // For unresolved calls (e.g., templates in generic lambdas), try to get name from callee expr
            const Expr* Callee = CE->getCallee()->IgnoreParenImpCasts();

            if (const auto* ULE = dyn_cast<UnresolvedLookupExpr>(Callee)) {
                // Handle dependent name lookup in templates (e.g., std::sqrt in generic lambda)
                Name = ULE->getName().getAsString();

                // Extract qualified name from nested name specifier if present
                if (NestedNameSpecifier* NNS = ULE->getQualifier()) {
                    llvm::raw_string_ostream OS(QualifiedName);
                    NNS->print(OS, PrintingPolicy(getLangOpts()));
                    QualifiedName += Name;
                }
            } else if (const auto* DRE = dyn_cast<DeclRefExpr>(Callee)) {
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
                Analysis.typeSize = isDoubleType(RetTy.getTypePtr()) ? 8 : 4;
            } else {
                Analysis.typeSize = 4; // Default to float
            }
            Analysis.isFloatingPoint = true;
        }

        // Check for min/max (handles std::min, std::max, fmin, fmax, etc.)
        bool isMinMax = Name == "min" || Name == "max" || Name == "fmin" || Name == "fmax" || Name == "fminf" ||
                        Name == "fmaxf" || QualifiedName.find("::min") != std::string::npos ||
                        QualifiedName.find("::max") != std::string::npos;
        if (isMinMax) {
            Analysis.hasMinMax = true;
            if (CE->getNumArgs() > 0) {
                QualType ArgTy = CE->getArg(0)->getType();
                // Handle dependent types (e.g., auto in generic lambdas)
                if (!ArgTy->isDependentType()) {
                    Analysis.accumulatorType = ArgTy;
                    Analysis.isFloatingPoint = ArgTy->isFloatingType();
                    Analysis.typeSize = isDoubleType(ArgTy.getTypePtr()) ? 8 : 4;
                } else {
                    // Can't know the type in a generic lambda, so guess based on name:
                    // fmin/fmax are obviously FP, and std::min/max are likely FP in numeric code.
                    // Assuming FP is safer anyway (higher N won't hurt, lower N might).
                    bool isFPVariant = (Name == "fmin" || Name == "fmax" || Name == "fminf" || Name == "fmaxf");
                    Analysis.typeSize = 4;
                    Analysis.isFloatingPoint = isFPVariant || Name == "min" || Name == "max";
                }
            }
        }
    }

    int ILPLoopCheck::lookupOptimalN(DetectedLoopType Type, const LoopAnalysis& Analysis) {
        const auto& P = ::ilp::cpu::get(TargetCPU);
        unsigned Size = Analysis.typeSize;
        bool FP = Analysis.isFloatingPoint;

        switch (Type) {
        case DetectedLoopType::Sum:
            if (Size == 1)
                return P.sum_1;
            if (Size == 2)
                return P.sum_2;
            if (Size == 4 && FP)
                return P.sum_4f;
            if (Size == 4)
                return P.sum_4i;
            if (Size == 8 && FP)
                return P.sum_8f;
            if (Size == 8)
                return P.sum_8i;
            return 4;

        case DetectedLoopType::DotProduct:
            if (Size == 4)
                return P.dotproduct_4;
            if (Size == 8)
                return P.dotproduct_8;
            return 8;

        case DetectedLoopType::Search:
            if (Size == 1)
                return P.search_1;
            if (Size == 2)
                return P.search_2;
            if (Size == 4)
                return P.search_4;
            if (Size == 8)
                return P.search_8;
            return 4;

        case DetectedLoopType::Copy:
            if (Size == 1)
                return P.copy_1;
            if (Size == 2)
                return P.copy_2;
            if (Size == 4)
                return P.copy_4;
            if (Size == 8)
                return P.copy_8;
            return 4;

        case DetectedLoopType::Transform:
            if (Size == 1)
                return P.transform_1;
            if (Size == 2)
                return P.transform_2;
            if (Size == 4)
                return P.transform_4;
            if (Size == 8)
                return P.transform_8;
            return 4;

        case DetectedLoopType::Multiply:
            if (Size == 4 && FP)
                return P.multiply_4f;
            if (Size == 4)
                return P.multiply_4i;
            if (Size == 8 && FP)
                return P.multiply_8f;
            if (Size == 8)
                return P.multiply_8i;
            return 4;

        case DetectedLoopType::Divide:
            if (Size == 4)
                return P.divide_4f;
            if (Size == 8)
                return P.divide_8f;
            return 2;

        case DetectedLoopType::Sqrt:
            if (Size == 4)
                return P.sqrt_4f;
            if (Size == 8)
                return P.sqrt_8f;
            return 2;

        case DetectedLoopType::MinMax:
            if (Size == 1)
                return P.minmax_1;
            if (Size == 2)
                return P.minmax_2;
            if (Size == 4 && FP)
                return P.minmax_4f;
            if (Size == 4)
                return P.minmax_4i;
            if (Size == 8 && FP)
                return P.minmax_8f;
            if (Size == 8)
                return P.minmax_8i;
            return 4;

        case DetectedLoopType::Bitwise:
            if (Size == 1)
                return P.bitwise_1;
            if (Size == 2)
                return P.bitwise_2;
            if (Size == 4)
                return P.bitwise_4;
            if (Size == 8)
                return P.bitwise_8;
            return 3;

        case DetectedLoopType::Shift:
            if (Size == 1)
                return P.shift_1;
            if (Size == 2)
                return P.shift_2;
            if (Size == 4)
                return P.shift_4;
            if (Size == 8)
                return P.shift_8;
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

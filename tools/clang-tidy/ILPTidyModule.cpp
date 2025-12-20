// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#include "ILPLoopCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

namespace clang::tidy::ilp {

    class ILPModule : public ClangTidyModule {
      public:
        void addCheckFactories(ClangTidyCheckFactories& CheckFactories) override {
            CheckFactories.registerCheck<ILPLoopCheck>("ilp-loop-analysis");
        }
    };

    // Register the module
    static ClangTidyModuleRegistry::Add<ILPModule> X("ilp-module", "Adds ILP loop analysis checks.");

} // namespace clang::tidy::ilp

// Anchor for linking
volatile int ILPModuleAnchorSource = 0;

//===--------------------------------------------------------------------===////
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2023 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_REFACTORING_REFACTORINGACTIONS_H
#define SWIFT_REFACTORING_REFACTORINGACTIONS_H

#include "swift/AST/ASTContext.h"
#include "swift/AST/SourceFile.h"
#include "swift/Basic/SourceManager.h"
#include "swift/IDE/IDERequests.h"
#include "swift/Parse/Lexer.h"
#include "swift/Refactoring/Refactoring.h"

namespace swift {
namespace refactoring {

using namespace swift;
using namespace swift::ide;

namespace {

/// Get the source file that corresponds to the given buffer.
SourceFile *getContainingFile(ModuleDecl *M, RangeConfig Range) {
  auto &SM = M->getASTContext().SourceMgr;
  // TODO: We should add an ID -> SourceFile mapping.
  return M->getSourceFileContainingLocation(
      SM.getRangeForBuffer(Range.BufferID).getStart());
}
} // namespace

class RefactoringAction {
protected:
  ModuleDecl *MD;
  SourceFile *TheFile;
  SourceEditConsumer &EditConsumer;
  ASTContext &Ctx;
  SourceManager &SM;
  DiagnosticEngine DiagEngine;
  SourceLoc StartLoc;
  StringRef PreferredName;

public:
  RefactoringAction(ModuleDecl *MD, RefactoringOptions &Opts,
                    SourceEditConsumer &EditConsumer,
                    DiagnosticConsumer &DiagConsumer)
      : MD(MD), TheFile(getContainingFile(MD, Opts.Range)),
        EditConsumer(EditConsumer), Ctx(MD->getASTContext()),
        SM(MD->getASTContext().SourceMgr), DiagEngine(SM),
        StartLoc(Lexer::getLocForStartOfToken(SM, Opts.Range.getStart(SM))),
        PreferredName(Opts.PreferredName) {
    DiagEngine.addConsumer(DiagConsumer);
  }
  virtual ~RefactoringAction() = default;
  virtual bool performChange() = 0;
};

/// Different from RangeBasedRefactoringAction, TokenBasedRefactoringAction
/// takes the input of a given token, e.g., a name or an "if" key word.
/// Contextual refactoring kinds can suggest applicable refactorings on that
/// token, e.g. rename or reverse if statement.
class TokenBasedRefactoringAction : public RefactoringAction {
protected:
  ResolvedCursorInfoPtr CursorInfo;

public:
  TokenBasedRefactoringAction(ModuleDecl *MD, RefactoringOptions &Opts,
                              SourceEditConsumer &EditConsumer,
                              DiagnosticConsumer &DiagConsumer)
      : RefactoringAction(MD, Opts, EditConsumer, DiagConsumer) {
    // Resolve the sema token and save it for later use.
    CursorInfo =
        evaluateOrDefault(TheFile->getASTContext().evaluator,
                          CursorInfoRequest{CursorInfoOwner(TheFile, StartLoc)},
                          new ResolvedCursorInfo());
  }
};

#define CURSOR_REFACTORING(KIND, NAME, ID)                                     \
  class RefactoringAction##KIND : public TokenBasedRefactoringAction {         \
  public:                                                                      \
    RefactoringAction##KIND(ModuleDecl *MD, RefactoringOptions &Opts,          \
                            SourceEditConsumer &EditConsumer,                  \
                            DiagnosticConsumer &DiagConsumer)                  \
        : TokenBasedRefactoringAction(MD, Opts, EditConsumer, DiagConsumer) {} \
    bool performChange() override;                                             \
    static bool isApplicable(ResolvedCursorInfoPtr Info,                       \
                             DiagnosticEngine &Diag);                          \
    bool isApplicable() {                                                      \
      return RefactoringAction##KIND::isApplicable(CursorInfo, DiagEngine);    \
    }                                                                          \
  };
#include "swift/Refactoring/RefactoringKinds.def"

class RangeBasedRefactoringAction : public RefactoringAction {
protected:
  ResolvedRangeInfo RangeInfo;

public:
  RangeBasedRefactoringAction(ModuleDecl *MD, RefactoringOptions &Opts,
                              SourceEditConsumer &EditConsumer,
                              DiagnosticConsumer &DiagConsumer)
      : RefactoringAction(MD, Opts, EditConsumer, DiagConsumer),
        RangeInfo(evaluateOrDefault(
            MD->getASTContext().evaluator,
            RangeInfoRequest(RangeInfoOwner(TheFile, Opts.Range.getStart(SM),
                                            Opts.Range.getEnd(SM))),
            ResolvedRangeInfo())) {}
};

#define RANGE_REFACTORING(KIND, NAME, ID)                                      \
  class RefactoringAction##KIND : public RangeBasedRefactoringAction {         \
  public:                                                                      \
    RefactoringAction##KIND(ModuleDecl *MD, RefactoringOptions &Opts,          \
                            SourceEditConsumer &EditConsumer,                  \
                            DiagnosticConsumer &DiagConsumer)                  \
        : RangeBasedRefactoringAction(MD, Opts, EditConsumer, DiagConsumer) {} \
    bool performChange() override;                                             \
    static bool isApplicable(const ResolvedRangeInfo &Info,                    \
                             DiagnosticEngine &Diag);                          \
    bool isApplicable() {                                                      \
      return RefactoringAction##KIND::isApplicable(RangeInfo, DiagEngine);     \
    }                                                                          \
  };
#include "swift/Refactoring/RefactoringKinds.def"

} // end namespace refactoring
} // end namespace swift

#endif

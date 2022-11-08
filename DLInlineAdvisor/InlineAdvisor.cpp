//===- InlineAdvisor.cpp - analysis pass implementation -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements InlineAdvisorAnalysis and DefaultInlineAdvisor, and
// related types.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ReplayInlineAdvisor.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/Utils/ImportedFunctionsInliningStatistics.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"

#include <dlfcn.h>

using namespace llvm;
#define DEBUG_TYPE "inline"
#ifdef LLVM_HAVE_TF_AOT_INLINERSIZEMODEL
#define LLVM_HAVE_TF_AOT
#endif

// This weirdly named statistic tracks the number of times that, when attempting
// to inline a function A into B, we analyze the callers of B in order to see
// if those would be more profitable and blocked inline steps.
STATISTIC(NumCallerCallersAnalyzed, "Number of caller-callers analyzed");

/// Flag to path of dynamic libarary
struct DLInlineAdvisorInfo {
  typedef std::unique_ptr<InlineAdvisor> (*DLInlineAdivsorFactory_t)(
      Module &M, FunctionAnalysisManager &FAM, LLVMContext &Context,
      std::unique_ptr<InlineAdvisor> OriginalAdvisor, InlineContext IC);

  std::string Path;

  void *Handle;
  DLInlineAdivsorFactory_t Factory;

  DLInlineAdvisorInfo() : Path{}, Handle(nullptr), Factory(nullptr) {}
  DLInlineAdvisorInfo(const std::string &Path) : Path(Path) {
    Handle = dlopen(Path.c_str(), RTLD_LAZY);
    if (!Handle) {
      errs() << "Cannot open library: " << dlerror() << '\n';
    } else {
      Factory =
          (DLInlineAdivsorFactory_t)dlsym(Handle, "DLInlineAdvisorFactory");
    }
  }
  ~DLInlineAdvisorInfo() {
    if (Handle)
      dlclose(Handle);
  }
};

static cl::opt<DLInlineAdvisorInfo, false, cl::parser<std::string>>
    DLInlineAdivisor("dl-inline-advisor-path",
                     cl::desc("Path to a dynamic library that can be used "
                              "instead of the default inline advisor."),
                     cl::value_desc("dynamic library path."));

static cl::opt<std::string> DLInlineAdivisorCTX(
    "dl-inline-advisor-ctx", cl::init(""),
    cl::desc(
        "String passed to dynamic inline advisor to modify it's behaviour."),
    cl::value_desc("dynamic library context."));

namespace {
using namespace llvm::ore;
class MandatoryInlineAdvice : public InlineAdvice {
public:
  MandatoryInlineAdvice(InlineAdvisor *Advisor, CallBase &CB,
                        OptimizationRemarkEmitter &ORE,
                        bool IsInliningMandatory)
      : InlineAdvice(Advisor, CB, ORE, IsInliningMandatory) {}

private:
  void recordInliningWithCalleeDeletedImpl() override { recordInliningImpl(); }

  void recordInliningImpl() override {
    if (IsInliningRecommended)
      emitInlinedInto(ORE, DLoc, Block, *Callee, *Caller, IsInliningRecommended,
                      [&](OptimizationRemark &Remark) {
                        Remark << ": always inline attribute";
                      });
  }

  void recordUnsuccessfulInliningImpl(const InlineResult &Result) override {
    if (IsInliningRecommended)
      ORE.emit([&]() {
        return OptimizationRemarkMissed(Advisor->getAnnotatedInlinePassName(),
                                        "NotInlined", DLoc, Block)
               << "'" << NV("Callee", Callee) << "' is not AlwaysInline into '"
               << NV("Caller", Caller)
               << "': " << NV("Reason", Result.getFailureReason());
      });
  }

  void recordUnattemptedInliningImpl() override {
    assert(!IsInliningRecommended && "Expected to attempt inlining");
  }
};
} // namespace

void DefaultInlineAdvice::recordUnsuccessfulInliningImpl(
    const InlineResult &Result) {
  using namespace ore;
  llvm::setInlineRemark(*OriginalCB, std::string(Result.getFailureReason()) +
                                         "; " + inlineCostStr(*OIC));
  ORE.emit([&]() {
    return OptimizationRemarkMissed(Advisor->getAnnotatedInlinePassName(),
                                    "NotInlined", DLoc, Block)
           << "'" << NV("Callee", Callee) << "' is not inlined into '"
           << NV("Caller", Caller)
           << "': " << NV("Reason", Result.getFailureReason());
  });
}

void DefaultInlineAdvice::recordInliningWithCalleeDeletedImpl() {
  if (EmitRemarks)
    emitInlinedIntoBasedOnCost(ORE, DLoc, Block, *Callee, *Caller, *OIC,
                               /* ForProfileContext= */ false,
                               Advisor->getAnnotatedInlinePassName());
}

void DefaultInlineAdvice::recordInliningImpl() {
  if (EmitRemarks)
    emitInlinedIntoBasedOnCost(ORE, DLoc, Block, *Callee, *Caller, *OIC,
                               /* ForProfileContext= */ false,
                               Advisor->getAnnotatedInlinePassName());
}



InlineAdvice::InlineAdvice(InlineAdvisor *Advisor, CallBase &CB,
                           OptimizationRemarkEmitter &ORE,
                           bool IsInliningRecommended)
    : Advisor(Advisor), Caller(CB.getCaller()), Callee(CB.getCalledFunction()),
      DLoc(CB.getDebugLoc()), Block(CB.getParent()), ORE(ORE),
      IsInliningRecommended(IsInliningRecommended) {}

void InlineAdvice::recordInlineStatsIfNeeded() {
  if (Advisor->ImportedFunctionsStats)
    Advisor->ImportedFunctionsStats->recordInline(*Caller, *Callee);
}

void InlineAdvice::recordInlining() {
  markRecorded();
  recordInlineStatsIfNeeded();
  recordInliningImpl();
}

void InlineAdvice::recordInliningWithCalleeDeleted() {
  markRecorded();
  recordInlineStatsIfNeeded();
  recordInliningWithCalleeDeletedImpl();
}

AnalysisKey InlineAdvisorAnalysis::Key;


namespace llvm {
static raw_ostream &operator<<(raw_ostream &R, const ore::NV &Arg) {
  return R << Arg.Val;
}

template <class RemarkT>
RemarkT &operator<<(RemarkT &&R, const InlineCost &IC) {
  using namespace ore;
  if (IC.isAlways()) {
    R << "(cost=always)";
  } else if (IC.isNever()) {
    R << "(cost=never)";
  } else {
    R << "(cost=" << ore::NV("Cost", IC.getCost())
      << ", threshold=" << ore::NV("Threshold", IC.getThreshold()) << ")";
  }
  if (const char *Reason = IC.getReason())
    R << ": " << ore::NV("Reason", Reason);
  return R;
}
} // namespace llvm

std::string llvm::inlineCostStr(const InlineCost &IC) {
  std::string Buffer;
  raw_string_ostream Remark(Buffer);
  Remark << IC;
  return Remark.str();
}

std::string llvm::formatCallSiteLocation(DebugLoc DLoc,
                                         const CallSiteFormat &Format) {
  std::string Buffer;
  raw_string_ostream CallSiteLoc(Buffer);
  bool First = true;
  for (DILocation *DIL = DLoc.get(); DIL; DIL = DIL->getInlinedAt()) {
    if (!First)
      CallSiteLoc << " @ ";
    // Note that negative line offset is actually possible, but we use
    // unsigned int to match line offset representation in remarks so
    // it's directly consumable by relay advisor.
    uint32_t Offset =
        DIL->getLine() - DIL->getScope()->getSubprogram()->getLine();
    uint32_t Discriminator = DIL->getBaseDiscriminator();
    StringRef Name = DIL->getScope()->getSubprogram()->getLinkageName();
    if (Name.empty())
      Name = DIL->getScope()->getSubprogram()->getName();
    CallSiteLoc << Name.str() << ":" << llvm::utostr(Offset);
    if (Format.outputColumn())
      CallSiteLoc << ":" << llvm::utostr(DIL->getColumn());
    if (Format.outputDiscriminator() && Discriminator)
      CallSiteLoc << "." << llvm::utostr(Discriminator);
    First = false;
  }

  return CallSiteLoc.str();
}

void llvm::addLocationToRemarks(OptimizationRemark &Remark, DebugLoc DLoc) {
  if (!DLoc) {
    return;
  }

  bool First = true;
  Remark << " at callsite ";
  for (DILocation *DIL = DLoc.get(); DIL; DIL = DIL->getInlinedAt()) {
    if (!First)
      Remark << " @ ";
    unsigned int Offset = DIL->getLine();
    Offset -= DIL->getScope()->getSubprogram()->getLine();
    unsigned int Discriminator = DIL->getBaseDiscriminator();
    StringRef Name = DIL->getScope()->getSubprogram()->getLinkageName();
    if (Name.empty())
      Name = DIL->getScope()->getSubprogram()->getName();
    Remark << Name << ":" << ore::NV("Line", Offset) << ":"
           << ore::NV("Column", DIL->getColumn());
    if (Discriminator)
      Remark << "." << ore::NV("Disc", Discriminator);
    First = false;
  }

  Remark << ";";
}

void llvm::emitInlinedInto(
    OptimizationRemarkEmitter &ORE, DebugLoc DLoc, const BasicBlock *Block,
    const Function &Callee, const Function &Caller, bool AlwaysInline,
    function_ref<void(OptimizationRemark &)> ExtraContext,
    const char *PassName) {
  ORE.emit([&]() {
    StringRef RemarkName = AlwaysInline ? "AlwaysInline" : "Inlined";
    OptimizationRemark Remark(PassName ? PassName : DEBUG_TYPE, RemarkName,
                              DLoc, Block);
    Remark << "'" << ore::NV("Callee", &Callee) << "' inlined into '"
           << ore::NV("Caller", &Caller) << "'";
    if (ExtraContext)
      ExtraContext(Remark);
    addLocationToRemarks(Remark, DLoc);
    return Remark;
  });
}

void llvm::emitInlinedIntoBasedOnCost(
    OptimizationRemarkEmitter &ORE, DebugLoc DLoc, const BasicBlock *Block,
    const Function &Callee, const Function &Caller, const InlineCost &IC,
    bool ForProfileContext, const char *PassName) {
  llvm::emitInlinedInto(
      ORE, DLoc, Block, Callee, Caller, IC.isAlways(),
      [&](OptimizationRemark &Remark) {
        if (ForProfileContext)
          Remark << " to match profiling context";
        Remark << " with " << IC;
      },
      PassName);
}

InlineAdvisor::InlineAdvisor(Module &M, FunctionAnalysisManager &FAM,
                             Optional<InlineContext> IC)
    : M(M), FAM(FAM), IC(IC),
      AnnotatedInlinePassName(llvm::AnnotateInlinePassName(*IC)) {}

InlineAdvisor::~InlineAdvisor() {}

std::unique_ptr<InlineAdvice> InlineAdvisor::getMandatoryAdvice(CallBase &CB,
                                                                bool Advice) {
  return std::make_unique<MandatoryInlineAdvice>(this, CB, getCallerORE(CB),
                                                 Advice);
}

static inline const char *getLTOPhase(ThinOrFullLTOPhase LTOPhase) {
  switch (LTOPhase) {
  case (ThinOrFullLTOPhase::None):
    return "main";
  case (ThinOrFullLTOPhase::ThinLTOPreLink):
  case (ThinOrFullLTOPhase::FullLTOPreLink):
    return "prelink";
  case (ThinOrFullLTOPhase::ThinLTOPostLink):
  case (ThinOrFullLTOPhase::FullLTOPostLink):
    return "postlink";
  }
  llvm_unreachable("unreachable");
}

static inline const char *getInlineAdvisorContext(InlinePass IP) {
  switch (IP) {
  case (InlinePass::AlwaysInliner):
    return "always-inline";
  case (InlinePass::CGSCCInliner):
    return "cgscc-inline";
  case (InlinePass::EarlyInliner):
    return "early-inline";
  case (InlinePass::MLInliner):
    return "ml-inline";
  case (InlinePass::ModuleInliner):
    return "module-inline";
  case (InlinePass::ReplayCGSCCInliner):
    return "replay-cgscc-inline";
  case (InlinePass::ReplaySampleProfileInliner):
    return "replay-sample-profile-inline";
  case (InlinePass::SampleProfileInliner):
    return "sample-profile-inline";
  }

  llvm_unreachable("unreachable");
}

std::string llvm::AnnotateInlinePassName(InlineContext IC) {
  return std::string(getLTOPhase(IC.LTOPhase)) + "-" +
         std::string(getInlineAdvisorContext(IC.Pass));
}

InlineAdvisor::MandatoryInliningKind
InlineAdvisor::getMandatoryKind(CallBase &CB, FunctionAnalysisManager &FAM,
                                OptimizationRemarkEmitter &ORE) {
  auto &Callee = *CB.getCalledFunction();

  auto GetTLI = [&](Function &F) -> const TargetLibraryInfo & {
    return FAM.getResult<TargetLibraryAnalysis>(F);
  };

  auto &TIR = FAM.getResult<TargetIRAnalysis>(Callee);

  auto TrivialDecision =
      llvm::getAttributeBasedInliningDecision(CB, &Callee, TIR, GetTLI);

  if (TrivialDecision) {
    if (TrivialDecision->isSuccess())
      return MandatoryInliningKind::Always;
    else
      return MandatoryInliningKind::Never;
  }
  return MandatoryInliningKind::NotMandatory;
}

std::unique_ptr<InlineAdvice> InlineAdvisor::getAdvice(CallBase &CB,
                                                       bool MandatoryOnly) {
  if (!MandatoryOnly)
    return getAdviceImpl(CB);
  bool Advice = CB.getCaller() != CB.getCalledFunction() &&
                MandatoryInliningKind::Always ==
                    getMandatoryKind(CB, FAM, getCallerORE(CB));
  return getMandatoryAdvice(CB, Advice);
}

OptimizationRemarkEmitter &InlineAdvisor::getCallerORE(CallBase &CB) {
  return FAM.getResult<OptimizationRemarkEmitterAnalysis>(*CB.getCaller());
}

PreservedAnalyses
InlineAdvisorAnalysisPrinterPass::run(Module &M, ModuleAnalysisManager &MAM) {
  const auto *IA = MAM.getCachedResult<InlineAdvisorAnalysis>(M);
  if (!IA)
    OS << "No Inline Advisor\n";
  else
    IA->getAdvisor()->print(OS);
  return PreservedAnalyses::all();
}

PreservedAnalyses InlineAdvisorAnalysisPrinterPass::run(
    LazyCallGraph::SCC &InitialC, CGSCCAnalysisManager &AM, LazyCallGraph &CG,
    CGSCCUpdateResult &UR) {
  const auto &MAMProxy =
      AM.getResult<ModuleAnalysisManagerCGSCCProxy>(InitialC, CG);

  if (InitialC.size() == 0) {
    OS << "SCC is empty!\n";
    return PreservedAnalyses::all();
  }
  Module &M = *InitialC.begin()->getFunction().getParent();
  const auto *IA = MAMProxy.getCachedResult<InlineAdvisorAnalysis>(M);
  if (!IA)
    OS << "No Inline Advisor\n";
  else
    IA->getAdvisor()->print(OS);
  return PreservedAnalyses::all();
}

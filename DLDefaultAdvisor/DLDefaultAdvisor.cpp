#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/LazyCallGraph.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ReplayInlineAdvisor.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/Utils/ImportedFunctionsInliningStatistics.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

#include "DLInlineAdvisor.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace llvm;
#define DEBUG_TYPE "inline"

extern "C" std::unique_ptr<InlineAdvisor>
DLInlineAdvisorFactory(Module &M, FunctionAnalysisManager &FAM,
                       LLVMContext &Context,
                       std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                       InlineContext IC, std::string &DLCTX) {
  return std::make_unique<DLInlineAdvisor>(
      M, FAM, Context, std::move(OriginalAdvisor), IC, DLCTX);
}

DLInlineAdvisor::DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM,
                                 LLVMContext &Context,
                                 std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                                 InlineContext IC, std::string &DLCTX)
    : InlineAdvisor(M, FAM, IC), OriginalAdvisor(std::move(OriginalAdvisor)) {
  // this is going to be passed by constructor
  // once I get constructor working properly
  if (DLCTX == "false") {
    adviceMode = AdviceModes::FALSE;
  } else if (DLCTX == "true") {
    adviceMode = AdviceModes::TRUE;
  } else {
    adviceMode = AdviceModes::DEFAULT;
  }

}

void DLInlineAdvisor::onPassEntry(LazyCallGraph::SCC *SCC) {
  std::cout << "Entered Pass on Dynamic Advisor" << std::endl;
}

void DLInlineAdvisor::onPassExit(LazyCallGraph::SCC *SCC) {
  std::cout << "Exited Pass on Dynamic Advisor" << std::endl;
}

std::unique_ptr<InlineAdvice> DLInlineAdvisor::getAdviceImpl(CallBase &CB) {
  auto advice = OriginalAdvisor->getAdvice(CB);

  switch (adviceMode) {
  case AdviceModes::FALSE:
    advice->recordUnattemptedInlining();
    advice =
        std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), false);
    break;
  case AdviceModes::TRUE:
    advice->recordUnattemptedInlining();
    advice =
        std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), true);
    break;
  case AdviceModes::DEFAULT:
    break;
  }

  return advice;
}
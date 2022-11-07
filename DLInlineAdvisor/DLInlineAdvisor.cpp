#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ReplayInlineAdvisor.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/Utils/ImportedFunctionsInliningStatistics.h"
#include "llvm/IR/DebugInfoMetadata.h"
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

extern "C" std::unique_ptr<InlineAdvisor> DLInlineAdvisorFactory(
    Module &M, FunctionAnalysisManager &FAM, LLVMContext &Context,
    std::unique_ptr<InlineAdvisor> OriginalAdvisor, InlineContext IC) {
  return std::make_unique<DLInlineAdvisor>(M, FAM, Context,
                                           std::move(OriginalAdvisor), IC);
}

DLInlineAdvisor::DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM,
                                 LLVMContext &Context,
                                 std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                                 InlineContext IC)
    : InlineAdvisor(M, FAM, IC), OriginalAdvisor(std::move(OriginalAdvisor)),
      adviceMode(AdviceModes::DEFAULT) {}

void DLInlineAdvisor::onPassEntry(LazyCallGraph::SCC *SCC){};

void DLInlineAdvisor::onPassExit(LazyCallGraph::SCC *SCC) {
  std::cout << "locations seen: " << std::endl;
  for (std::string &loc : seenInlineLocations) {
    std::cout << loc << std::endl;
  }
  std::cout << "decisions made: " << std::endl;
  for (std::string &decision : decisionsTaken) {
    std::cout << decision << std::endl;
  }
  if (adviceMode == AdviceModes::OVERRIDE) {
    std::cout << "advice map: " << std::endl;
    for (auto &pair : adviceMap) {
      std::cout << pair.first << " -> " << pair.second << std::endl;
    }
  }
};

static inline std::string getLocString(DebugLoc loc, bool show_inlining) {
  std::string OS{};
  // TODO! migrate this to llvm
  if (loc.get() == nullptr) {
    exit(1);
  }
  // end migrate
  OS += loc->getFilename();
  OS += ":";
  OS += std::to_string(loc.getLine());
  if (loc.getCol() != 0) {
    OS += ":";
    OS += std::to_string(loc.getCol());
  }

  if (show_inlining) {
    if (DebugLoc InlinedAtDL = loc->getInlinedAt()) {
      OS += "@[";
      OS += getLocString(InlinedAtDL, true);
      OS += "]";
    }
  }

  return OS;
}

static inline std::string getFnString(Function *F) {
  // this needs to be more complete
  return F->getName().str();
}

std::unique_ptr<InlineAdvice> DLInlineAdvisor::getAdviceImpl(CallBase &CB) {
  auto defaultAdvice = OriginalAdvisor->getAdvice(CB);

  std::string callee = getFnString(CB.getCalledFunction());
  std::string caller = getFnString(CB.getCaller());

  std::string CallLocation{};

  DebugLoc loc = defaultAdvice->getOriginalCallSiteDebugLoc();
  seenInlineLocations.push_back(getLocString(loc, false));
  CallLocation = getLocString(loc, true);

  std::stringstream ss{};
  ss << caller << "->" << callee << " @ " << CallLocation << " : "
     << (defaultAdvice->isInliningRecommended() ? "inline" : "no-inline");
  decisionsTaken.push_back(ss.str());

  return defaultAdvice;
}
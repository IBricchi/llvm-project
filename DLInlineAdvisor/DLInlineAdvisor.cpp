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
    : InlineAdvisor(M, FAM, IC), OriginalAdvisor(std::move(OriginalAdvisor)) {

  // this is going to be passed by constructor
  // once I get constructor working properly
  std::string ctx = "";
  if (ctx == "false") {
    adviceMode = AdviceModes::FALSE;
  } else if (ctx == "true") {
    adviceMode = AdviceModes::TRUE;
  } else if (ctx == "default") {
    adviceMode = AdviceModes::DEFAULT;
  } else if (!ctx.empty()) {
    adviceMode = AdviceModes::OVERRIDE;
    parseAdviceFile(ctx);
  } else {
    adviceMode = AdviceModes::DEFAULT;
  }
}

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

  switch (adviceMode) {
  case AdviceModes::FALSE:
    defaultAdvice->recordUnattemptedInlining();
    defaultAdvice = std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), false);
    break;
  case AdviceModes::TRUE:
    defaultAdvice->recordUnattemptedInlining();
    defaultAdvice = std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), true);
    break;
  case AdviceModes::OVERRIDE:
    if (adviceMap.find(CallLocation) != adviceMap.end()) {
      defaultAdvice->recordUnattemptedInlining();
      defaultAdvice = std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB),
                                                     adviceMap[CallLocation]);
    }
    break;
  }

  std::stringstream ss{};
  ss << caller << "->" << callee << " @ " << CallLocation << " : "
     << (defaultAdvice->isInliningRecommended() ? "inline" : "no-inline");
  decisionsTaken.push_back(ss.str());

  return defaultAdvice;
}

void DLInlineAdvisor::parseAdviceFile(std::string &filename) {
  std::ifstream file(filename);

  enum {
    NODE_LIST_HEADER,
    NODE_LIST,
    DECISION_LIST,
    ERROR,
  } state = NODE_LIST_HEADER;

  std::string line;
  while (std::getline(file, line)) {
    switch (state) {
    case NODE_LIST_HEADER: {
      if (line == "locations seen: ") {
        state = NODE_LIST;
      } else {
        state = ERROR;
      }
    } break;
    case NODE_LIST: {
      if (line == "decisions made: ") {
        state = DECISION_LIST;
      }
    } break;
    case DECISION_LIST: {
      std::istringstream iss(line);
      std::string tmp;
      std::string callee;
      std::string location;
      std::string decision;
      if (!(iss >> callee >> tmp >> location >> tmp >> decision)) {
        state = ERROR;
      } else {
        adviceMap.insert({location, decision == "inline"});
      }
    } break;
    }
  }

  if (state == ERROR) {
    std::cerr << "error parsing advice file" << std::endl;
    exit(1);
  }
}
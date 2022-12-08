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

cl::opt<std::string> DLCTX("dl-inline-ctx", cl::desc("DL context file"), cl::init(""));

extern "C" std::unique_ptr<InlineAdvisor>
DLInlineAdvisorFactory(Module &M, FunctionAnalysisManager &FAM,
                       LLVMContext &Context,
                       std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                       InlineContext IC) {
  return std::make_unique<DLInlineAdvisor>(
      M, FAM, Context, std::move(OriginalAdvisor), IC);
}

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
  return F->getName().str();
}

#define DOT_FORMAT

DLInlineAdvisor::DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM,
                                 LLVMContext &Context,
                                 std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                                 InlineContext IC)
    : InlineAdvisor(M, FAM, IC), OriginalAdvisor(std::move(OriginalAdvisor)) {
  // this is going to be passed by constructor
  // once I get constructor working properly
  if (DLCTX == "false") {
    adviceMode = AdviceModes::FALSE;
  } else if (DLCTX == "true") {
    adviceMode = AdviceModes::TRUE;
  } else if (DLCTX == "default") {
    adviceMode = AdviceModes::DEFAULT;
  } else if (!DLCTX.empty()) {
    adviceMode = AdviceModes::OVERRIDE;
    parseAdviceFile(DLCTX);
  } else {
    adviceMode = AdviceModes::DEFAULT;
  }

  // print all call caller/callee pairs
  std::cout << "Call Graph:" << std::endl;
#ifdef DOT_FORMAT
  std::cout << "digraph {" << std::endl;
#endif
  for (auto &F : M) {
    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *CB = dyn_cast<CallBase>(&I)) {
          if (CB->getCalledFunction()) {
            // continue if this is llvm intrinsic
            if (CB->getCalledFunction()->isIntrinsic()) {
              continue;
            }
            std::string caller = getFnString(CB->getCaller());
            // add debug info to caller
            std::string caller_loc = getLocString(CB->getDebugLoc(), false);
            std::string callee = getFnString(CB->getCalledFunction());
            #ifdef DOT_FORMAT
            std::string location =
                caller + " -> " + callee + " [label=\"" + caller_loc + "\"]";
                #else
            std::string location =
                caller + " -> " + callee + " @ " + caller_loc;
            #endif
            std::cout << location << std::endl;
          }
        }
      }
    }
  }
#ifdef DOT_FORMAT
  std::cout << "}" << std::endl;
#endif
}

void DLInlineAdvisor::onPassEntry(LazyCallGraph::SCC *SCC) {}

void DLInlineAdvisor::onPassExit(LazyCallGraph::SCC *SCC) {
  std::cout << "Decisions:" << std::endl;
  for (std::string &decision : decisionsTaken) {
    std::cout << decision << std::endl;
  }
  // if (adviceMode == AdviceModes::OVERRIDE) {
  //   std::cout << "Advice Map:" << std::endl;
  //   for (auto &pair : adviceMap) {
  //     std::cout << pair.first << " -> " << pair.second << std::endl;
  //   }
  // }
}

std::unique_ptr<InlineAdvice> DLInlineAdvisor::getAdviceImpl(CallBase &CB) {
  auto defaultAdvice = OriginalAdvisor->getAdvice(CB);

  std::string callee = getFnString(CB.getCalledFunction());
  std::string caller = getFnString(CB.getCaller());

  std::string CallLocation{};

  DebugLoc loc = defaultAdvice->getOriginalCallSiteDebugLoc();
  CallLocation = getLocString(loc, true);

  switch (adviceMode) {
  case AdviceModes::FALSE:
    defaultAdvice->recordUnattemptedInlining();
    defaultAdvice =
        std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), false);
    break;
  case AdviceModes::TRUE:
    defaultAdvice->recordUnattemptedInlining();
    defaultAdvice =
        std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB), true);
    break;
  case AdviceModes::OVERRIDE:
    if (adviceMap.find(CallLocation) != adviceMap.end()) {
      defaultAdvice->recordUnattemptedInlining();
      defaultAdvice = std::make_unique<InlineAdvice>(this, CB, getCallerORE(CB),
                                                     adviceMap[CallLocation]);
    }
    break;
  case AdviceModes::DEFAULT:
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

  // error reading file
  bool error = false;

  // read file line by line untill Decisions is found
  std::string line;
  while (std::getline(file, line)) {
    if (line.find("Decisions:") != std::string::npos) {
      break;
    }
  }
  // read file line by line untill EOF
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string tmp;
    std::string callee;
    std::string location;
    std::string decision;
    if (!(iss >> callee >> tmp >> location >> tmp >> decision)) {
      error = false;
      break;
    } else {
      adviceMap.insert({location, decision == "inline"});
    }
  }

  if (error) {
    std::cerr << "error parsing advice file" << std::endl;
    exit(1);
  }
}
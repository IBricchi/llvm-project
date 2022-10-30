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

using namespace llvm;
#define DEBUG_TYPE "inline"

void getAdvice(InlineAdvisor &advisor, OptimizationRemarkEmitter &ORE,
               CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice);

void wrapper_getAdvice(InlineAdvisor *advisor, OptimizationRemarkEmitter *ORE,
                       CallBase *CB, bool *MandatoryOnly,
                       std::unique_ptr<InlineAdvice> *defaultAdvice) {
  getAdvice(*advisor, *ORE, *CB, *MandatoryOnly, *defaultAdvice);
}

extern "C" void dynamic_getAdvice(void *advisor, void *ORE, void *CB,
                                  void *MandatoryOnly, void *defaultAdvice) {
  wrapper_getAdvice((InlineAdvisor *)advisor, (OptimizationRemarkEmitter *)ORE,
                    (CallBase *)CB, (bool *)MandatoryOnly,
                    (std::unique_ptr<InlineAdvice> *)defaultAdvice);
}

static uint64_t pairing_function(uint64_t a, uint64_t b) {
  return (a + b) * (a + b + 1) / 2 + b;
}

#include <iostream>
#include <sstream>

#define print(x) std::cout << __LINE__ << ": " << #x << " = " << x << std::endl

std::string getLocString(DebugLoc loc, bool show_inlining) {
  // auto *Scope = cast<DIScope>(loc.getScope());
  std::string OS{};
  // DILocation *dloc = loc.get();
  // migrate this to llvm
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
      OS += " @[ ";
      OS += getLocString(InlinedAtDL, true);
      OS += " ]";
    }
  }

  return OS;
}

static std::vector<std::string> seen_inline_locations;

void getAdvice(InlineAdvisor &advisor, OptimizationRemarkEmitter &ORE,
               CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice) {
  StringRef Callee = CB.getCalledFunction()->getName();

  std::string CallLocation{};

  DebugLoc loc = defaultAdvice->getOriginalCallSiteDebugLoc();
  seen_inline_locations.push_back(getLocString(loc, false));
  CallLocation = getLocString(loc, true);

  // change any decision to a false decision
  defaultAdvice->recordUnattemptedInlining();
  defaultAdvice = std::make_unique<InlineAdvice>(&advisor, CB, ORE, false);

  // print decision
  std::cout << Callee.str() << " @ " << CallLocation << " : "
            << (defaultAdvice->isInliningRecommended() ? "inline" : "no inline")
            << std::endl;

}

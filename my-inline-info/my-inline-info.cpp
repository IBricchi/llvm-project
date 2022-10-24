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

void getAdvice(CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice);

void wrapper_getAdvice(CallBase *CB, bool *MandatoryOnly,
                       std::unique_ptr<InlineAdvice> *defaultAdvice) {
  getAdvice(*CB, *MandatoryOnly, *defaultAdvice);
}

extern "C" void dynamic_getAdvice(void *CB, void *MandatoryOnly,
                                  void *defaultAdvice) {
  wrapper_getAdvice((CallBase*) CB, (bool*) MandatoryOnly, (std::unique_ptr<InlineAdvice>*) defaultAdvice);
}

static uint64_t pairing_function(uint64_t a, uint64_t b) {
  return (a + b) * (a + b + 1) / 2 + b;
}

void getAdvice(CallBase &CB, bool MandatoryOnly,
               std::unique_ptr<InlineAdvice> &defaultAdvice){
    __builtin_printf("Hello World\n");
};

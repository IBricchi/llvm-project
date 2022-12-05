#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
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

#define DEBUG_TYPE "inline"

namespace llvm {

class CallBase;
class Function;
class LLVMContext;
class Module;

// class DLInlineAdvisor : public InlineAdvisor {
// public:
//   DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM, InlineParams Params,
//                   InlineContext IC)
//       : InlineAdvisor(M, FAM, IC),
//         OriginalAdvisor{} {}

//   std::unique_ptr<InlineAdvice> getAdviceImpl(CallBase &CB) override {
//     return OriginalAdvisor->getAdvice(CB);
//   }

// private:
//   std::unique_ptr<InlineAdvisor> OriginalAdvisor;
// };
} // namespace llvm

using namespace llvm;
extern "C" InlineAdvisor*
DLInlineAdvisorFactory(Module &M, FunctionAnalysisManager &FAM,
                       InlineParams Params, InlineContext IC) {
  return new DefaultInlineAdvisor(M, FAM, Params, IC);
}

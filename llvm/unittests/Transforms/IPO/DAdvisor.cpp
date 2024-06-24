#include "llvm/Analysis/InlineAdvisor.h"

#define DEBUG_TYPE "inline"

namespace llvm {

class DLInlineAdvisor : public DefaultInlineAdvisor {
public:
  DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM, InlineParams Params,
                  InlineContext IC)
      : DefaultInlineAdvisor(M, FAM, Params, IC) {}
};
} // namespace llvm

using namespace llvm;

extern "C" std::unique_ptr<InlineAdvisor>
DLInlineAdvisorFactory(Module &M, FunctionAnalysisManager &FAM,
                       InlineParams Params, InlineContext IC) {
  return std::make_unique<DLInlineAdvisor>(M, FAM, Params, IC);
}
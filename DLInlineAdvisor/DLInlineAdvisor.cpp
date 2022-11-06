//===- ReplayInlineAdvisor.cpp - Replay InlineAdvisor ---------------------===//
#include "DLInlineAdvisor.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>

using namespace llvm;

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
    : InlineAdvisor(M, FAM, IC), OriginalAdvisor(std::move(OriginalAdvisor)) {}

std::unique_ptr<InlineAdvice> DLInlineAdvisor::getAdviceImpl(CallBase &CB) {
  if (OriginalAdvisor)
    return OriginalAdvisor->getAdvice(CB);

  // If no decision is made above, return non-decision
  return {};
}

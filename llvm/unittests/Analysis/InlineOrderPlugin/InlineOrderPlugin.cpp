#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/InlineOrder.h"

using namespace llvm;

namespace {

class NoFooInlineOrder : public InlineOrder<std::pair<CallBase *, int>> {
public:
  NoFooInlineOrder(FunctionAnalysisManager &FAM, const InlineParams &Params,
                   ModuleAnalysisManager &MAM, Module &M) {
    // we temporarily set the PluginInlineOrderAnalysis::HasBeenRegistered
    // as false to disable the plugin loading itself and renable it later
    llvm::PluginInlineOrderAnalysis::HasBeenRegistered = false;
    DefaultInlineOrder = getInlineOrder(FAM, Params, MAM, M);
    llvm::PluginInlineOrderAnalysis::HasBeenRegistered = true;
  }
  size_t size() override { return DefaultInlineOrder->size(); }
  void push(const std::pair<CallBase *, int> &Elt) override {
    // we ignore call that include foo in the name
    if (Elt.first->getCalledFunction()->getName() == "foo") {
      DefaultInlineOrder->push(Elt);
    }
  }
  std::pair<CallBase *, int> pop() override {
    return DefaultInlineOrder->pop();
  }
  void erase_if(function_ref<bool(std::pair<CallBase *, int>)> Pred) override {
    DefaultInlineOrder->erase_if(Pred);
  }

private:
  std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>> DefaultInlineOrder;
};

std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>>
DefaultInlineOrderFactory(FunctionAnalysisManager &FAM,
                          const InlineParams &Params,
                          ModuleAnalysisManager &MAM, Module &M) {
  return std::make_unique<NoFooInlineOrder>(FAM, Params, MAM, M);
}

struct DefaultDynamicInlineOrder : PassInfoMixin<DefaultDynamicInlineOrder> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    PluginInlineOrderAnalysis DA(DefaultInlineOrderFactory);
    MAM.registerPass([&] { return DA; });
    return PreservedAnalyses::all();
  }
};

} // namespace

/* New PM Registration */
llvm::PassPluginLibraryInfo getDefaultDynamicInlineOrderPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DynamicDefaultInlineOrder",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                  MPM.addPass(DefaultDynamicInlineOrder());
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &PM,
                   ArrayRef<PassBuilder::PipelineElement> InnerPipeline) {
                  if (Name == "dynamic-inline-order") {
                    PM.addPass(DefaultDynamicInlineOrder());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDefaultDynamicInlineOrderPluginInfo();
}

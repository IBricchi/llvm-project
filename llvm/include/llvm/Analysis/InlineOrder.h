//===- InlineOrder.h - Inlining order abstraction -*- C++ ---*-------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
#ifndef LLVM_ANALYSIS_INLINEORDER_H
#define LLVM_ANALYSIS_INLINEORDER_H

#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/Analysis/InlineCost.h"
#include <utility>

namespace llvm {
class CallBase;

template <typename T> class InlineOrder {
public:
  virtual ~InlineOrder() = default;

  virtual size_t size() = 0;

  virtual void push(const T &Elt) = 0;

  virtual T pop() = 0;

  virtual void erase_if(function_ref<bool(T)> Pred) = 0;

  bool empty() { return !size(); }
};

std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>>
getInlineOrder(FunctionAnalysisManager &FAM, const InlineParams &Params,
               ModuleAnalysisManager &MAM, Module &M);

/// Used for dynamically registering InlineOrder as plugins
///
/// An inline order plugin adds a new inline order at runtime by registering
/// an instance of PluginInlineOrderAnalysis in the current
/// ModuleAnalysisManager. For example, the following code dynamically registers
/// the default inline order:
///
/// namespace {
///
/// std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>>
/// DefaultInlineOrderFactory(FunctionAnalysisManager &FAM,
///                           const InlineParams &Params,
///                           ModuleAnalysisManager &MAM, Module &M) {
///   llvm::PluginInlineOrderAnalysis::HasBeenRegistered = false;
///   auto DefaultInlineOrder =
///       std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>>(
///           getInlineOrder(FAM, Params, MAM, M));
///   llvm::PluginInlineOrderAnalysis::HasBeenRegistered = true;
///   return DefaultInlineOrder;
/// }
///
/// struct DefaultDynamicInlineOrder : PassInfoMixin<DefaultDynamicInlineOrder>
/// {
///   PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
///     PluginInlineOrderAnalysis DA(DefaultInlineOrderFactory);
///     MAM.registerPass([&] { return DA; });
///     return PreservedAnalyses::all();
///   }
/// };
///
/// } // namespace
///
/// extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
/// llvmGetPassPluginInfo() {
///   return {LLVM_PLUGIN_API_VERSION, "DefaultDynamicInlineOrder",
///   LLVM_VERSION_STRING,
///           [](PassBuilder &PB) {
///             PB.registerPipelineStartEPCallback(
///                 [](ModulePassManager &MPM, OptimizationLevel Level) {
///                   MPM.addPass(DefaultDynamicInlineOrder());
///                 });
///           }};
/// }
///
/// A plugin must implement an InlineOrderFactory and register it with a
/// PluginInlineOrderAnalysis to the provided ModuleanAlysisManager.
///
/// If such a plugin has been registered
/// llvm::getInlineOrder will return the dynamically loaded inliner order.
///
class PluginInlineOrderAnalysis
    : public AnalysisInfoMixin<PluginInlineOrderAnalysis> {
public:
  static AnalysisKey Key;
  static bool HasBeenRegistered;

  typedef std::unique_ptr<InlineOrder<std::pair<CallBase *, int>>> (
      *InlineOrderFactory)(FunctionAnalysisManager &FAM,
                           const InlineParams &Params,
                           ModuleAnalysisManager &MAM, Module &M);

  PluginInlineOrderAnalysis(InlineOrderFactory Factory) : Factory(Factory) {
    HasBeenRegistered = true;
    assert(Factory != nullptr &&
           "The plugin inline order factory should not be a null pointer.");
  }

  struct Result {
    InlineOrderFactory Factory;
  };

  Result run(Module &, ModuleAnalysisManager &) { return {Factory}; }
  Result getResult() { return {Factory}; }

private:
  InlineOrderFactory Factory;
};

} // namespace llvm
#endif // LLVM_ANALYSIS_INLINEORDER_H

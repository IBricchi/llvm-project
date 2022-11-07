#pragma once

#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/InlineAdvisor.h"

namespace llvm {
class CallBase;
class Function;
class LLVMContext;
class Module;

/// Replay inline advisor that uses optimization remarks from inlining of
/// previous build to guide current inlining. This is useful for inliner tuning.
class DLInlineAdvisor : public InlineAdvisor {
public:
  DLInlineAdvisor(Module &M, FunctionAnalysisManager &FAM, LLVMContext &Context,
                  std::unique_ptr<InlineAdvisor> OriginalAdvisor,
                  InlineContext IC);

  virtual void onPassEntry(LazyCallGraph::SCC *SCC = nullptr) override;
  virtual void onPassExit(LazyCallGraph::SCC *SCC = nullptr) override;

  std::unique_ptr<InlineAdvice> getAdviceImpl(CallBase &CB) override;

private:
  std::unique_ptr<InlineAdvisor> OriginalAdvisor;

  enum struct AdviceModes {
    DEFAULT,
    FALSE,
    TRUE,
    OVERRIDE,
  } adviceMode;

  std::unordered_map<std::string, bool> adviceMap;

  std::vector<std::string> seenInlineLocations;
  std::vector<std::string> decisionsTaken;

  void parseAdviceFile(std::string& filename);

};
} // namespace llvm
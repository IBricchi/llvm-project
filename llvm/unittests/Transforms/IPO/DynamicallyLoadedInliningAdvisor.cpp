#include "llvm/AsmParser/Parser.h"
#include "llvm/Config/config.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

extern llvm::cl::opt<std::string> DLInlineAdvisorPath;

namespace llvm {

void anchor() {}

static std::string libPath(const std::string Name = "DAdvisor") {
  const auto &Argvs = testing::internal::GetArgvs();
  const char *Argv0 = Argvs.size() > 0 ? Argvs[0].c_str() : "PluginsTests";
  void *Ptr = (void *)(intptr_t)anchor;
  std::string Path = sys::fs::getMainExecutable(Argv0, Ptr);
  llvm::SmallString<256> Buf{sys::path::parent_path(Path)};
  sys::path::append(Buf, (Name + LLVM_PLUGIN_EXT).c_str());
  return std::string(Buf.str());
}

TEST(DynamicInliningAdvisorTest, Foo) {
  outs() << DLInlineAdvisorPath << '\n';
  DLInlineAdvisorPath = libPath();
  outs() << DLInlineAdvisorPath << '\n';

  LLVMContext Ctx;
  ModulePassManager MPM;
  auto IP = getInlineParams(3, 0);

  PassBuilder PB;
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  MPM.addPass(ModuleInlinerPass(IP, InliningAdvisorMode::Default,
                                ThinOrFullLTOPhase::None));

  SMDiagnostic Error;
  StringRef Text = R"(; Function Attrs: nounwind sspstrong uwtable
        define dso_local i32 @baz(i32 noundef %0) #0 {
          %2 = alloca i32, align 4
          store i32 %0, i32* %2, align 4, !tbaa !5
          %3 = load i32, i32* %2, align 4, !tbaa !5
          %4 = mul nsw i32 %3, 2
          ret i32 %4
        }

        ; Function Attrs: nounwind sspstrong uwtable
        define dso_local i32 @bar(i32 noundef %0) #0 {
          %2 = alloca i32, align 4
          store i32 %0, i32* %2, align 4, !tbaa !5
          %3 = load i32, i32* %2, align 4, !tbaa !5
          %4 = call i32 @baz(i32 noundef %3)
          ret i32 %4
        }

        ; Function Attrs: nounwind sspstrong uwtable
        define dso_local i32 @foo(i32 noundef %0) #0 {
          %2 = alloca i32, align 4
          store i32 %0, i32* %2, align 4, !tbaa !5
          %3 = load i32, i32* %2, align 4, !tbaa !5
          %4 = call i32 @bar(i32 noundef %3)
          ret i32 %4
        }

        attributes #0 = { nounwind sspstrong uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

        !llvm.module.flags = !{!0, !1, !2, !3}
        !llvm.ident = !{!4}

        !0 = !{i32 1, !"wchar_size", i32 4}
        !1 = !{i32 7, !"PIC Level", i32 2}
        !2 = !{i32 7, !"PIE Level", i32 2}
        !3 = !{i32 7, !"uwtable", i32 1}
        !4 = !{!"clang version 14.0.6"}
        !5 = !{!6, !6, i64 0}
        !6 = !{!"int", !7, i64 0}
        !7 = !{!"omnipotent char", !8, i64 0}
        !8 = !{!"Simple C/C++ TBAA"})";

  std::unique_ptr<Module> M = parseAssemblyString(Text, Error, Ctx);
  ASSERT_TRUE(M);
  MPM.run(*M, MAM);
}

} // namespace llvm

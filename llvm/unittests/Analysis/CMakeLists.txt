set(LLVM_LINK_COMPONENTS
  Analysis
  AsmParser
  Core
  Passes
  Support
  TargetParser
  TransformUtils
  IPO
  )

set(ANALYSIS_TEST_SOURCES
  AliasAnalysisTest.cpp
  AliasSetTrackerTest.cpp
  AssumeBundleQueriesTest.cpp
  BasicAliasAnalysisTest.cpp
  BlockFrequencyInfoTest.cpp
  BranchProbabilityInfoTest.cpp
  CallGraphTest.cpp
  CaptureTrackingTest.cpp
  CFGTest.cpp
  CGSCCPassManagerTest.cpp
  ConstraintSystemTest.cpp
  DDGTest.cpp
  DivergenceAnalysisTest.cpp
  DomTreeUpdaterTest.cpp
  GlobalsModRefTest.cpp
  FunctionPropertiesAnalysisTest.cpp
  InlineCostTest.cpp
  IRSimilarityIdentifierTest.cpp
  IVDescriptorsTest.cpp
  LazyCallGraphTest.cpp
  LoadsTest.cpp
  LoopInfoTest.cpp
  LoopNestTest.cpp
  MemoryBuiltinsTest.cpp
  MemoryProfileInfoTest.cpp
  MemorySSATest.cpp
  MLModelRunnerTest.cpp
  PhiValuesTest.cpp
  PluginInlineAdvisorAnalysisTest.cpp
  PluginInlineOrderAnalysisTest.cpp
  ProfileSummaryInfoTest.cpp
  ScalarEvolutionTest.cpp
  VectorFunctionABITest.cpp
  SparsePropagation.cpp
  TargetLibraryInfoTest.cpp
  TensorSpecTest.cpp
  TBAATest.cpp
  UnrollAnalyzerTest.cpp
  ValueLatticeTest.cpp
  ValueTrackingTest.cpp
  VectorUtilsTest.cpp
  )

# The unit tests target does not use InlineAdvisorPlugin.cpp
# so if not added to LLVM_OPTIONAL_SOURCES, FileCheck will
# complain about unused file.
set(LLVM_OPTIONAL_SOURCES InlineAdvisorPlugin.cpp)

set(MLGO_TESTS TFUtilsTest.cpp)
if (LLVM_HAVE_TFLITE)
  LIST(APPEND ANALYSIS_TEST_SOURCES ${MLGO_TESTS})
else()
  LIST(APPEND LLVM_OPTIONAL_SOURCES ${MLGO_TESTS})
endif()

add_llvm_unittest_with_input_files(AnalysisTests
  ${ANALYSIS_TEST_SOURCES}
  )

target_link_libraries(AnalysisTests PRIVATE LLVMTestingSupport)

add_subdirectory(InlineAdvisorPlugin)
add_subdirectory(InlineOrderPlugin)

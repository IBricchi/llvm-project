CXX = g++
CXXFLAGS = -fPIC -shared -O3
INCLUDE_PATHS = -I../llvm/include/ -I../../llvm/build/include/

# EXTRA_FILES = /home/ibricchi/llvm-inliner/llvm/lib/Analysis/InlineAdvisor.cpp
# EXTRA_FILES += /home/ibricchi/llvm-inliner/llvm/lib/Support/CommandLine.cpp

DLInlineAdvisor.so: DLInlineAdvisor.cpp DLInlineAdvisor.h $(EXTRA_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) $(EXTRA_FILES) $< -o $@
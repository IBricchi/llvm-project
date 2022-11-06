CXX = g++
CXXFLAGS = -fPIC -shared -O3
INCLUDE_PATHS = -I../llvm/include/ -I../../llvm/build/include/

DLInlineAdvisor.so: DLInlineAdvisor.cpp DLInlineAdvisor.h
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) $< -o $@
CXX = g++
CXXFLAGS = -fPIC -shared -O3
INCLUDE_PATHS = -I../llvm/include/ -I../../llvm/build/include/

my-inline-info.so: my-inline-info.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(INCLUDE_PATHS)
# Set the minimum CMake version required and the C++ standard.
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)

# Set the project name and specify the language used.
project(example)

# Find the LLVM libraries and headers.
find_package(LLVM REQUIRED CONFIG)

# Include the AddLLVM.cmake file to use the add_llvm_library function.
include(AddLLVM)

# Set the include directories.
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Use the add_llvm_library function to build an LLVM library.
add_llvm_pass_plugin(example example.cpp)


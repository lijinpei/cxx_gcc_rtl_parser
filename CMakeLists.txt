cmake_minimum_required (VERSION 3.12.3)
project (grp CXX)

find_package (LLVM REQUIRED CONFIG)
message (STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message (STATUS "Using LLVMConfig.cmake i: ${LLVM_DIR}")
include_directories (${LLVM_INCLUDE_DIRS})
add_definitions (${LLVM_DEFINITIONS})

add_executable(grp main.cpp lexer.cpp parser.cpp)
llvm_map_components_to_libnames(llvm_libs support core)
target_link_libraries (grp ${llvm_libs})

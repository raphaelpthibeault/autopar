cmake_minimum_required(VERSION 3.29)
project(autopar)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})

add_executable(autopar
    src/main.cpp
    src/concepts.cpp
    src/TaskCreationVisitor.cpp
)

include_directories(
  inc/
)

target_link_libraries(autopar
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
)

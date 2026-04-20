find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_library(scy
  lexer.cpp
  parser.cpp
  ast.cpp
  type.cpp
  ast_visualizer.cpp
  ast_printer.cpp
  scope.cpp
  semantic_analyzer.cpp
  codegen.cpp
)

target_include_directories(scy
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  SYSTEM PUBLIC
    $<BUILD_INTERFACE:${LLVM_INCLUDE_DIRS}>
)

set_target_properties(LLVMCore LLVMSupport PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${LLVM_INCLUDE_DIRS}")

target_link_libraries(scy
  PUBLIC
    LLVMCore
    LLVMSupport
  PRIVATE
    project_options
    project_sanitizers
    project_coverage
)

target_compile_definitions(scy
  PUBLIC
    ${LLVM_DEFINITIONS}
)

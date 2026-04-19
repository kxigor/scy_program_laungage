add_library(scy
  lexer.cpp
  parser.cpp
  ast.cpp
  type.cpp
  ast_visualizer.cpp
  ast_printer.cpp
  scope.cpp
  semantic_analyzer.cpp
)

target_link_libraries(scy
  PRIVATE
    project_options
    project_sanitizers
    project_coverage
)

target_include_directories(scy
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)

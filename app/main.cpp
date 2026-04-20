#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <fstream>
#include <include/ast.hpp>
#include <include/ast_visualizer.hpp>
#include <include/codegen.hpp>
#include <include/lexer.hpp>
#include <include/parser.hpp>
#include <include/semantic_analyzer.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace {
llvm::cl::opt<std::string> input_filename(llvm::cl::Positional,
                                          llvm::cl::desc("<input file>"),
                                          llvm::cl::init("-"));

llvm::cl::opt<std::string> output_filename("o",
                                           llvm::cl::desc("Output IR file"),
                                           llvm::cl::value_desc("filename"),
                                           llvm::cl::init(""));

llvm::cl::opt<bool> emit_dot("emit-dot",
                             llvm::cl::desc("Emit AST as DOT graph"),
                             llvm::cl::init(false));

llvm::cl::opt<bool> print_ast("print-ast",
                              llvm::cl::desc("Print AST to stderr"),
                              llvm::cl::init(false));

std::string read_source(const std::string& filename) {
  if (filename == "-") {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
  }
  std::ifstream file(filename);
  if (!file.is_open()) {
    llvm::errs() << "Error: cannot open file '" << filename << "'\n";
    return "";
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}
}  // namespace

int main(int argc, char** argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "scy compiler\n");

  const auto kSource = read_source(input_filename);
  if (kSource.empty() && input_filename != "-") {
    return 1;
  }

  scy::Lexer lexer(kSource);
  auto tokens = lexer.tokenize();

  scy::Parser parser(std::move(tokens));
  auto program = parser.parse();

  if (parser.has_errors()) {
    for (const auto& err : parser.errors()) {
      llvm::errs() << "Parse error: " << err.what() << "\n";
    }
    return 1;
  }

  if (print_ast) {
    scy::print_ast(std::cerr, program);
  }

  if (emit_dot) {
    scy::visualize_ast(std::cout, program);
    return 0;
  }

  scy::SemanticAnalyzer sema;
  auto sema_result = sema.analyze(program);

  if (!sema_result.errors.empty()) {
    for (const auto& err : sema_result.errors) {
      llvm::errs() << "Semantic error at line " << err.location.line << ", col "
                   << err.location.column << ": " << err.message << "\n";
    }
    return 1;
  }

  scy::CodeGen codegen("scy_module");
  if (!codegen.generate(program, sema_result)) {
    for (const auto& err : codegen.errors()) {
      llvm::errs() << "Codegen error: " << err << "\n";
    }
    return 1;
  }

  if (!output_filename.empty()) {
    if (!codegen.dump_to_file(output_filename)) {
      llvm::errs() << "Error: cannot write to '" << output_filename << "'\n";
      return 1;
    }
  } else {
    codegen.print_ir(llvm::outs());
  }

  return 0;
}

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <fstream>
#include <include/ast_visualizer.hpp>
#include <include/codegen.hpp>
#include <include/lexer.hpp>
#include <include/parser.hpp>
#include <include/semantic_analyzer.hpp>
#include <iostream>
#include <sstream>
#include <string>

static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input file>"),
                                                llvm::cl::init("-"));

static llvm::cl::opt<std::string> OutputFilename(
    "o", llvm::cl::desc("Output IR file"), llvm::cl::value_desc("filename"),
    llvm::cl::init(""));

static llvm::cl::opt<bool> EmitDot("emit-dot",
                                   llvm::cl::desc("Emit AST as DOT graph"),
                                   llvm::cl::init(false));

static llvm::cl::opt<bool> PrintAST("print-ast",
                                    llvm::cl::desc("Print AST to stderr"),
                                    llvm::cl::init(false));

static std::string read_source(const std::string& filename) {
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

int main(int argc, char** argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "scy compiler\n");

  std::string source = read_source(InputFilename);
  if (source.empty() && InputFilename != "-") {
    return 1;
  }

  scy::Lexer lexer(source);
  auto tokens = lexer.tokenize();

  scy::Parser parser(std::move(tokens));
  auto program = parser.parse();

  if (parser.has_errors()) {
    for (const auto& err : parser.errors()) {
      llvm::errs() << "Parse error: " << err.what() << "\n";
    }
    return 1;
  }

  if (PrintAST) {
    scy::print_ast(std::cerr, program);
  }

  if (EmitDot) {
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

  if (!OutputFilename.empty()) {
    if (!codegen.dump_to_file(OutputFilename)) {
      llvm::errs() << "Error: cannot write to '" << OutputFilename << "'\n";
      return 1;
    }
  } else {
    codegen.print_ir(llvm::outs());
  }

  return 0;
}

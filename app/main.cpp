#include <include/ast_visualizer.hpp>
#include <include/lexer.hpp>
#include <include/parser.hpp>
#include <iostream>
#include <utility>

int main() {
  const char* program_code = R"(
int sum_of_two(int a, int b) {
  return a + b;
}

void printer() {
  print 1 + 5;
  print sum_of_two(1, 5);
}

int main() {
  printer();
  
  int a = 1;
  int b = 2;
  int c = 3;
  a = a + b;
  b = b + c;
  c = c + a;
  a = b + c;
  b = c + a;
  c = a + b;
  if (a > 5) {
    print a;
    if (b > 8) {
      print b;
      if(c > 11) {
        print c;
        if(a + b + c > 50) {
          print 666;
        }
      }
    }
  }
  return 0;
}
)";

  scy::Lexer lexer(program_code);
  auto tokens = lexer.tokenize();

  scy::Parser parser(std::move(tokens));
  auto program = parser.parse();

  scy::visualize_ast(std::cout, program);
}
#include <gtest/gtest.h>

#include <include/lexer.hpp>
#include <include/parser.hpp>
#include <include/semantic_analyzer.hpp>

namespace scy::test {

class SemanticAnalyzerTest : public ::testing::Test {
 protected:
  SemanticResult analyze(StringViewT source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
    SemanticAnalyzer analyzer;
    return analyzer.analyze(program);
  }

  void expect_no_errors(const SemanticResult& result) {
    EXPECT_TRUE(result.errors.empty())
        << "Expected no errors, got " << result.errors.size() << ": "
        << (result.errors.empty() ? "" : result.errors[0].message);
  }

  void expect_error(const SemanticResult& result, SemanticErrorKind kind) {
    bool found = false;
    for (const auto& err : result.errors) {
      if (err.kind == kind) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Expected error of given kind not found";
  }
};

TEST_F(SemanticAnalyzerTest, ValidSimpleProgram) {
  auto result = analyze("int main() { return 0; }");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, ValidVariableUsage) {
  auto result = analyze(R"(
    int main() {
      int x = 10;
      int y = x;
      return y;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, UndeclaredVariable) {
  auto result = analyze(R"(
    int main() {
      return x;
    }
  )");
  expect_error(result, SemanticErrorKind::UndeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, RedeclaredVariableInSameScope) {
  auto result = analyze(R"(
    int main() {
      int x = 1;
      int x = 2;
      return x;
    }
  )");
  expect_error(result, SemanticErrorKind::RedeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, ShadowingAllowed) {
  auto result = analyze(R"(
    int main() {
      int x = 1;
      if (x > 0) {
        int x = 2;
        return x;
      }
      return x;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, UndeclaredFunction) {
  auto result = analyze(R"(
    int main() {
      return foo(1);
    }
  )");
  expect_error(result, SemanticErrorKind::UndeclaredFunction);
}

TEST_F(SemanticAnalyzerTest, ForwardFunctionCall) {
  auto result = analyze(R"(
    int main() {
      return add(1, 2);
    }

    int add(int a, int b) {
      return a + b;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, GlobalVariableAccess) {
  auto result = analyze(R"(
    int g = 42;
    int main() {
      return g;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, ParameterAccess) {
  auto result = analyze(R"(
    int add(int a, int b) {
      return a + b;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, DuplicateParameter) {
  auto result = analyze(R"(
    int bad(int a, int a) {
      return a;
    }
  )");
  expect_error(result, SemanticErrorKind::RedeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, NestedScopes) {
  auto result = analyze(R"(
    int main() {
      int a = 1;
      {
        int b = 2;
        {
          int c = a + b;
        }
      }
      return a;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, VariableOutOfScope) {
  auto result = analyze(R"(
    int main() {
      {
        int x = 10;
      }
      return x;
    }
  )");
  expect_error(result, SemanticErrorKind::UndeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, AssignToUndeclared) {
  auto result = analyze(R"(
    void main() {
      x = 5;
    }
  )");
  expect_error(result, SemanticErrorKind::UndeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, ComplexProgram) {
  auto result = analyze(R"(
    int global = 100;

    void print(int x) {}

    int add(int a, int b) {
      return a + b;
    }

    int main() {
      int x = 10;
      int y = 20;
      if (x < y) {
        print(add(x, y));
      } else {
        print(0);
      }
      return 0;
    }
  )");
  expect_no_errors(result);
}

TEST_F(SemanticAnalyzerTest, RedeclaredGlobalFunction) {
  auto result = analyze(R"(
    int foo() { return 0; }
    int foo() { return 1; }
  )");
  expect_error(result, SemanticErrorKind::RedeclaredVariable);
}

TEST_F(SemanticAnalyzerTest, ScopeTreeStructure) {
  auto result = analyze(R"(
    int main() {
      int a = 1;
      {
        int b = 2;
      }
      {
        int c = 3;
      }
      return a;
    }
  )");
  expect_no_errors(result);

  // Global scope should have children (function scope)
  ASSERT_FALSE(result.global_scope->children().empty());

  // Function scope (main) should have children (the two block scopes)
  const auto* func_scope = result.global_scope->children()[0].get();
  // func_scope has the block scopes as children
  EXPECT_GE(func_scope->children().size(), 2);
}

}  // namespace scy::test

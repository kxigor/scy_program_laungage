#include <gtest/gtest.h>

#include <include/codegen.hpp>
#include <include/lexer.hpp>
#include <include/parser.hpp>
#include <include/semantic_analyzer.hpp>

#include <llvm/Support/raw_ostream.h>

#include <string>

namespace scy::test {

class CodeGenTest : public ::testing::Test {
 protected:
  struct CompileResult {
    bool success;
    StringT ir;
    VectorT<StringT> errors;
  };

  CompileResult compile(StringViewT source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();

    SemanticAnalyzer sema;
    auto sema_result = sema.analyze(program);

    CodeGen codegen("test_module");
    bool ok = codegen.generate(program, sema_result);

    StringT ir_str;
    llvm::raw_string_ostream ir_os(ir_str);
    codegen.print_ir(ir_os);

    return CompileResult{ok, ir_str, codegen.errors()};
  }
};

TEST_F(CodeGenTest, EmptyMainFunction) {
  auto result = compile("int main() { return 0; }");
  ASSERT_TRUE(result.success) << result.errors[0];
  EXPECT_NE(result.ir.find("define i32 @main()"), StringT::npos);
  EXPECT_NE(result.ir.find("ret i32 0"), StringT::npos);
}

TEST_F(CodeGenTest, SimpleReturn) {
  auto result = compile("int main() { return 42; }");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("ret i32 42"), StringT::npos);
}

TEST_F(CodeGenTest, Addition) {
  auto result = compile("int main() { return 1 + 2; }");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("add"), StringT::npos);
}

TEST_F(CodeGenTest, LocalVariable) {
  auto result = compile(R"(
    int main() {
      int x = 10;
      return x;
    }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("alloca i32"), StringT::npos);
  EXPECT_NE(result.ir.find("store i32 10"), StringT::npos);
}

TEST_F(CodeGenTest, VariableAssignment) {
  auto result = compile(R"(
    int main() {
      int x = 1;
      x = 2;
      return x;
    }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("store i32 2"), StringT::npos);
}

TEST_F(CodeGenTest, FunctionCall) {
  auto result = compile(R"(
    int add(int a, int b) { return a + b; }
    int main() { return add(3, 4); }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("define i32 @add(i32"), StringT::npos);
  EXPECT_NE(result.ir.find("call i32 @add"), StringT::npos);
}

TEST_F(CodeGenTest, VoidFunction) {
  auto result = compile(R"(
    void noop() {}
    int main() { noop(); return 0; }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("define void @noop()"), StringT::npos);
  EXPECT_NE(result.ir.find("call void @noop()"), StringT::npos);
}

TEST_F(CodeGenTest, IfStatement) {
  auto result = compile(R"(
    int main() {
      int x = 5;
      if (x > 3) {
        return 1;
      }
      return 0;
    }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("icmp sgt"), StringT::npos);
  EXPECT_NE(result.ir.find("br i1"), StringT::npos);
}

TEST_F(CodeGenTest, IfElseStatement) {
  auto result = compile(R"(
    int main() {
      int x = 5;
      if (x > 3) {
        return 1;
      } else {
        return 0;
      }
    }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("then"), StringT::npos);
  EXPECT_NE(result.ir.find("else"), StringT::npos);
}

TEST_F(CodeGenTest, GlobalVariable) {
  auto result = compile(R"(
    int g = 42;
    int main() { return g; }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("@g = global i32 42"), StringT::npos);
}

TEST_F(CodeGenTest, UnaryNot) {
  auto result = compile(R"(
    int main() { return !0; }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("icmp eq"), StringT::npos);
}

TEST_F(CodeGenTest, UnaryNeg) {
  auto result = compile(R"(
    int main() { return -5; }
  )");
  ASSERT_TRUE(result.success);
  // neg is usually lowered to `sub 0, x`
  EXPECT_TRUE(result.ir.find("sub") != StringT::npos ||
              result.ir.find("neg") != StringT::npos);
}

TEST_F(CodeGenTest, ComparisonOperators) {
  auto result = compile(R"(
    int main() {
      int a = 1;
      int b = 2;
      int eq = a == b;
      int ne = a != b;
      int lt = a < b;
      int gt = a > b;
      return eq + ne + lt + gt;
    }
  )");
  ASSERT_TRUE(result.success);
  EXPECT_NE(result.ir.find("icmp eq"), StringT::npos);
  EXPECT_NE(result.ir.find("icmp ne"), StringT::npos);
  EXPECT_NE(result.ir.find("icmp slt"), StringT::npos);
  EXPECT_NE(result.ir.find("icmp sgt"), StringT::npos);
}

TEST_F(CodeGenTest, ComplexProgram) {
  auto result = compile(R"(
    void print(int x) {}

    int sum(int a, int b) {
      return a + b;
    }

    int main() {
      int x = 10;
      int y = 20;
      int z = sum(x, y);
      if (z > 25) {
        print(z);
      } else {
        print(0);
      }
      return 0;
    }
  )");
  ASSERT_TRUE(result.success);
}

}  // namespace scy::test

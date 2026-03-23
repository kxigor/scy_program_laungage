#include <cstddef>
#include <include/ast.hpp>
#include <include/ast_visualizer.hpp>
#include <include/config.hpp>
#include <ostream>
#include <string>
#include <type_traits>
#include <variant>

namespace scy {

void AstVisualizer::generate_dot(const Program& program) {
  os_ << "digraph AST {\n";
  os_ << "  node [fontname=\"Helvetica\", fontsize=10];\n";
  os_ << "  edge [fontname=\"Helvetica\", fontsize=9];\n\n";

  visit_program(program);

  os_ << "}\n";
}

std::size_t AstVisualizer::next_id() { return id_counter_++; }

void AstVisualizer::add_node(std::size_t id, const StringT& label,
                             const StringT& shape) {
  os_ << "  node" << id << " [label=\"" << label << "\", shape=" << shape
      << "];\n";
}

void AstVisualizer::add_edge(std::size_t from, std::size_t to,
                             const StringT& label) {
  os_ << "  node" << from << " -> node" << to;
  if (!label.empty()) {
    os_ << " [label=\"" << label << "\"]";
  }
  os_ << ";\n";
}

std::size_t AstVisualizer::visit_program(const Program& program) {
  const std::size_t kId = next_id();
  add_node(kId, "Program", "ellipse");

  for (const auto& decl : program.declarations) {
    const std::size_t kChildId = visit_declaration(*decl);
    add_edge(kId, kChildId);
  }

  return kId;
}

std::size_t AstVisualizer::visit_declaration(const Declaration& decl) {
  return std::visit(
      [&](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, FunctionDecl>) {
          const std::size_t kId = next_id();
          StringT label = "Function\\n" + StringT(arg.return_type.as_text()) +
                          " " + StringT(arg.name) + "(";
          for (std::size_t i = 0; i < arg.params.size(); ++i) {
            if (i > 0) {
              label += ", ";
            }
            label += StringT(arg.params[i].type.as_text()) + " " +
                     StringT(arg.params[i].name);
          }
          label += ")";
          add_node(kId, label, "box");

          for (const auto& stmt : arg.body) {
            const std::size_t kChildId = visit_statement(*stmt);
            add_edge(kId, kChildId);
          }

          return kId;

        } else if constexpr (std::is_same_v<T, GlobalVarDecl>) {
          const std::size_t kId = next_id();
          const StringT kLabel = "GlobalVar\\n" + StringT(arg.type.as_text()) +
                                 " " + StringT(arg.name);
          add_node(kId, kLabel, "box");

          if (arg.initializer) {
            const std::size_t kInitId = visit_expression(**arg.initializer);
            add_edge(kId, kInitId, "init");
          }

          return kId;
        }

        return 0;
      },
      decl.data);
}

std::size_t AstVisualizer::visit_statement(const Statement& stmt) {
  return std::visit(
      [&](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, ExpressionStmt>) {
          const std::size_t kId = next_id();
          add_node(kId, "ExprStmt", "box");
          const std::size_t kExprId = visit_expression(*arg.expression);
          add_edge(kId, kExprId);
          return kId;

        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          const std::size_t kId = next_id();
          add_node(kId, "Return", "box");
          if (arg.value) {
            const std::size_t kValId = visit_expression(**arg.value);
            add_edge(kId, kValId);
          }
          return kId;

        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          const std::size_t kId = next_id();
          add_node(kId, "Block", "box");
          for (const auto& s : arg.statements) {
            const std::size_t kChildId = visit_statement(*s);
            add_edge(kId, kChildId);
          }
          return kId;

        } else if constexpr (std::is_same_v<T, IfStmt>) {
          const std::size_t kId = next_id();
          add_node(kId, "If", "diamond");

          const std::size_t kCondId = visit_expression(*arg.condition);
          add_edge(kId, kCondId, "cond");

          const std::size_t kThenId = visit_statement(*arg.then_branch);
          add_edge(kId, kThenId, "then");

          if (arg.else_branch) {
            const std::size_t kElseId = visit_statement(**arg.else_branch);
            add_edge(kId, kElseId, "else");
          }

          return kId;

        } else if constexpr (std::is_same_v<T, VarDeclStmt>) {
          const std::size_t kId = next_id();
          const StringT kLabel = "VarDecl\\n" + StringT(arg.type.as_text()) +
                                 " " + StringT(arg.name);
          add_node(kId, kLabel, "box");

          if (arg.initializer) {
            const std::size_t kInitId = visit_expression(**arg.initializer);
            add_edge(kId, kInitId, "init");
          }

          return kId;
        }

        return 0;
      },
      stmt.data);
}

std::size_t AstVisualizer::visit_expression(const Expression& expr) {
  return std::visit(
      [&](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, NumberExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, StringT(arg.literal), "ellipse");
          return kId;

        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, StringT(arg.name), "ellipse");
          return kId;

        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, "Unary " + StringT(arg.op.lexem), "circle");
          const std::size_t kOperandId = visit_expression(*arg.operand);
          add_edge(kId, kOperandId);
          return kId;

        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, StringT(arg.op.lexem), "circle");
          const std::size_t kLeftId = visit_expression(*arg.left);
          const std::size_t kRightId = visit_expression(*arg.right);
          add_edge(kId, kLeftId, "L");
          add_edge(kId, kRightId, "R");
          return kId;

        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, "Assign", "circle");

          const std::size_t kNameId = next_id();
          add_node(kNameId, StringT(arg.name), "ellipse");
          add_edge(kId, kNameId, "target");

          const std::size_t kValId = visit_expression(*arg.value);
          add_edge(kId, kValId, "value");

          return kId;

        } else if constexpr (std::is_same_v<T, CallExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, "Call\\n" + StringT(arg.callee), "hexagon");

          for (std::size_t i = 0; i < arg.arguments.size(); ++i) {
            const std::size_t kArgId = visit_expression(*arg.arguments[i]);
            add_edge(kId, kArgId, "arg" + std::to_string(i));
          }

          return kId;

        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          const std::size_t kId = next_id();
          add_node(kId, "( )", "circle");
          const std::size_t kInnerId = visit_expression(*arg.expression);
          add_edge(kId, kInnerId);
          return kId;
        }

        return 0;
      },
      expr.data);
}

void visualize_ast(std::ostream& os, const Program& program) {
  AstVisualizer visualizer(os);
  visualizer.generate_dot(program);
}

}  // namespace scy
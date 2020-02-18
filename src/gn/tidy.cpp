/**
 * gn-tidy
 *
 * This is an example tool used for parsing and editing GN files, similar to
 * Clang tidy.
 *
 * Usage:
 *
 *   $ gn-tidy path/to/BUILD.gn  // Apply configs to targets that have a
 *                               // particular source file
 *   $ gn-tidy path/to/BUILD.gn  --dump-ast  // Dump an AST
 */
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/json/json_reader.h"
#include "gn/input_file.h"
#include "gn/parser.h"
#include "gn/tokenizer.h"

namespace {

bool SetInputFileContents(InputFile &input_file, const std::string &filename) {
  base::FilePath filepath(filename);
  return input_file.Load(filepath);
}

struct CommandLineArgs {
  std::string filename;
  std::string extra_args;
  bool dump_ast = false;
};

bool ParseCmdLineArgs(int argc, char **argv, CommandLineArgs &cmd_line_args) {
  for (int i = 1; i < argc;) {
    std::string arg(argv[i]);

    if (arg == "--dump-ast") {
      cmd_line_args.dump_ast = true;
      ++i;
      continue;
    } else if (arg == "--args") {
      if (i < argc - 1) {
        std::cerr << "Missing argument for --args\n";
        return false;
      }
      cmd_line_args.extra_args = argv[i+1];
      i += 2;
      continue;
    } else if (cmd_line_args.filename.empty()) {
      cmd_line_args.filename = arg;
      ++i;
      continue;
    } else {
      std::cerr << "Unhandled argument: " << arg << "\n";
      return false;
    }
  }
  return true;
}

class ConstNodeVisitor {
 public:
  void VisitNode(const ParseNode &node) {
    if (node.AsAccessor())  Visit(*node.AsAccessor());
    else if (node.AsBinaryOp())  Visit(*node.AsBinaryOp());
    else if (node.AsBlockComment())  Visit(*node.AsBlockComment());
    else if (node.AsBlock())  Visit(*node.AsBlock());
    else if (node.AsConditionNode())  Visit(*node.AsConditionNode());
    else if (node.AsEnd())  Visit(*node.AsEnd());
    else if (node.AsFunctionCall())  Visit(*node.AsFunctionCall());
    else if (node.AsIdentifier())  Visit(*node.AsIdentifier());
    else if (node.AsList())  Visit(*node.AsList());
    else if (node.AsLiteral())  Visit(*node.AsLiteral());
    else if (node.AsUnaryOp())  Visit(*node.AsUnaryOp());
    else {
      std::cerr << "Unknown node type.\n";
      exit(1);
    }
  }

  // Return true to continue traversing.
  virtual bool ActOn(const AccessorNode &node) { return true; }
  virtual bool ActOn(const BinaryOpNode &node) { return true; }
  virtual bool ActOn(const BlockCommentNode &node) { return true; }
  virtual bool ActOn(const BlockNode &node) { return true; }
  virtual bool ActOn(const ConditionNode &node) { return true; }
  virtual bool ActOn(const EndNode &node) { return true; }
  virtual bool ActOn(const FunctionCallNode &node) { return true; }
  virtual bool ActOn(const IdentifierNode &node) { return true; }
  virtual bool ActOn(const ListNode &node) { return true; }
  virtual bool ActOn(const LiteralNode &node) { return true; }
  virtual bool ActOn(const UnaryOpNode &node) { return true; }

 private:
  void Visit(const AccessorNode &node) {
    if (ActOn(node)) {
      if (node.subscript())
        VisitNode(*node.subscript());
      else if (node.member())
        VisitNode(*node.member());
    }
  }

  void Visit(const BinaryOpNode &node) {
    if (ActOn(node)) {
      VisitNode(*node.left());
      VisitNode(*node.right());
    }
  }

  void Visit(const BlockCommentNode &node) {
    ActOn(node);
  }

  void Visit(const BlockNode &node) {
    if (ActOn(node)) {
      for (const auto& statement : node.statements())
        VisitNode(*statement);
      if (node.End() && node.End()->comments())
        VisitNode(*node.End());
    }
  }

  void Visit(const ConditionNode &node) {
    if (ActOn(node)) {
      VisitNode(*node.condition());
      VisitNode(*node.if_true());
      if (node.if_false())
        VisitNode(*node.if_false());
    }
  }

  void Visit(const EndNode &node) {
    ActOn(node);
  }

  void Visit(const FunctionCallNode &node) {
    if (ActOn(node)) {
      VisitNode(*node.args());
      if (node.block())
        VisitNode(*node.block());
    }
  }

  void Visit(const IdentifierNode &node) {
    ActOn(node);
  }

  void Visit(const ListNode &node) {
    if (ActOn(node)) {
      for (const auto &cur : node.contents()) {
        VisitNode(*cur);
      }
      if (node.End() && node.End()->comments())
        VisitNode(*node.End());
    }
  }

  void Visit(const LiteralNode &node) {
    ActOn(node);
  }

  void Visit(const UnaryOpNode &node) {
    if (ActOn(node)) {
      VisitNode(*node.operand());
    }
  }
};

class ConfigInserter : public ConstNodeVisitor {
 public:
  // source_file.cc -> [warnings]
  using WarningMap = std::unordered_map<std::string, std::vector<std::string>>;

  ConfigInserter(){}
  ConfigInserter(const WarningMap &warning_map) : warning_map_(warning_map) {}

  bool ActOn(const FunctionCallNode &node) override {
    // source_set("allocator") {
    //   public_configs = [
    //     ":magma_util_config",
    //     "$magma_build_root:magma_src_include_config",
    //   ]
    //
    //   sources = [
    //     "address_space_allocator.h",
    //     "retry_allocator.cc",
    //     "retry_allocator.h",
    //     "simple_allocator.cc",
    //     "simple_allocator.h",
    //   ]
    //
    //   public_deps = [
    //     ":common",
    //     "//zircon/public/lib/fit",
    //   ]
    // }
    const ListNode *args = node.args();
    if (!args)
      return true;

    const auto *block = node.block();
    if (!block)
      return true;

    // Search for sources list.
    for (const auto &stmt : block->statements()) {
      if (const ListNode *srcs = isSourcesListStmt(*stmt)) {
        for (const auto &src : srcs->contents()) {
          if (const LiteralNode *lit = src->AsLiteral()) {
            std::cout << lit->value().value() << "\n";
          }
        }
      }
    }
    return false;
  }

 private:
  // sources = [...]
  const ListNode *isSourcesListStmt(const ParseNode &node) {
    const BinaryOpNode *binop = node.AsBinaryOp();
    if (!binop)
      return nullptr;

    if (binop->op().type() != Token::EQUAL)
      return nullptr;

    const IdentifierNode *id = binop->left()->AsIdentifier();
    if (!id || id->value().value() != "sources")
      return nullptr;

    return binop->right()->AsList();
  }

  const WarningMap warning_map_;
};

}  // namespace

int main(int argc, char **argv) {
  assert(argc > 0 && "Expected at least one argument for the filename.");

  CommandLineArgs args;
  if (!ParseCmdLineArgs(argc, argv, args))
    return 1;

  SourceFile source_file;
  InputFile input_file(source_file);
  if (!SetInputFileContents(input_file, args.filename)) {
    std::cerr << "Could not read " << argv[1] << ".\n";
    return 1;
  }

  Err err;
  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, &err);
  if (err.has_error()) {
    err.PrintNonfatalToStdout();
    return 1;
  }

  std::unique_ptr<ParseNode> result = Parser::Parse(tokens, &err);
  if (!result) {
    err.PrintNonfatalToStdout();
    return 1;
  }

  if (args.dump_ast) {
    std::ostringstream collector;
    RenderToText(result->GetJSONNode(), 0, collector);
    std::cout << collector.str() << "\n";
    return 0;
  }

  std::unique_ptr<base::Value> json_args;
  if (!args.extra_args.empty()) {
    int error_code, error_line, error_column;
    std::string error_msg;
    json_args = base::JSONReader::ReadAndReturnError(args.extra_args,
                                               base::JSON_PARSE_RFC,
                                               /*error_code_out=*/nullptr,
                                               &error_msg,
                                               &error_line,
                                               &error_column);
    if (!json_args) {
      std::cerr << error_msg << "\n";
      return 1;
    }
  }

  ConfigInserter visitor;
  visitor.VisitNode(*result);

  return 0;
}

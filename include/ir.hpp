#ifndef IR_HPP
#define IR_HPP

#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cassert>
#include "parser.hpp"

class IRGenerator {
 public:
  IRGenerator();
  ~IRGenerator();

  std::string generate(const std::vector<std::unique_ptr<ASTNode>>& ast);
  std::string getCurrentIR();

  private:
  std::stringstream irStream;
  int tempCounter;
  int labelCounter;
  std::unordered_map<std::string, std::string> symbolTable; // 保留全局
  std::unordered_map<std::string, std::string> functionTable;
  std::unordered_map<std::string, std::string> constantTable;
  std::unordered_map<std::string, std::string> typeTable; // struct name -> LLVM type string
  std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> structFields; // struct name -> list of (field_name, type)
  std::unordered_map<std::string, std::string> varTypes; // 保留全局

  // Scope management
  std::vector<std::unordered_map<std::string, std::string>> symbolScopes;
  std::vector<std::unordered_map<std::string, std::string>> varTypeScopes;
  std::string currentRetType;
  bool inFunctionBody = false;
  std::string returnVar;
  std::string returnLabel;
  std::string currentLoopLabel;
  std::string currentBreakLabel;

  void preScan(const std::vector<std::unique_ptr<ASTNode>>& ast);
  void generateStructTypes();

  std::string toIRType(TypeNode* type);
  std::string toIRType(const std::string& typeName);

  std::string visit(ExpressionNode* node);
  std::string visit(LiteralExpressionNode* node);
  std::string visit(PathExpressionNode* node);
  std::string visit(CallExpressionNode* node);
  std::string visit(MethodCallExpressionNode* node);
  std::string visit(IndexExpressionNode* node);
  std::string visit(ArrayExpressionNode* node);
  std::string visit(ArithmeticOrLogicalExpressionNode* node);
  std::string visit(ComparisonExpressionNode* node);
  std::string visit(LazyBooleanExpressionNode* node);
  std::string visit(TypeCastExpressionNode* node);
  std::string visit(AssignmentExpressionNode* node);
  std::string visit(CompoundAssignmentExpressionNode* node);
  std::string visit(StructExpressionNode* node);
  std::string visit(DereferenceExpressionNode* node);
  std::string visit(NegationExpressionNode* node);
  std::string visit(BorrowExpressionNode* node);
  std::string visit(BlockExpressionNode* node);
  std::string visit(IfExpressionNode* node);
  std::string visit(PredicateLoopExpressionNode* node);
  std::string visit(ReturnExpressionNode* node);
  std::string visit(ContinueExpressionNode* node);
  std::string visit(FieldExpressionNode* node);
  std::string visit(GroupedExpressionNode* node);
  std::string visit(ExpressionWithoutBlockNode* node);
  std::string visit(OperatorExpressionNode* node);
  void visit_in_block(ExpressionWithoutBlockNode* node);

  void visit(ItemNode* node);
  void visit(FunctionNode* node);
  void visit(StructStructNode* node);
  void visit(TupleStructNode* node);
  void visit(EnumerationNode* node);
  void visit(ConstantItemNode* node);
  void visit(InherentImplNode* node);
  void visit(TraitImplNode* node);

  void visit(StatementNode* node);
  void visit(LetStatement* node);
  void visit(ExpressionStatement* node);

  std::string createTemp();
  std::string createLabel();
  std::string emitAlloca(const std::string& type, const std::string& name = "");
  std::string emitLoad(const std::string& type, const std::string& ptr);
  std::string emitStore(const std::string& value, const std::string& ptr);
  std::string emitBinaryOp(const std::string& op, const std::string& lhs, const std::string& rhs, const std::string& type);

  std::string getLhsAddress(ExpressionNode* lhs);
  std::string getLhsType(ExpressionNode* lhs);

  std::optional<int> evaluateConstant(ExpressionNode* expr);
  std::string getElementType(const std::string& typeStr);

  void error(const std::string& msg);

  void enterScope();
  void exitScope();
  std::string lookupSymbol(const std::string& name);
  std::string lookupVarType(const std::string& name);
};

IRGenerator::IRGenerator() : tempCounter(0), labelCounter(0) {
  irStream << "; ModuleID = 'generated.ll'\n";
  irStream << "source_filename = \"generated.ll\"\n";
  irStream << "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n";
  irStream << "target triple = \"x86_64-unknown-linux-gnu\"\n\n";
  // Initialize global scope
  symbolScopes.push_back({});
  varTypeScopes.push_back({});
}

IRGenerator::~IRGenerator() = default;

std::string IRGenerator::generate(const std::vector<std::unique_ptr<ASTNode>>& ast) {
  // 第一步：预扫描全局声明
  preScan(ast);

  //irStream << "; begin generating IR of struct\n";
  // 第二步：先生成所有 struct 的 IR
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (dynamic_cast<StructStructNode*>(item) || dynamic_cast<TupleStructNode*>(item)) {
        visit(item);
      }
    }
  }

  //irStream << "; finish generating IR of struct\n";

  // 添加内置函数定义
  irStream << "@.str = private unnamed_addr constant [3 x i8] c\"%s\\00\", align 1\n";
  irStream << "@.str.1 = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\", align 1\n";
  irStream << "@.str.2 = private unnamed_addr constant [3 x i8] c\"%d\\00\", align 1\n";
  irStream << "@.str.3 = private unnamed_addr constant [4 x i8] c\"%d\\0A\\00\", align 1\n";
  irStream << "\n";
  irStream << "define dso_local void @print(i8* noundef %0) {\n";
  irStream << "  %2 = alloca i8*, align 8\n";
  irStream << "  store i8* %0, i8** %2, align 8\n";
  irStream << "  %3 = load i8*, i8** %2, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @println(i8* noundef %0) {\n";
  irStream << "  %2 = alloca i8*, align 8\n";
  irStream << "  store i8* %0, i8** %2, align 8\n";
  irStream << "  %3 = load i8*, i8** %2, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @printInt(i32 noundef %0) {\n";
  irStream << "  %2 = alloca i32, align 4\n";
  irStream << "  store i32 %0, i32* %2, align 4\n";
  irStream << "  %3 = load i32, i32* %2, align 4\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0), i32 noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @printlnInt(i32 noundef %0) {\n";
  irStream << "  %2 = alloca i32, align 4\n";
  irStream << "  store i32 %0, i32* %2, align 4\n";
  irStream << "  %3 = load i32, i32* %2, align 4\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([4 x i8], [4 x i8]* @.str.3, i64 0, i64 0), i32 noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @getString() {\n";
  irStream << "  %1 = alloca i8*, align 8\n";
  irStream << "  %2 = call i8* @malloc(i32 noundef 256)\n";
  irStream << "  store i8* %2, i8** %1, align 8\n";
  irStream << "  %3 = load i8*, i8** %1, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @scanf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  %5 = load i8*, i8** %1, align 8\n";
  irStream << "  ret i8* %5\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i32 @getInt() {\n";
  irStream << "  %1 = alloca i32, align 4\n";
  irStream << "  %2 = call i32 (i8*, ...) @scanf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0), i32* noundef %1)\n";
  irStream << "  %3 = load i32, i32* %1, align 4\n";
  irStream << "  ret i32 %3\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @builtin_memset(i8* noundef %0, i32 noundef %1, i32 noundef %2) {\n";
  irStream << "  %4 = alloca i8*, align 8\n";
  irStream << "  %5 = alloca i32, align 4\n";
  irStream << "  %6 = alloca i32, align 4\n";
  irStream << "  store i8* %0, i8** %4, align 8\n";
  irStream << "  store i32 %1, i32* %5, align 4\n";
  irStream << "  store i32 %2, i32* %6, align 4\n";
  irStream << "  %7 = load i8*, i8** %4, align 8\n";
  irStream << "  %8 = load i32, i32* %5, align 4\n";
  irStream << "  %9 = load i32, i32* %6, align 4\n";
  irStream << "  %10 = call i8* @memset(i8* noundef %7, i32 noundef %8, i32 noundef %9)\n";
  irStream << "  ret i8* %10\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @builtin_memcpy(i8* noundef %0, i8* noundef %1, i32 noundef %2) {\n";
  irStream << "  %4 = alloca i8*, align 8\n";
  irStream << "  %5 = alloca i8*, align 8\n";
  irStream << "  %6 = alloca i32, align 4\n";
  irStream << "  store i8* %0, i8** %4, align 8\n";
  irStream << "  store i8* %1, i8** %5, align 8\n";
  irStream << "  store i32 %2, i32* %6, align 4\n";
  irStream << "  %7 = load i8*, i8** %4, align 8\n";
  irStream << "  %8 = load i8*, i8** %5, align 8\n";
  irStream << "  %9 = load i32, i32* %6, align 4\n";
  irStream << "  %10 = call i8* @memcpy(i8* noundef %7, i8* noundef %8, i32 noundef %9)\n";
  irStream << "  ret i8* %10\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "declare i32 @scanf(i8*, ...)\n";
  irStream << "declare i32 @printf(i8*, ...)\n";
  irStream << "declare i8* @malloc(i32 noundef)\n";
  irStream << "declare i8* @memset(i8* noundef, i32 noundef, i32 noundef)\n";
  irStream << "declare i8* @memcpy(i8* noundef, i8* noundef, i32 noundef)\n\n";

  // 第三步：生成其他代码（函数等）
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (!dynamic_cast<StructStructNode*>(item) && !dynamic_cast<TupleStructNode*>(item) && !dynamic_cast<ConstantItemNode*>(item)) {
        visit(item);
      }
    } else if (auto* stmt = dynamic_cast<StatementNode*>(node.get())) {
      visit(stmt);
    } else if (auto* expr = dynamic_cast<ExpressionNode*>(node.get())) {
      visit(expr);
    } else {
      error("Unknown AST node type");
    }
  }
  return irStream.str();
}

std::string IRGenerator::toIRType(TypeNode* type) {
  //irStream << "; getting irtype of typenode: " << type->toString() << '\n';
  if (!type) return "void";

  switch (type->node_type) {
    case TypeType::TypePath_node: {
      auto* typePathNode = dynamic_cast<TypePathNode*>(type);
      if (typePathNode && typePathNode->type_path) {
        std::string path = typePathNode->type_path->toString();
        if (typeTable.find(path) != typeTable.end()) {
          return typeTable[path];
        }
        if (path == "i32") return "i32";
        if (path == "i64") return "i64";
        if (path == "bool") return "i1";
      }
      break;
    }
    case TypeType::ReferenceType_node: {
      //irStream << "; the type is reference type\n";
      auto* refType = dynamic_cast<ReferenceTypeNode*>(type);
      if (refType) {
        return toIRType(refType->type.get()) + "*";
      }
      break;
    }
    case TypeType::ArrayType_node: {
      //irStream << "; the type is array type\n";
      auto* arrayType = dynamic_cast<ArrayTypeNode*>(type);
      if (arrayType) {
          std::string innerType = toIRType(arrayType->type.get());
          //irStream << "; the inner type of array type: " << innerType << '\n';
          auto numOpt = evaluateConstant(arrayType->expression.get());
          if (numOpt) {
            return "[" + std::to_string(*numOpt) + " x " + innerType + "]";
          } else {
            error("Array size must be constant expression");
          }
      }
      break;
    }
  }
  return "i32";
}

std::string IRGenerator::toIRType(const std::string& typeName) {
  if (typeName == "i32") return "i32";
  if (typeName == "i64") return "i64";
  if (typeName == "bool") return "i1";
  return "i32";
}

std::string IRGenerator::visit(ExpressionNode* node) {
  if (auto* lit = dynamic_cast<LiteralExpressionNode*>(node)) {
    return visit(lit);
  } else if (auto* path = dynamic_cast<PathExpressionNode*>(node)) {
    return visit(path);
  } else if (auto* call = dynamic_cast<CallExpressionNode*>(node)) {
    return visit(call);
  } else if (auto* arith = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(node)) {
    return visit(arith);
  } else if (auto* comp = dynamic_cast<ComparisonExpressionNode*>(node)) {
    return visit(comp);
  } else if (auto* assign = dynamic_cast<AssignmentExpressionNode*>(node)) {
    return visit(assign);
  } else if (auto* compAssign = dynamic_cast<CompoundAssignmentExpressionNode*>(node)) {
    return visit(compAssign);
  } else if (auto* struct_ = dynamic_cast<StructExpressionNode*>(node)) {
    return visit(struct_);
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(node)) {
    return visit(deref);
  } else if (auto* block = dynamic_cast<BlockExpressionNode*>(node)) {
    return visit(block);
  } else if (auto* if_ = dynamic_cast<IfExpressionNode*>(node)) {
    return visit(if_);
  } else if (auto* ret = dynamic_cast<ReturnExpressionNode*>(node)) {
    return visit(ret);
  } else if (auto* group = dynamic_cast<GroupedExpressionNode*>(node)) {
    return visit(group);
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(node)) {
    return visit(field);
  } else if (auto* lazy_bool = dynamic_cast<LazyBooleanExpressionNode*>(node)) {
    return visit(lazy_bool);
  } else if (auto* method_call = dynamic_cast<MethodCallExpressionNode*>(node)) {
    return visit(method_call);
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(node)) {
    return visit(index);
  } else if (auto* loop = dynamic_cast<PredicateLoopExpressionNode*>(node)) {
    return visit(loop);
  } else if (auto* cont = dynamic_cast<ContinueExpressionNode*>(node)) {
    return visit(cont);
  } else if (auto* opExpr = dynamic_cast<OperatorExpressionNode*>(node)) {
    return visit(opExpr);
  } else if (auto* type_cast = dynamic_cast<TypeCastExpressionNode*>(node)) {
    return visit(type_cast);
  } else if (auto* array = dynamic_cast<ArrayExpressionNode*>(node)) {
    return visit(array);
  } else if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(node)) {
    return visit(borrow);
  } else if (auto* neg = dynamic_cast<NegationExpressionNode*>(node)) {
    return visit(neg);
  }
  error(std::string("Unsupported expression type: ") + typeid(*node).name());
  return "";
}

std::string IRGenerator::visit(LiteralExpressionNode* node) {
  if (std::holds_alternative<std::unique_ptr<string_literal>>(node->literal)) {
    auto& strLit = std::get<std::unique_ptr<string_literal>>(node->literal);
    std::string strValue = strLit->value;
    // 转义字符串，添加 null terminator
    std::string escaped = strValue + "\\00";
    int len = strValue.size() + 1;
    std::string globalName = ".str." + std::to_string(tempCounter++);
    irStream << "@" << globalName << " = private unnamed_addr constant [" << len << " x i8] c\"" << escaped << "\", align 1\n";
    std::string temp = createTemp();
    irStream << "  %" << temp << " = getelementptr inbounds [" << len << " x i8], [" << len << " x i8]* @" << globalName << ", i32 0, i32 0\n";
    return "%" + temp;
  } else {
    // 整数或其他
    std::string temp = createTemp();
    std::string type = "i32";
    std::string value = node->toString();
    irStream << "  %" << temp << " = add i32 0, " << value << "\n";
    return "%" + temp;
  }
}

std::string IRGenerator::visit(PathExpressionNode* node) {
  std::string name = node->toString();
  if (constantTable.find(name) != constantTable.end()) {
    // 常量，直接返回值
    return constantTable[name];
  } else {
    std::string temp = lookupSymbol(name);
    if (!temp.empty()) {
      std::string type = lookupVarType(name);
      if (type.empty()) type = "i32";
      if (type.find('%') == 0 && type.back() == '*') {
        // 结构体指针，load 结构体
        std::string loadTemp = createTemp();
        std::string structType = type.substr(0, type.size() - 1);
        irStream << "  %" << loadTemp << " = load " << structType << ", " << type << " %" << temp << "\n";
        return "%" + loadTemp;
      } else if (type.find('*') != std::string::npos) {
        // 其他指针，直接返回指针
        return "%" + temp;
      } else {
        // 基本类型，load
        std::string loadTemp = createTemp();
        irStream << "  %" << loadTemp << " = load " << type << ", " << type << "* %" << temp << "\n";
        return "%" + loadTemp;
      }
    }
  }
  error("Undefined variable: " + name);
  return "";
}

std::string IRGenerator::visit(CallExpressionNode* node) {
    std::string funcName;
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
        funcName = path->toString();
    } else {
        error("Unsupported function expression in call");
        return "";
    }
    std::string args = "";
    if (node->call_params) {
        for (size_t i = 0; i < node->call_params->expressions.size(); ++i) {
            if (i > 0) args += ", ";
            std::string argValue = visit(node->call_params->expressions[i].get());
            std::string argType = "i32"; // 简化，假设都是 i32
            args += argType + " " + argValue;
        }
    }
    std::string retType = functionTable[funcName];
    if (retType.empty()) retType = "i32"; // 默认
    if (retType == "void") {
        irStream << "  call void @" << funcName << "(" << args << ")\n";
        return "";
    } else {
        std::string temp = createTemp();
        irStream << "  %" << temp << " = call " << retType << " @" << funcName << "(" << args << ")\n";
        return "%" + temp;
    }
}

std::string IRGenerator::visit(ArithmeticOrLogicalExpressionNode* node) {
  std::string lhs = visit(node->expression1.get());
  std::string rhs = visit(node->expression2.get());
  std::string temp = createTemp();
  std::string op;
  switch (node->type) {
    case ADD: op = "add"; break;
    case MINUS: op = "sub"; break;
    case MUL: op = "mul"; break;
    case DIV: op = "sdiv"; break;
    default: op = "add";
  }
  irStream << "  %" << temp << " = " << op << " i32 " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(ComparisonExpressionNode* node) {
  std::string lhs = visit(node->expression1.get());
  std::string rhs = visit(node->expression2.get());
  std::string temp = createTemp();
  std::string op;
  switch (node->type) {
    case EQ: op = "icmp eq"; break;
    case NEQ: op = "icmp ne"; break;
    case GT: op = "icmp sgt"; break;
    // 添加更多
    default: op = "icmp eq";
  }
  irStream << "  %" << temp << " = " << op << " i32 " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(AssignmentExpressionNode* node) {
  // 获取 rhs 值
  std::string rhsValue = visit(node->expression2.get());
  // 获取 lhs 地址和类型
  std::string lhsAddr = getLhsAddress(node->expression1.get());
  std::string lhsType = getLhsType(node->expression1.get());
  if (lhsAddr.empty() || lhsType.empty()) {
    return "";
  }
  if (lhsType.find('%') == 0 && lhsType.back() == '*') {
    // 结构体指针，store 结构体值
    std::string structType = lhsType.substr(0, lhsType.size() - 1);
    irStream << "  store " << structType << " " << rhsValue << ", " << lhsType << " " << lhsAddr << "\n";
  } else {
    // 基本类型
    irStream << "  store " << lhsType << " " << rhsValue << ", " << lhsType << "* " << lhsAddr << "\n";
  }
  return "";
}

std::string IRGenerator::visit(CompoundAssignmentExpressionNode* node) {
  // 获取 lhs 地址和类型
  std::string lhsAddr = getLhsAddress(node->expression1.get());
  std::string lhsType = getLhsType(node->expression1.get());
  if (lhsAddr.empty() || lhsType.empty()) {
    return "";
  }

  // load lhs 值
  std::string lhsValue;
  if (lhsType.find('%') == 0 && lhsType.back() == '*') {
    // 结构体指针，load 结构体
    std::string structType = lhsType.substr(0, lhsType.size() - 1);
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << structType << ", " << lhsType << " " << lhsAddr << "\n";
    lhsValue = "%" + loadTemp;
  } else {
    // 基本类型，load
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << lhsType << ", " << lhsType << "* " << lhsAddr << "\n";
    lhsValue = "%" + loadTemp;
  }

  // 获取 rhs 值
  std::string rhsValue = visit(node->expression2.get());

  // 计算 lhs op rhs
  std::string op;
  switch (node->type) {
    case ADD: op = "add"; break;
    case MINUS: op = "sub"; break;
    case MUL: op = "mul"; break;
    case DIV: op = "sdiv"; break;
    case MOD: op = "srem"; break;
    case AND: op = "and"; break;
    case OR: op = "or"; break;
    case XOR: op = "xor"; break;
    case SHL: op = "shl"; break;
    case SHR: op = "ashr"; break;
    default: op = "add";
  }
  std::string resultTemp = createTemp();
  irStream << "  %" << resultTemp << " = " << op << " " << lhsType << " " << lhsValue << ", " << rhsValue << "\n";

  // store 结果到 lhs
  if (lhsType.find('%') == 0 && lhsType.back() == '*') {
    // 结构体指针，store 结构体值
    std::string structType = lhsType.substr(0, lhsType.size() - 1);
    irStream << "  store " << structType << " %" << resultTemp << ", " << lhsType << " " << lhsAddr << "\n";
  } else {
    // 基本类型
    irStream << "  store " << lhsType << " %" << resultTemp << ", " << lhsType << "* " << lhsAddr << "\n";
  }

  return "";
}

std::string IRGenerator::visit(StructExpressionNode* node) {
  std::string structName = node->pathin_expression->toString();
  auto it = structFields.find(structName);
  if (it == structFields.end()) {
    error("Unknown struct " + structName);
    return "";
  }
  std::string structType = "%" + structName;

  // 初始值
  std::string currentValue;
  if (node->struct_base) {
    currentValue = visit(node->struct_base->expression.get());
  } else {
    currentValue = "undef";
  }

  // 为每个字段生成 insertvalue
  if (node->struct_expr_fields) {
    for (const auto& field : node->struct_expr_fields->struct_expr_fields) {
      std::string fieldName;
      if (std::holds_alternative<Identifier>(field->id_or_tupe_index)) {
        fieldName = std::get<Identifier>(field->id_or_tupe_index).id;
      } else {
        // tuple index, 暂时不支持
        error("Tuple index in struct expression not supported");
        return "";
      }
      // 找到索引
      int index = -1;
      for (int i = 0; i < it->second.size(); ++i) {
        if (it->second[i].first == fieldName) {
          index = i;
          break;
        }
      }
      if (index == -1) {
        error("Field not found: " + fieldName);
        return "";
      }
      std::string fieldValue = visit(field->expression.get());
      std::string temp = createTemp();
      irStream << "  %" << temp << " = insertvalue " << structType << " " << currentValue << ", " << it->second[index].second << " " << fieldValue << ", " << index << "\n";
      currentValue = "%" + temp;
    }
  }

  return currentValue;
}

std::string IRGenerator::visit(DereferenceExpressionNode* node) {
  std::string ptr = visit(node->expression.get());
  std::string ptrType = getLhsType(node->expression.get());
  std::string valueType;
  if (ptrType.back() == '*') {
    valueType = ptrType.substr(0, ptrType.size() - 1);
  } else {
    error("Dereference on non-pointer type");
    return "";
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << valueType << ", " << ptrType << " " << ptr << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(NegationExpressionNode* node) {
  std::string expr = visit(node->expression.get());
  std::string temp = createTemp();
  if (node->type == NegationExpressionNode::MINUS) {
    irStream << "  %" << temp << " = sub i32 0, " << expr << "\n";
  } else if (node->type == NegationExpressionNode::BANG) {
    irStream << "  %" << temp << " = xor i32 " << expr << ", -1\n";
  } else {
    error("Unknown negation type");
    return "";
  }
  return "%" + temp;
}

std::string IRGenerator::visit(BorrowExpressionNode* node) {
  std::string addr = getLhsAddress(node->expression.get());
  if (!addr.empty()) {
    return addr;
  } else {
    // 计算值，alloca 临时，store，返回指针
    std::string value = visit(node->expression.get());
    std::string type = getLhsType(node->expression.get());
    if (type.empty()) type = "i32"; // 默认
    std::string tempPtr = createTemp();
    irStream << "  %" << tempPtr << " = alloca " << type << "\n";
    irStream << "  store " << type << " " << value << ", " << type << "* %" << tempPtr << "\n";
    return "%" + tempPtr;
  }
}

std::string IRGenerator::visit(MethodCallExpressionNode* node) {
  std::string methodName;
  if (std::holds_alternative<Identifier>(node->path_expr_segment)) {
    methodName = std::get<Identifier>(node->path_expr_segment).id;
  } else {
    error("PathInType in method call not supported");
    return "";
  }
  std::string self = visit(node->expression.get());
  std::string args = self;
  if (node->call_params) {
    for (auto& arg : node->call_params->expressions) {
      args += ", " + visit(arg.get());
    }
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = call i32 @" << methodName << "(" << args << ")\n";
  return "%" + temp;
}

std::string IRGenerator::visit(IndexExpressionNode* node) {
  std::string addr = getLhsAddress(node);
  std::string type = getLhsType(node);
  if (addr.empty() || type.empty()) {
    return "";
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << type << ", " << type << "* " << addr << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(PredicateLoopExpressionNode* node) {
  std::string loopLabel = createLabel();
  std::string bodyLabel = createLabel();
  std::string endLabel = createLabel();

  std::string oldLoopLabel = currentLoopLabel;
  std::string oldBreakLabel = currentBreakLabel;
  currentLoopLabel = loopLabel;
  currentBreakLabel = endLabel;

  irStream << loopLabel << ":\n";
  std::string cond;
  if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(node->conditions->condition)) {
    cond = visit(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get());
  } else {
    error("Unsupported condition type in while");
    return "";
  }
  irStream << "  br i1 " << cond << ", label %" << bodyLabel << ", label %" << endLabel << "\n";

  irStream << bodyLabel << ":\n";
  visit(node->block_expression.get());
  irStream << "  br label %" << loopLabel << "\n";

  irStream << endLabel << ":\n";

  currentLoopLabel = oldLoopLabel;
  currentBreakLabel = oldBreakLabel;
  return "";
}

std::string IRGenerator::visit(ContinueExpressionNode* node) {
  if (currentLoopLabel.empty()) {
    error("continue outside of loop");
    return "";
  }
  irStream << "  br label %" << currentLoopLabel << "\n";
  return "";
}

std::string IRGenerator::visit(TypeCastExpressionNode* node) {
  // For now, just return the expression value, assuming no actual cast needed
  return visit(node->expression.get());
}

std::string IRGenerator::visit(ArrayExpressionNode* node) {
  if (node->if_empty) {
    error("Empty array expression not supported");
    return "";
  }
  // Assume element type i32 for now
  std::string elementType = "i32";
  int size;
  if (node->type == ArrayExpressionType::REPEAT) {
    auto countOpt = evaluateConstant(node->expressions[1].get());
    if (!countOpt) {
      error("Array repeat count must be constant");
      return "";
    }
    size = *countOpt;
    std::string value = visit(node->expressions[0].get());
    std::string temp = createTemp();
    irStream << "  %" << temp << " = insertvalue [" << size << " x " << elementType << "] undef, " << elementType << " " << value << ", 0\n";
    for (int i = 1; i < size; ++i) {
      std::string nextTemp = createTemp();
      irStream << "  %" << nextTemp << " = insertvalue [" << size << " x " << elementType << "] %" << temp << ", " << elementType << " " << value << ", " << i << "\n";
      temp = nextTemp;
    }
    return "%" + temp;
  } else {
    size = node->expressions.size();
    std::string temp = createTemp();
    irStream << "  %" << temp << " = insertvalue [" << size << " x " << elementType << "] undef, " << elementType << " " << visit(node->expressions[0].get()) << ", 0\n";
    for (int i = 1; i < size; ++i) {
      std::string nextTemp = createTemp();
      irStream << "  %" << nextTemp << " = insertvalue [" << size << " x " << elementType << "] %" << temp << ", " << elementType << " " << visit(node->expressions[i].get()) << ", " << i << "\n";
      temp = nextTemp;
    }
    return "%" + temp;
  }
}

std::string IRGenerator::visit(LazyBooleanExpressionNode* node) {
  std::string lhs = visit(node->expression1.get());
  std::string temp = createTemp();
  std::string trueLabel = createLabel();
  std::string falseLabel = createLabel();
  std::string endLabel = createLabel();

  if (node->type == LAZY_AND) {
    // br i1 lhs, label trueLabel, label falseLabel
    irStream << "  br i1 " << lhs << ", label %" << trueLabel << ", label %" << falseLabel << "\n";

    // trueLabel: rhs
    irStream << trueLabel << ":\n";
    std::string rhs = visit(node->expression2.get());
    irStream << "  br label %" << endLabel << "\n";

    // falseLabel: 0
    irStream << falseLabel << ":\n";
    irStream << "  br label %" << endLabel << "\n";

    // end: phi
    irStream << endLabel << ":\n";
    irStream << "  %" << temp << " = phi i1 [ " << rhs << ", %" << trueLabel << " ], [ 0, %" << falseLabel << " ]\n";
  } else { // LAZY_OR
    // br i1 lhs, label trueLabel, label falseLabel
    irStream << "  br i1 " << lhs << ", label %" << trueLabel << ", label %" << falseLabel << "\n";

    // trueLabel: 1
    irStream << trueLabel << ":\n";
    irStream << "  br label %" << endLabel << "\n";

    // falseLabel: rhs
    irStream << falseLabel << ":\n";
    std::string rhs = visit(node->expression2.get());
    irStream << "  br label %" << endLabel << "\n";

    // end: phi
    irStream << endLabel << ":\n";
    irStream << "  %" << temp << " = phi i1 [ 1, %" << trueLabel << " ], [ " << rhs << ", %" << falseLabel << " ]\n";
  }

  return "%" + temp;
}

std::string IRGenerator::visit(BlockExpressionNode* node) {
  enterScope();
  for (const auto& stmt : node->statement) {
    visit(stmt.get());
  }
  std::string result = "";
  if (node->expression_without_block) {
    if (inFunctionBody) {
      visit_in_block(node->expression_without_block.get());
    } else {
      result = visit(node->expression_without_block.get());
    }
  }
  return result;
  exitScope();
  return result;
}

std::string IRGenerator::visit(ReturnExpressionNode* node) {
  if (node->expression) {
    std::string value = visit(node->expression.get());
    irStream << "  ret " << currentRetType << " " << value << "\n";
  } else {
    irStream << "  ret void\n";
  }
  return "";
}

std::string IRGenerator::visit(GroupedExpressionNode* node) {
  return visit(node->expression.get());
}

std::string IRGenerator::visit(ExpressionWithoutBlockNode* node) {
  return std::visit([&](auto&& arg) -> std::string {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<OperatorExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<MethodCallExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ContinueExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
      return visit(arg.get());
    } else {
      error("Unsupported ExpressionWithoutBlock type");
      return "";
    }
  }, node->expr);
}

std::string IRGenerator::visit(OperatorExpressionNode* node) {
  return std::visit([&](auto&& arg) -> std::string {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ComparisonExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<AssignmentExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<CompoundAssignmentExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<BorrowExpressionNode>>) {
      return visit(arg.get());
    } else {
      error("Unsupported OperatorExpression type");
      return "";
    }
  }, node->operator_expression);
}

void IRGenerator::visit_in_block(ExpressionWithoutBlockNode* node) {
  std::string value = visit(node);
  irStream << "  store " << currentRetType << " " << value << ", " << currentRetType << "* %" << returnVar << "\n";
  irStream << "  br label %" << returnLabel << "\n";
}

std::string IRGenerator::visit(FieldExpressionNode* node) {
  std::string addr = getLhsAddress(node);
  std::string type = getLhsType(node);
  if (addr.empty() || type.empty()) {
    return "";
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << type << ", " << type << "* " << addr << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(IfExpressionNode* node) {
  // 处理 conditions
  //irStream << "; begin getting ir of ifexpression\n";
  std::string cond;
  if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(node->conditions->condition)) {
    const auto& expr = std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition);
    cond = visit(expr.get());
  } else {
    // 其他条件类型，暂时不支持
    error("Unsupported condition type in if");
    return "";
  }

  std::string thenLabel = createLabel();
  std::string elseLabel = createLabel();

  irStream << "  br i1 " << cond << ", label %" << thenLabel << ", label %" << elseLabel << "\n";

  if (inFunctionBody) {
    // 对于函数体，直接 br 到 returnLabel
    irStream << thenLabel << ":\n";
    //irStream << "  ; then branch\n";
    visit(node->block_expression.get());

    irStream << elseLabel << ":\n";
    //irStream << "  ; else branch\n";
    if (node->else_block) {
      visit(node->else_block.get());
    } else if (node->else_if) {
      visit(node->else_if.get());
    }

    return "";
  } else {
    // 对于表达式，生成 phi
    std::string endLabel = createLabel();

    irStream << thenLabel << ":\n";
    irStream << "  ; then branch\n";
    std::string thenValue = visit(node->block_expression.get());
    irStream << "  br label %" << endLabel << "\n";

    irStream << elseLabel << ":\n";
    irStream << "  ; else branch\n";
    std::string elseValue;
    if (node->else_block) {
      elseValue = visit(node->else_block.get());
    } else if (node->else_if) {
      elseValue = visit(node->else_if.get());
    }
    irStream << "  br label %" << endLabel << "\n";

    irStream << endLabel << ":\n";
    if (!thenValue.empty() && !elseValue.empty()) {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = phi " << currentRetType << " [ " << thenValue << ", %" << thenLabel << " ], [ " << elseValue << ", %" << elseLabel << " ]\n";
      return "%" + temp;
    }
    return "";
  }
}

void IRGenerator::visit(ItemNode* node) {
    if (auto* func = dynamic_cast<FunctionNode*>(node)) {
        visit(func);
    } else if (auto* struct_ = dynamic_cast<StructStructNode*>(node)) {
        visit(struct_);
    } else if (auto* tuple = dynamic_cast<TupleStructNode*>(node)) {
        visit(tuple);
    } else if (auto* enum_ = dynamic_cast<EnumerationNode*>(node)) {
        visit(enum_);
    } else if (auto* const_ = dynamic_cast<ConstantItemNode*>(node)) {
        visit(const_);
    } else if (auto* impl = dynamic_cast<InherentImplNode*>(node)) {
        visit(impl);
    } else if (auto* traitImpl = dynamic_cast<TraitImplNode*>(node)) {
        visit(traitImpl);
    }
    // 添加其他Item类型
}

void IRGenerator::visit(FunctionNode* node) {
    std::string funcName = node->identifier;
    std::string retType = node->return_type ? toIRType(node->return_type->type.get()) : "void";
    currentRetType = retType;

    // 函数签名
    std::string params = "";
    std::vector<std::pair<std::string, std::string>> paramList; // paramType, paramName

    if (node->function_parameter) {
        // 处理 self_param
        if (node->function_parameter->self_param) {
            std::string selfType;
            if (node->impl_type_name) {
                selfType = "%" + *node->impl_type_name + "*";
            } else {
                selfType = "i8*"; // 默认
            }
            std::string selfName = "self";
            paramList.emplace_back(selfType, selfName);
        }

        // 处理 function_params
        for (size_t i = 0; i < node->function_parameter->function_params.size(); ++i) {
            const auto& param = node->function_parameter->function_params[i];
            std::string paramType;
            std::string paramName;
            if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(param->info)) {
                const auto& fpp = std::get<std::unique_ptr<FunctionParamPattern>>(param->info);
                paramType = fpp->type ? toIRType(fpp->type.get()) : "i32";
                //irStream << "； the " << i << "th param type in function node: " << paramType << '\n';
                if (!paramType.empty() && paramType[0] == '%') paramType += "*";
                paramName = fpp->pattern ? fpp->pattern->toString() : "arg" + std::to_string(i);
            } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(param->info)) {
                const auto& typeNode = std::get<std::unique_ptr<TypeNode>>(param->info);
                paramType = toIRType(typeNode.get());
                if (!paramType.empty() && paramType[0] == '%') paramType += "*";
                paramName = "arg" + std::to_string(i);
            } else if (std::holds_alternative<std::unique_ptr<ellipsis>>(param->info)) {
                // 跳过 ellipsis
                continue;
            }
            paramList.emplace_back(paramType, paramName);
            //irStream << "； the " << i << "th param type: " << paramType << '\n';
        }
    }

    for (size_t i = 0; i < paramList.size(); ++i) {
        params += paramList[i].first + " %" + paramList[i].second;
        if (i + 1 < paramList.size()) params += ", ";
    }

    irStream << "; Function: " << funcName << "\n";
    irStream << "define " << retType << " @" << funcName << "(" << params << ") {\n";

    if (retType != "void") {
        returnVar = createTemp();
        irStream << "  %" << returnVar << " = alloca " << retType << "\n";
        returnLabel = createLabel();
    }

    // 局部变量：参数处理
    size_t index = 0;
    for (const auto& p : paramList) {
        //irStream << "; processing the " << index << "th parameter of function\n";
        index++;
        std::string paramType = p.first;
        std::string paramName = p.second;
        // 对于指针类型，直接使用参数名作为符号
        if (paramType.find('*') != std::string::npos) {
            symbolScopes.back()[paramName] = paramName;
            varTypeScopes.back()[paramName] = paramType;
        } else {
            // 对于基本类型，alloca
            std::string temp = createTemp();
            symbolScopes.back()[paramName] = temp;
            varTypeScopes.back()[paramName] = paramType;
            irStream << "  %" << temp << " = alloca " << paramType << "\n";
            irStream << "  store " << paramType << " %" << paramName << ", " << paramType << "* %" << temp << "\n";
        }
    }

    // basic blocks 和 expression visitor
    enterScope();  // 函数作用域
    inFunctionBody = true;
    if (node->block_expression) {
        visit(node->block_expression.get());  // 生成代码，包括 store 和 br
    }
    inFunctionBody = false;
    exitScope();

    // 生成统一的 return block
    if (retType != "void") {
        irStream << returnLabel << ":\n";
        std::string val = createTemp();
        irStream << "  %" << val << " = load " << retType << ", " << retType << "* %" << returnVar << "\n";
        irStream << "  ret " << retType << " %" << val << "\n";
    } else {
        irStream << "  ret void\n";
    }

    irStream << "}\n\n";
}

void IRGenerator::visit(StructStructNode* node) {
  irStream << "%" << node->identifier << " = type { ";
  if (node->struct_fields) {
    for (size_t i = 0; i < node->struct_fields->struct_fields.size(); ++i) {
      const auto& field = node->struct_fields->struct_fields[i];
      std::string fieldType = toIRType(field->type.get());
      irStream << fieldType;
      if (i + 1 < node->struct_fields->struct_fields.size()) irStream << ", ";
    }
  }
  irStream << " }\n\n";
}

void IRGenerator::visit(TupleStructNode* node) {
  irStream << "%" << node->identifier << " = type { ";
  if (node->tuple_fields) {
    for (const auto& field : node->tuple_fields->tuple_fields) {
      std::string typeStr = toIRType(field->type.get());
      irStream << typeStr << ", ";
    }
  }
  irStream << "}\n\n";
}

void IRGenerator::visit(EnumerationNode* node) {
  irStream << "; Enum " << node->identifier << "\n";
}

void IRGenerator::visit(ConstantItemNode* node) {
  if (node->identifier && node->expression) {
    std::string value;
    if (auto* lit = dynamic_cast<LiteralExpressionNode*>(node->expression.get())) {
      value = lit->toString();
    }
    constantTable[*node->identifier] = value;
  }
}

void IRGenerator::visit(InherentImplNode* node) {
  //irStream << "; Inherent impl for " << node->type->toString() << "\n";
  for (auto& assoc : node->associated_item) {
    std::visit([&](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<ConstantItemNode>>) {
        visit(arg.get());
      } else if constexpr (std::is_same_v<T, std::unique_ptr<FunctionNode>>) {
        visit(arg.get());
      }
    }, assoc->associated_item);
  }
}

void IRGenerator::visit(TraitImplNode* node) {
  irStream << "; Trait impl\n";
}

void IRGenerator::visit(StatementNode* node) {
  if (node->type == LETSTATEMENT && node->let_statement) {
    visit(node->let_statement.get());
  } else if (node->type == EXPRESSIONSTATEMENT && node->expr_statement) {
    visit(node->expr_statement.get());
  }
}

void IRGenerator::visit(LetStatement* node) {
  std::string varName = node->pattern->toString(); // 简化
  std::string type = node->type ? toIRType(node->type.get()) : "i32";
  std::string temp = createTemp();
  symbolScopes.back()[varName] = temp;
  varTypeScopes.back()[varName] = type;
  irStream << "  %" << temp << " = alloca " << type << "\n";
  if (node->expression) {
    std::string value = visit(node->expression.get());
    irStream << "  store " << type << " " << value << ", " << type << "* %" << temp << "\n";
  }
}

void IRGenerator::visit(ExpressionStatement* node) {
  visit(node->expression.get());
}

std::string IRGenerator::createTemp() {
  return "t" + std::to_string(tempCounter++);
}

std::string IRGenerator::createLabel() {
  return "L" + std::to_string(labelCounter++);
}

std::string IRGenerator::emitAlloca(const std::string& type, const std::string& name) {
  std::string temp = name.empty() ? createTemp() : name;
  irStream << "  %" << temp << " = alloca " << type << "\n";
  return "%" + temp;
}

std::string IRGenerator::emitLoad(const std::string& type, const std::string& ptr) {
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << type << ", " << type << "* " << ptr << "\n";
  return "%" + temp;
}

std::string IRGenerator::emitStore(const std::string& value, const std::string& ptr) {
  irStream << "  store i32 " << value << ", i32* " << ptr << "\n";
  return "";
}

std::string IRGenerator::emitBinaryOp(const std::string& op, const std::string& lhs, const std::string& rhs, const std::string& type) {
  std::string temp = createTemp();
  irStream << "  %" << temp << " = " << op << " " << type << " " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

void IRGenerator::error(const std::string& msg) {
  std::cerr << "IR Generation Error: " << msg << std::endl;
  throw std::runtime_error(msg);
}

std::string IRGenerator::getCurrentIR() {
  return irStream.str();
}

void IRGenerator::preScan(const std::vector<std::unique_ptr<ASTNode>>& ast) {
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (auto* const_ = dynamic_cast<ConstantItemNode*>(item)) {
        visit(const_);
      }
    }
  }
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (auto* func = dynamic_cast<FunctionNode*>(item)) {
        // 记录函数原型
        std::string retType = func->return_type ? toIRType(func->return_type->type.get()) : "void";
        //irStream << "; return type of function: " << func->identifier << " : " << retType << '\n';
        functionTable[func->identifier] = retType;
      } else if (auto* struct_ = dynamic_cast<StructStructNode*>(item)) {
        // 记录 struct 字段
        //irStream << "; recording elements of struct " << struct_->identifier << ": \n";
        std::vector<std::pair<std::string, std::string>> fields;
        if (struct_->struct_fields) {
          size_t index = 0;
          for (const auto& field : struct_->struct_fields->struct_fields) {
            std::string fieldType = toIRType(field->type.get());
            //irStream << "; the " << index << "th element in the struct: " << fieldType << '\n';
            index++;
            fields.emplace_back(field->identifier, fieldType);
          }
        }
        structFields[struct_->identifier] = fields;
        typeTable[struct_->identifier] = "%" + struct_->identifier;
      }
    }
  }
}

void IRGenerator::generateStructTypes() {
  for (const auto& [name, fields] : structFields) {
    irStream << "%" << name << " = type { ";
    for (size_t i = 0; i < fields.size(); ++i) {
      irStream << fields[i].second;
      if (i + 1 < fields.size()) irStream << ", ";
    }
    irStream << " }\n";
  }
  if (!structFields.empty()) irStream << "\n";
}

void IRGenerator::enterScope() {
  symbolScopes.push_back({});
  varTypeScopes.push_back({});
}

void IRGenerator::exitScope() {
  if (!symbolScopes.empty()) symbolScopes.pop_back();
  if (!varTypeScopes.empty()) varTypeScopes.pop_back();
}

std::string IRGenerator::lookupSymbol(const std::string& name) {
  for (auto it = symbolScopes.rbegin(); it != symbolScopes.rend(); ++it) {
    if (it->find(name) != it->end()) return (*it)[name];
  }
  return "";
}

std::string IRGenerator::lookupVarType(const std::string& name) {
  for (auto it = varTypeScopes.rbegin(); it != varTypeScopes.rend(); ++it) {
    if (it->find(name) != it->end()) return (*it)[name];
  }
  return "";
}

std::string IRGenerator::getLhsAddress(ExpressionNode* lhs) {
  if (auto* path = dynamic_cast<PathExpressionNode*>(lhs)) {
    std::string name = path->toString();
    return "%" + lookupSymbol(name);
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(lhs)) {
    // *expr, expr 应该是指针，地址是 expr 的值
    return visit(deref->expression.get());
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(lhs)) {
    // expr.field, 生成 getelementptr
    std::string baseAddr = getLhsAddress(field->expression.get());
    std::string baseType = getLhsType(field->expression.get());
    size_t starPos = baseType.find('*');
    if (starPos == std::string::npos) {
      // base is struct value, need to alloca temporary
      std::string tempPtr = createTemp();
      irStream << "  %" << tempPtr << " = alloca " << baseType << "\n";
      irStream << "  store " << baseType << " " << baseAddr << ", " << baseType << "* %" << tempPtr << "\n";
      baseAddr = "%" + tempPtr;
      baseType += "*";
      starPos = baseType.size() - 1;
    }
    std::string structName = baseType.substr(1, starPos - 1);
    auto it = structFields.find(structName);
    if (it == structFields.end()) {
      error("Unknown struct " + structName);
      return "";
    }
    int index = -1;
    for (int i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == field->identifier.id) {
        index = i;
        break;
      }
    }
    if (index == -1) {
      error("Field not found " + field->identifier.id);
      return "";
    }
    std::string fieldPtr = createTemp();
    irStream << "  %" << fieldPtr << " = getelementptr " << "%" + structName << ", " << "%" + structName << "* " << baseAddr << ", i32 0, i32 " << index << "\n";
    return "%" + fieldPtr;
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(lhs)) {
    // base[index], 生成 getelementptr
    std::string baseAddr = getLhsAddress(index->base.get());
    std::string indexValue = visit(index->index.get());
    std::string baseType = getLhsType(index->base.get());
    std::string elementType = getElementType(baseType);
    if (elementType == baseType) {
      // not array or pointer, assume pointer
      if (baseType.back() == '*') {
        elementType = baseType.substr(0, baseType.size() - 1);
      } else {
        error("Index on non-array non-pointer type");
        return "";
      }
    }
    std::string ptrTemp = createTemp();
    if (baseType.find('[') == 0) {
      // array type
      irStream << "  %" << ptrTemp << " = getelementptr " << baseType << ", " << baseType << "* " << baseAddr << ", i32 0, i32 " << indexValue << "\n";
    } else {
      // pointer
      irStream << "  %" << ptrTemp << " = getelementptr " << elementType << ", " << elementType << "* " << baseAddr << ", i32 " << indexValue << "\n";
    }
    return "%" + ptrTemp;
  } else {
    error("Unsupported lhs type in assignment");
    return "";
  }
}

std::string IRGenerator::getElementType(const std::string& typeStr) {
  bool isPtr = false;
  std::string base = typeStr;
  if (!base.empty() && base.back() == '*') {
    isPtr = true;
    base = base.substr(0, base.size() - 1);
  }
  std::string elem;
  if (base.size() > 2 && base[0] == '[') {
    // [num x type]
    size_t xPos = base.find(" x ");
    if (xPos != std::string::npos) {
      elem = base.substr(xPos + 3, base.size() - xPos - 4); // remove ]
    } else {
      elem = base;
    }
  } else {
    elem = base;
  }
  if (isPtr) {
    elem += '*';
  }
  return elem;
}

std::string IRGenerator::getLhsType(ExpressionNode* lhs) {
  if (auto* path = dynamic_cast<PathExpressionNode*>(lhs)) {
    std::string name = path->toString();
    return lookupVarType(name);
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(lhs)) {
    // *expr, 类型是 expr 类型去掉 *
    std::string exprType = getLhsType(deref->expression.get());
    if (exprType.back() == '*') {
      return exprType.substr(0, exprType.size() - 1);
    } else {
      error("Dereference on non-pointer type");
      return "";
    }
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(lhs)) {
    std::string baseType = getLhsType(field->expression.get());
    size_t starPos = baseType.find('*');
    std::string structName;
    if (starPos != std::string::npos) {
      structName = baseType.substr(1, starPos - 1);
    } else {
      structName = baseType.substr(1);
    }
    auto it = structFields.find(structName);
    if (it == structFields.end()) {
      error("Unknown struct " + structName);
      return "";
    }
    int index = -1;
    for (int i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == field->identifier.id) {
        index = i;
        break;
      }
    }
    if (index == -1) {
      error("Field not found " + field->identifier.id);
      return "";
    }
    return it->second[index].second;
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(lhs)) {
    // base[index], 类型是 base 的元素类型
    //irStream << "; getting type of index expression\n";
    std::string baseType = getLhsType(index->base.get());
    //irStream << "; base type in index expression: " << baseType << "\n";
    std::string elementType = getElementType(baseType);
    if (elementType != baseType) {
      // 是数组
      return elementType;
    } else if (baseType.back() == '*') {
      // 是指针
      return baseType.substr(0, baseType.size() - 1);
    } else {
      error("Index on non-array non-pointer type");
      return "";
    }
  } else {
    error("Unsupported lhs type in assignment");
    return "";
  }
}

std::optional<int> IRGenerator::evaluateConstant(ExpressionNode* expr) {
  if (auto* lit = dynamic_cast<LiteralExpressionNode*>(expr)) {
    if (std::holds_alternative<std::unique_ptr<integer_literal>>(lit->literal)) {
      return std::stoi(std::get<std::unique_ptr<integer_literal>>(lit->literal)->value);
    }
  } else if (auto* path = dynamic_cast<PathExpressionNode*>(expr)) {
    std::string name = path->toString();
    if (constantTable.find(name) != constantTable.end()) {
      return std::stoi(constantTable[name]);
    } else {
      irStream << "; constant: " << name << " not found\n";
    }
  } else if (auto* arith = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr)) {
    auto lhs = evaluateConstant(arith->expression1.get());
    auto rhs = evaluateConstant(arith->expression2.get());
    if (lhs && rhs) {
      switch (arith->type) {
        case ADD: return *lhs + *rhs;
        case MINUS: return *lhs - *rhs;
        case MUL: return *lhs * *rhs;
        case DIV: return *lhs / *rhs;
        case MOD: return *lhs % *rhs;
        default: break;
      }
    }
  } else {
    irStream << "Unknow type of expression in evaluating constant" << typeid(*expr).name() << '\n';
    error(std::string("Unknow type of expression in evaluating constant") + typeid(*expr).name());
  }
  return std::nullopt;
}

#endif